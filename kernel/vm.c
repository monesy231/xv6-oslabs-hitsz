#include "param.h"
#include "types.h"
#include "memlayout.h"
#include "elf.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"

/*
 * the kernel's page table.
 */
pagetable_t kernel_pagetable;

extern char etext[];  // kernel.ld sets this to end of kernel code.

extern char trampoline[];  // trampoline.S

/*
 * create a direct-map page table for the kernel.
 */
void kvminit() {
  kernel_pagetable = (pagetable_t)kalloc();
  memset(kernel_pagetable, 0, PGSIZE);

  // uart registers
  kvmmap(UART0, UART0, PGSIZE, PTE_R | PTE_W);

  // virtio mmio disk interface
  kvmmap(VIRTIO0, VIRTIO0, PGSIZE, PTE_R | PTE_W);

  // CLINT
  kvmmap(CLINT, CLINT, 0x10000, PTE_R | PTE_W);

  // PLIC
  kvmmap(PLIC, PLIC, 0x400000, PTE_R | PTE_W);

  // map kernel text executable and read-only.
  kvmmap(KERNBASE, KERNBASE, (uint64)etext - KERNBASE, PTE_R | PTE_X);

  // map kernel data and the physical RAM we'll make use of.
  kvmmap((uint64)etext, (uint64)etext, PHYSTOP - (uint64)etext, PTE_R | PTE_W);

  // map the trampoline for trap entry/exit to
  // the highest virtual address in the kernel.
  kvmmap(TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_R | PTE_X);
}

// 从 L2 递归到 L0，只打印有效项。
static void vmprint_level(pagetable_t pagetable, int level, uint64 prefix) {
  for (int i = 0; i < 512; i++) {
    pte_t pte = pagetable[i];
    if ((pte & PTE_V) == 0) continue;
    // 每级索引占 9 位，拼接时保留上层地址。
    uint64 va = prefix | ((uint64)i << PXSHIFT(level));
    uint64 pa = PTE2PA(pte);
    // 内核 printf 不支持 %c，用字符串输出 rwxu 权限。
    char flags[] = "----";
    if (pte & PTE_R) flags[0] = 'r';
    if (pte & PTE_W) flags[1] = 'w';
    if (pte & PTE_X) flags[2] = 'x';
    if (pte & PTE_U) flags[3] = 'u';
    for (int depth = 2; depth > level; depth--) printf("||   ");
    printf("||idx: %d: ", i);
    // 有 R/W/X 权限的是叶子项，否则继续下探。
    if (pte & (PTE_R | PTE_W | PTE_X)) {
      printf("va: %p -> pa: %p, flags: %s\n", va, pa, flags);
    } else {
      printf("pa: %p, flags: %s\n", pa, flags);
      if (level > 0) vmprint_level((pagetable_t)pa, level - 1, va);
    }
  }
}

void vmprint(pagetable_t pagetable) {
  printf("page table %p\n", pagetable);
  vmprint_level(pagetable, 2, 0);
}

// 只释放页表页，不释放叶子映射指向的物理页。
static void kvmfreewalk(pagetable_t pagetable) {
  for (int i = 0; i < 512; i++) {
    pte_t pte = pagetable[i];
    if ((pte & PTE_V) && (pte & (PTE_R | PTE_W | PTE_X)) == 0)
      kvmfreewalk((pagetable_t)PTE2PA(pte));
    pagetable[i] = 0;
  }
  kfree((void *)pagetable);
}

// 先断开共享 L0 表的引用，避免随内核页表重复释放。
void kvmfree(pagetable_t pagetable) {
  if (pagetable[0] & PTE_V) {
    pagetable_t l1 = (pagetable_t)PTE2PA(pagetable[0]);
    for (int i = 0; i < PX(1, PLIC); i++) l1[i] = 0;
  }
  kvmfreewalk(pagetable);
}

// 新建私有内核页表；省去 CLINT 映射，给用户低地址区留空间。
pagetable_t kvmcreate(void) {
  pagetable_t pagetable = uvmcreate();
  if (pagetable == 0) return 0;
  if (mappages(pagetable, UART0, PGSIZE, UART0, PTE_R | PTE_W) < 0 ||
      mappages(pagetable, VIRTIO0, PGSIZE, VIRTIO0, PTE_R | PTE_W) < 0 ||
      mappages(pagetable, PLIC, 0x400000, PLIC, PTE_R | PTE_W) < 0 ||
      mappages(pagetable, KERNBASE, (uint64)etext - KERNBASE, KERNBASE,
               PTE_R | PTE_X) < 0 ||
      mappages(pagetable, (uint64)etext, PHYSTOP - (uint64)etext, (uint64)etext,
               PTE_R | PTE_W) < 0 ||
      mappages(pagetable, TRAMPOLINE, PGSIZE, (uint64)trampoline, PTE_R | PTE_X) < 0) {
    kvmfree(pagetable);
    return 0;
  }
  return pagetable;
}

// 设备映射已建好内核 L1 表，这里无需分配。
// 192 MiB / 2 MiB = 96：共享 L0 表，空项也要同步以清除旧引用。
void sync_pagetable(pagetable_t user, pagetable_t kernel) {
  pagetable_t dst = (pagetable_t)PTE2PA(kernel[0]);
  pagetable_t src = (user[0] & PTE_V) ? (pagetable_t)PTE2PA(user[0]) : 0;
  for (int i = 0; i < PX(1, PLIC); i++) dst[i] = src ? src[i] : 0;
  sfence_vma();
}

// Switch h/w page table register to the kernel's page table,
// and enable paging.
void kvminithart() {
  w_satp(MAKE_SATP(kernel_pagetable));
  sfence_vma();
}

// Return the address of the PTE in page table pagetable
// that corresponds to virtual address va.  If alloc!=0,
// create any required page-table pages.
//
// The risc-v Sv39 scheme has three levels of page-table
// pages. A page-table page contains 512 64-bit PTEs.
// A 64-bit virtual address is split into five fields:
//   39..63 -- must be zero.
//   30..38 -- 9 bits of level-2 index.
//   21..29 -- 9 bits of level-1 index.
//   12..20 -- 9 bits of level-0 index.
//    0..11 -- 12 bits of byte offset within the page.
pte_t *walk(pagetable_t pagetable, uint64 va, int alloc) {
  if (va >= MAXVA) panic("walk");

  for (int level = 2; level > 0; level--) {
    pte_t *pte = &pagetable[PX(level, va)];
    if (*pte & PTE_V) {
      pagetable = (pagetable_t)PTE2PA(*pte);
    } else {
      if (!alloc || (pagetable = (pde_t *)kalloc()) == 0) return 0;
      memset(pagetable, 0, PGSIZE);
      *pte = PA2PTE(pagetable) | PTE_V;
    }
  }
  return &pagetable[PX(0, va)];
}

// Look up a virtual address, return the physical address,
// or 0 if not mapped.
// Can only be used to look up user pages.
uint64 walkaddr(pagetable_t pagetable, uint64 va) {
  pte_t *pte;
  uint64 pa;

  if (va >= MAXVA) return 0;

  pte = walk(pagetable, va, 0);
  if (pte == 0) return 0;
  if ((*pte & PTE_V) == 0) return 0;
  if ((*pte & PTE_U) == 0) return 0;
  pa = PTE2PA(*pte);
  return pa;
}

// add a mapping to the kernel page table.
// only used when booting.
// does not flush TLB or enable paging.
void kvmmap(uint64 va, uint64 pa, uint64 sz, int perm) {
  if (mappages(kernel_pagetable, va, sz, pa, perm) != 0) panic("kvmmap");
}

// translate a kernel virtual address to
// a physical address. only needed for
// addresses on the stack.
// assumes va is page aligned.
uint64 kvmpa(uint64 va) {
  uint64 off = va % PGSIZE;
  pte_t *pte;
  uint64 pa;

  pte = walk(kernel_pagetable, va, 0);
  if (pte == 0) panic("kvmpa");
  if ((*pte & PTE_V) == 0) panic("kvmpa");
  pa = PTE2PA(*pte);
  return pa + off;
}

// Create PTEs for virtual addresses starting at va that refer to
// physical addresses starting at pa. va and size might not
// be page-aligned. Returns 0 on success, -1 if walk() couldn't
// allocate a needed page-table page.
int mappages(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, int perm) {
  uint64 a, last;
  pte_t *pte;

  a = PGROUNDDOWN(va);
  last = PGROUNDDOWN(va + size - 1);
  for (;;) {
    if ((pte = walk(pagetable, a, 1)) == 0) return -1;
    if (*pte & PTE_V) panic("remap");
    *pte = PA2PTE(pa) | perm | PTE_V;
    if (a == last) break;
    a += PGSIZE;
    pa += PGSIZE;
  }
  return 0;
}

// Remove npages of mappings starting from va. va must be
// page-aligned. The mappings must exist.
// Optionally free the physical memory.
void uvmunmap(pagetable_t pagetable, uint64 va, uint64 npages, int do_free) {
  uint64 a;
  pte_t *pte;

  if ((va % PGSIZE) != 0) panic("uvmunmap: not aligned");

  for (a = va; a < va + npages * PGSIZE; a += PGSIZE) {
    if ((pte = walk(pagetable, a, 0)) == 0) panic("uvmunmap: walk");
    if ((*pte & PTE_V) == 0) panic("uvmunmap: not mapped");
    if (PTE_FLAGS(*pte) == PTE_V) panic("uvmunmap: not a leaf");
    if (do_free) {
      uint64 pa = PTE2PA(*pte);
      kfree((void *)pa);
    }
    *pte = 0;
  }
}

// create an empty user page table.
// returns 0 if out of memory.
pagetable_t uvmcreate() {
  pagetable_t pagetable;
  pagetable = (pagetable_t)kalloc();
  if (pagetable == 0) return 0;
  memset(pagetable, 0, PGSIZE);
  return pagetable;
}

// Load the user initcode into address 0 of pagetable,
// for the very first process.
// sz must be less than a page.
void uvminit(pagetable_t pagetable, uchar *src, uint sz) {
  char *mem;

  if (sz >= PGSIZE) panic("inituvm: more than a page");
  mem = kalloc();
  memset(mem, 0, PGSIZE);
  mappages(pagetable, 0, PGSIZE, (uint64)mem, PTE_W | PTE_R | PTE_X | PTE_U);
  memmove(mem, src, sz);
}

// Allocate PTEs and physical memory to grow process from oldsz to
// newsz, which need not be page aligned.  Returns new size or 0 on error.
uint64 uvmalloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz) {
  char *mem;
  uint64 a;

  // 用户地址不能进入 PLIC 设备映射区。
  if (newsz > PLIC) return 0;
  if (newsz < oldsz) return oldsz;

  oldsz = PGROUNDUP(oldsz);
  for (a = oldsz; a < newsz; a += PGSIZE) {
    mem = kalloc();
    if (mem == 0) {
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
    memset(mem, 0, PGSIZE);
    if (mappages(pagetable, a, PGSIZE, (uint64)mem, PTE_W | PTE_X | PTE_R | PTE_U) != 0) {
      kfree(mem);
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
  }
  return newsz;
}

// Deallocate user pages to bring the process size from oldsz to
// newsz.  oldsz and newsz need not be page-aligned, nor does newsz
// need to be less than oldsz.  oldsz can be larger than the actual
// process size.  Returns the new process size.
uint64 uvmdealloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz) {
  if (newsz >= oldsz) return oldsz;

  if (PGROUNDUP(newsz) < PGROUNDUP(oldsz)) {
    int npages = (PGROUNDUP(oldsz) - PGROUNDUP(newsz)) / PGSIZE;
    uvmunmap(pagetable, PGROUNDUP(newsz), npages, 1);
  }

  return newsz;
}

// Recursively free page-table pages.
// All leaf mappings must already have been removed.
void freewalk(pagetable_t pagetable) {
  // there are 2^9 = 512 PTEs in a page table.
  for (int i = 0; i < 512; i++) {
    pte_t pte = pagetable[i];
    if ((pte & PTE_V) && (pte & (PTE_R | PTE_W | PTE_X)) == 0) {
      // this PTE points to a lower-level page table.
      uint64 child = PTE2PA(pte);
      freewalk((pagetable_t)child);
      pagetable[i] = 0;
    } else if (pte & PTE_V) {
      panic("freewalk: leaf");
    }
  }
  kfree((void *)pagetable);
}

// Free user memory pages,
// then free page-table pages.
void uvmfree(pagetable_t pagetable, uint64 sz) {
  if (sz > 0) uvmunmap(pagetable, 0, PGROUNDUP(sz) / PGSIZE, 1);
  freewalk(pagetable);
}

// Given a parent process's page table, copy
// its memory into a child's page table.
// Copies both the page table and the
// physical memory.
// returns 0 on success, -1 on failure.
// frees any allocated pages on failure.
int uvmcopy(pagetable_t old, pagetable_t new, uint64 sz) {
  pte_t *pte;
  uint64 pa, i;
  uint flags;
  char *mem;

  for (i = 0; i < sz; i += PGSIZE) {
    if ((pte = walk(old, i, 0)) == 0) panic("uvmcopy: pte should exist");
    if ((*pte & PTE_V) == 0) panic("uvmcopy: page not present");
    pa = PTE2PA(*pte);
    flags = PTE_FLAGS(*pte);
    if ((mem = kalloc()) == 0) goto err;
    memmove(mem, (char *)pa, PGSIZE);
    if (mappages(new, i, PGSIZE, (uint64)mem, flags) != 0) {
      kfree(mem);
      goto err;
    }
  }
  return 0;

err:
  uvmunmap(new, 0, i / PGSIZE, 1);
  return -1;
}

// mark a PTE invalid for user access.
// used by exec for the user stack guard page.
void uvmclear(pagetable_t pagetable, uint64 va) {
  pte_t *pte;

  pte = walk(pagetable, va, 0);
  if (pte == 0) panic("uvmclear");
  *pte &= ~PTE_U;
}

// Copy from kernel to user.
// Copy len bytes from src to virtual address dstva in a given page table.
// Return 0 on success, -1 on error.
int copyout(pagetable_t pagetable, uint64 dstva, char *src, uint64 len) {
  uint64 n, va0, pa0;

  while (len > 0) {
    va0 = PGROUNDDOWN(dstva);
    pa0 = walkaddr(pagetable, va0);
    if (pa0 == 0) return -1;
    n = PGSIZE - (dstva - va0);
    if (n > len) n = len;
    memmove((void *)(pa0 + (dstva - va0)), src, n);

    len -= n;
    src += n;
    dstva = va0 + PGSIZE;
  }
  return 0;
}

// 遍历页表检查 V/U/R 权限，实际拷贝仍用硬件地址翻译。
// 返回连续可读长度，允许字符串在保护页前正常结束。
static uint64 user_readable(pagetable_t pagetable, uint64 va, uint64 len) {
  uint64 done = 0;
  if (va >= PLIC) return 0;
  if (len > PLIC - va) len = PLIC - va;
  while (done < len) {
    pte_t *pte = walk(pagetable, va + done, 0);
    int perm = PTE_V | PTE_U | PTE_R;
    if (pte == 0 || (*pte & perm) != perm) break;
    uint64 n = PGSIZE - ((va + done) % PGSIZE);
    if (n > len - done) n = len - done;
    done += n;
  }
  return done;
}

// 定长拷贝要求整段可读，不能借内核映射绕过用户保护页。
int copyin(pagetable_t pagetable, char *dst, uint64 srcva, uint64 len) {
  if (user_readable(pagetable, srcva, len) != len) return -1;
  // 避免 SUM 开启期间发生进程切换。
  push_off();
  uint64 sum = r_sstatus() & SSTATUS_SUM;
  w_sstatus(r_sstatus() | SSTATUS_SUM);
  int result = copyin_new(pagetable, dst, srcva, len);
  // 恢复原 SUM 状态，不一直开放用户页访问。
  w_sstatus((r_sstatus() & ~SSTATUS_SUM) | sum);
  pop_off();
  return result;
}

// 缩短读取上限，保留保护页前的合法字符串；新函数仍只调用一次。
int copyinstr(pagetable_t pagetable, char *dst, uint64 srcva, uint64 max) {
  max = user_readable(pagetable, srcva, max);
  push_off();
  uint64 sum = r_sstatus() & SSTATUS_SUM;
  w_sstatus(r_sstatus() | SSTATUS_SUM);
  int result = copyinstr_new(pagetable, dst, srcva, max);
  w_sstatus((r_sstatus() & ~SSTATUS_SUM) | sum);
  pop_off();
  return result;
}

// check if use global kpgtbl or not
int test_pagetable() {
  uint64 satp = r_satp();
  uint64 gsatp = MAKE_SATP(kernel_pagetable);
  printf("test_pagetable: %d\n", satp != gsatp);
  return satp != gsatp;
}