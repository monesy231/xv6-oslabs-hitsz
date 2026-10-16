// Buffer cache.
//
// The buffer cache uses hash buckets to hold cached copies of disk
// block contents. Caching disk blocks in memory reduces disk reads
// and provides a synchronization point for shared disk blocks.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.

#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

// 按块号分到 31 个桶以减少冲突，缓冲块总数仍为 NBUF。
#define NBUCKETS 31

struct bucket {
  // 保护桶内链表、块身份和引用计数；块内容由睡眠锁保护。
  struct spinlock lock;
  struct buf *head;
};

struct {
  struct bucket bucket[NBUCKETS];
  struct buf buf[NBUF];

  // 仅未命中时拿 evict，保护空闲池、游标及块身份的更换。
  // 加锁顺序：先 evict，再桶锁；需要全部桶时按编号递增。
  struct spinlock evict;
  struct buf *free;
  uint cursor;
} bcache;

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.evict, "bcache.evict");
  for(int i = 0; i < NBUCKETS; i++)
    initlock(&bcache.bucket[i].lock, "bcache.bucket");

  // 尚未分配的块放在独立空闲池，避免初始的 (0, 0) 被误判为命中。
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    initsleeplock(&b->lock, "buffer");
    b->next = bcache.free;
    bcache.free = b;
  }
}

// 调用者持有桶锁；匹配设备号和块号后增加引用，防止等待期间被回收。
static struct buf*
blookup(struct bucket *bucket, uint dev, uint blockno)
{
  struct buf *b;

  for(b = bucket->head; b; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      return b;
    }
  }
  return 0;
}

// 在桶锁内摘下引用为 0 的块，并预留一个引用；双向链表可直接摘除。
static void
bremove(struct bucket *bucket, struct buf *b)
{
  if(b->prev)
    b->prev->next = b->next;
  else
    bucket->head = b->next;
  if(b->next)
    b->next->prev = b->prev;
  b->refcnt = 1;
}

// 调用者持有 evict；返回一个已预留、尚未加入目标桶的缓冲块。
static struct buf*
brecycle(void)
{
  struct buf *b;
  struct bucket *bucket;

  if(bcache.free){
    b = bcache.free;
    bcache.free = b->next;
    b->refcnt = 1;
    return b;
  }

  // evict 保证块身份不变，桶锁保护引用计数；按环形游标找空闲块。
  for(int i = 0; i < NBUF; i++){
    b = &bcache.buf[bcache.cursor];
    bcache.cursor = (bcache.cursor + 1) % NBUF;
    bucket = &bcache.bucket[b->blockno % NBUCKETS];
    acquire(&bucket->lock);
    if(b->refcnt == 0){
      bremove(bucket, b);
      release(&bucket->lock);
      return b;
    }
    release(&bucket->lock);
  }

  // 扫过的块可能刚被释放，锁住全部桶再复查，避免误报耗尽。
  // 按编号递增加锁、逆序释放，保持统一顺序。
  for(int i = 0; i < NBUCKETS; i++)
    acquire(&bcache.bucket[i].lock);
  b = 0;
  for(int i = 0; i < NBUF; i++){
    struct buf *candidate = &bcache.buf[bcache.cursor];
    bcache.cursor = (bcache.cursor + 1) % NBUF;
    if(candidate->refcnt == 0){
      b = candidate;
      bucket = &bcache.bucket[b->blockno % NBUCKETS];
      bremove(bucket, b);
      break;
    }
  }
  for(int i = NBUCKETS - 1; i >= 0; i--)
    release(&bcache.bucket[i].lock);
  if(b)
    return b;
  panic("bget: no buffers");
}

// 查找或分配唯一的 (dev, blockno) 缓存，返回时持有块的睡眠锁。
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  struct bucket *bucket = &bcache.bucket[blockno % NBUCKETS];

  acquire(&bucket->lock);
  b = blookup(bucket, dev, blockno);
  release(&bucket->lock);
  if(b){
    // 已增加引用，块不会被回收；等待睡眠锁前必须释放自旋锁。
    acquiresleep(&b->lock);
    return b;
  }

  // 等待 evict 时可能已有其他 CPU 插入该块，重查以避免重复副本。
  acquire(&bcache.evict);
  acquire(&bucket->lock);
  b = blookup(bucket, dev, blockno);
  release(&bucket->lock);
  if(b){
    release(&bcache.evict);
    acquiresleep(&b->lock);
    return b;
  }

  b = brecycle();
  acquire(&bucket->lock);
  b->dev = dev;
  b->blockno = blockno;
  // 更换块身份后旧数据失效，交给 bread 重新读盘。
  b->valid = 0;
  b->prev = 0;
  b->next = bucket->head;
  if(b->next)
    b->next->prev = b;
  bucket->head = b;
  release(&bucket->lock);
  release(&bcache.evict);

  acquiresleep(&b->lock);
  return b;
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// 先释放睡眠锁，再在桶锁内减引用；减引用前块身份不会改变。
void
brelse(struct buf *b)
{
  struct bucket *bucket = &bcache.bucket[b->blockno % NBUCKETS];

  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  acquire(&bucket->lock);
  b->refcnt--;
  release(&bucket->lock);
}

// 调用者已有引用，所属桶不会改变；增加引用可防止日志提交前被回收。
void
bpin(struct buf *b)
{
  struct bucket *bucket = &bcache.bucket[b->blockno % NBUCKETS];

  acquire(&bucket->lock);
  b->refcnt++;
  release(&bucket->lock);
}

// 解除日志固定；最后一个引用释放后，该块才可被回收。
void
bunpin(struct buf *b)
{
  struct bucket *bucket = &bcache.bucket[b->blockno % NBUCKETS];

  acquire(&bucket->lock);
  b->refcnt--;
  release(&bucket->lock);
}
