// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
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
extern uint ticks;

#define NBUCKET 13
int hash(uint blockno) {
  return blockno % NBUCKET;
}

struct {
  struct spinlock bhash_lk[NBUCKET];
  struct buf bhash_head[NBUCKET];
  struct buf buf[NBUF];
  struct spinlock evict_lk;
  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
} bcache;

void
binit(void)
{
  struct buf *b;

  for (int i = 0; i < NBUCKET; i++) {
    initlock(&bcache.bhash_lk[i], "bcache bucket lk");
    bcache.bhash_head[i].prev = &bcache.bhash_head[i];
    bcache.bhash_head[i].next = &bcache.bhash_head[i];
  }
  for (b = bcache.buf; b < bcache.buf + NBUF; b++) {
    b->last_used = 0;
    b->refcnt = 0;
    b->next = bcache.bhash_head[0].next;
    b->prev = &bcache.bhash_head[0];
    initsleeplock(&b->lock, "buffer");
    bcache.bhash_head[0].next->prev = b;
    bcache.bhash_head[0].next = b;
  }
  initlock(&bcache.evict_lk, "bcache evict lk");
}

// search all buckets for LRU block, and modify the blubkt, 
// return pointer to the block with bucket's lock
struct buf*
bfindLRU(int *lrubkt) {
  *lrubkt = -1;
  struct buf *bLRU = 0;
  struct buf *b;
  uint min_time = 1 << 31;
  for (int i = 0; i < NBUCKET; i++) {
    acquire(&bcache.bhash_lk[i]);
    for (b = bcache.bhash_head[i].next; b != &bcache.bhash_head[i]; b = b->next) {
      if (b->refcnt == 0 && b->last_used < min_time) {
        if (*lrubkt >= 0 && *lrubkt != i)
          release(&bcache.bhash_lk[*lrubkt]);
        bLRU = b;
        min_time = b->last_used;
        *lrubkt = i;
      }
    }
    if (*lrubkt != i)
      release(&bcache.bhash_lk[i]);
  }
  return bLRU;
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  uint key = hash(blockno);
  acquire(&bcache.bhash_lk[key]);

  // Is the block already cached?
  for(b = bcache.bhash_head[key].next; b != &bcache.bhash_head[key]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.bhash_lk[key]);
      acquiresleep(&b->lock);
      return b;
    }
  }
  // release the key lock
  release(&bcache.bhash_lk[key]);
  // not cached, acquire evict lock and check whether the block has been cached.
  acquire(&bcache.evict_lk);
  for(b = bcache.bhash_head[key].next; b != &bcache.bhash_head[key]; b = b->next) {
    acquire(&bcache.bhash_lk[key]);
    if(b->dev == dev && b->blockno == blockno){
      // has been cached by other cpu
      // because have acquired the evict lock, so the linked list is valid (won't be added by other threads)
      b->refcnt++;
      release(&bcache.bhash_lk[key]);
      release(&bcache.evict_lk);
      acquiresleep(&b->lock);
      return b;
    }
    release(&bcache.bhash_lk[key]);
  }
  // really hasn't been cached, now start to find LRU
  int lrubkt;
  b = bfindLRU(&lrubkt);
  // return the LRU with that bucket's lock
  if (lrubkt < 0 || b == 0)
    panic("bget: no buffers");
  // remove the LRU block from the previosu list
  b->prev->next = b->next;
  b->next->prev = b->prev;
  if (lrubkt != key) {
    // if the original bucket != key
    // updated the list and release the lock
    release(&bcache.bhash_lk[lrubkt]);
    // re-require the key lock
    acquire(&bcache.bhash_lk[key]);
  }
  b->next = bcache.bhash_head[key].next;
  b->prev = &bcache.bhash_head[key];
  bcache.bhash_head[key].next->prev = b;
  bcache.bhash_head[key].next = b;
  b->dev = dev;
  b->blockno = blockno;
  b->refcnt = 1;
  b->valid = 0;
  release(&bcache.bhash_lk[key]);
  release(&bcache.evict_lk);
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

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");
  releasesleep(&b->lock);
  uint key = hash(b->blockno);
  acquire(&bcache.bhash_lk[key]);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->last_used = ticks;
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.bhash_head[key].next;
    b->prev = &bcache.bhash_head[key];
    bcache.bhash_head[key].next->prev = b;
    bcache.bhash_head[key].next = b;
  }
  release(&bcache.bhash_lk[key]);
}

void
bpin(struct buf *b) {
  uint key = hash(b->blockno);
  acquire(&bcache.bhash_lk[key]);
  b->refcnt++;
  release(&bcache.bhash_lk[key]);
}

void
bunpin(struct buf *b) {
  uint key = hash(b->blockno);
  acquire(&bcache.bhash_lk[key]);
  b->refcnt--;
  release(&bcache.bhash_lk[key]);
}


