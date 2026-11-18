// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

// 复用空闲页的页首保存 next，不额外分配链表节点。
struct run {
  struct run *next;
};

// 每 CPU 一条空闲链表、一把锁，减少不同 CPU 之间的争用。
struct {
  struct spinlock lock;
  struct run *freelist;
} kmem[NCPU];

// 一次最多借 32 页，减少频繁借页，也限制持锁时间。
#define STEAL_BATCH 32

void
kinit()
{
  // kmem 前缀供 kalloctest 汇总锁争用次数。
  for(int i = 0; i < NCPU; i++)
    initlock(&kmem[i].lock, "kmem");
  freerange(end, (void*)PHYSTOP);

  // 其他 CPU 仍在 main() 等待 started，此时可直接重分空闲页。
  // 轮流放入 NCPU 条链表，避免启动时都向同一 CPU 借页。
  push_off();
  int id = cpuid();
  struct run *r = kmem[id].freelist;
  // 先取下整条链表再分散，避免页面同时挂在两条链表上。
  kmem[id].freelist = 0;
  for(int target = 0; r; target = (target + 1) % NCPU){
    struct run *next = r->next;
    r->next = kmem[target].freelist;
    kmem[target].freelist = r;
    r = next;
  }
  pop_off();
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  // 关中断固定 CPU 编号；跨 CPU 的链表访问仍靠自旋锁互斥。
  push_off();
  int id = cpuid();
  acquire(&kmem[id].lock);
  // 归还当前 CPU 即可，不必回到最初分配它的 CPU。
  r->next = kmem[id].freelist;
  kmem[id].freelist = r;
  release(&kmem[id].lock);
  pop_off();
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  // 优先取本地空闲页，常见路径只需本地锁。
  push_off();
  int id = cpuid();
  acquire(&kmem[id].lock);
  r = kmem[id].freelist;
  if(r)
    kmem[id].freelist = r->next;
  release(&kmem[id].lock);

  // 本地为空才向其他 CPU 借页，此时已释放本地锁。
  if(r == 0){
    // 所有其他链表都要检查，未启用 CPU 的空闲页也能借用。
    for(int offset = 1; offset < NCPU; offset++){
      int donor = (id + offset) % NCPU;
      acquire(&kmem[donor].lock);
      r = kmem[donor].freelist;
      struct run *last = r;
      if(r){
        for(int count = 1; count < STEAL_BATCH && last->next; count++)
          last = last->next;
        // 在来源锁内整段摘下，防止同一页被再次分配。
        kmem[donor].freelist = last->next;
        last->next = 0;
      }
      release(&kmem[donor].lock);

      if(r){
        // 摘下的页已独占：第一张返回，其余挂回本地。
        // 先放来源锁再拿本地锁，避免相互借页时死锁。
        if(r->next){
          acquire(&kmem[id].lock);
          last->next = kmem[id].freelist;
          kmem[id].freelist = r->next;
          release(&kmem[id].lock);
        }
        break;
      }
    }
  }
  // 取页和借页都结束后再恢复中断状态，保证途中 CPU 编号不变。
  pop_off();

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
