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

#define NBUCKETS 23

/*
 * TODO: Account dev no too.
 */
static uint
hash(unsigned int x)
{
  return x % NBUCKETS;
}

struct {
  struct spinlock lock[NBUCKETS];
  struct buf map[NBUCKETS];
  struct buf buf[NBUF];
} bcache;

void
print_hashmap()
{
  struct buf *b;

  for (int i = 0; i < NBUCKETS; i++) {
    acquire(&bcache.lock[i]);
    printf("Bucket %d: ", i);
    for (b = bcache.map[i].next; b != &bcache.map[i]; b = b->next) {
      printf("%d, ", b->blockno);
    }
    printf("\n");
    release(&bcache.lock[i]);
  }
}

void
binit(void)
{
  struct buf *b;

  for (int i = 0; i < NBUCKETS; i++) {
    initlock(&bcache.lock[i], "bcache");
    bcache.map[i].next = &bcache.map[i];
    bcache.map[i].prev = &bcache.map[i];
  }

  // Create linked list of buffers
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = bcache.map[0].next;
    b->prev = &bcache.map[0];
    initsleeplock(&b->lock, "buffer");
    bcache.map[0].next->prev = b;
    bcache.map[0].next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  uint id;

  id = hash(blockno);

  acquire(&bcache.lock[id]);

  // Is the block already cached?
  for(b = bcache.map[id].next; b != &bcache.map[id]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.lock[id]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Scan the hashmap for free buffers
  for (int i = 0; i < NBUCKETS; i++) {
    if (i == id) continue;

    acquire(&bcache.lock[i]);
    for (b = bcache.map[i].next; b != &bcache.map[i]; b = b->next) {
      if (b->refcnt == 0) {
        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;

        b->prev->next = b->next;
        b->next->prev = b->prev;

        b->next = bcache.map[id].next;
        b->prev = &bcache.map[id];
        bcache.map[id].next->prev = b;
        bcache.map[id].next = b;

        release(&bcache.lock[i]);
        release(&bcache.lock[id]);
        acquiresleep(&b->lock);
        return b;
      }
    }
    release(&bcache.lock[i]);
  }

  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b->dev, b, 0);
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
  virtio_disk_rw(b->dev, b, 1);
}

// Release a locked buffer.
// Move to the head of the MRU list.
void
brelse(struct buf *b)
{
  uint id;

  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  id = hash(b->blockno);
  acquire(&bcache.lock[id]);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.map[id].next;
    b->prev = &bcache.map[id];
    bcache.map[id].next->prev = b;
    bcache.map[id].next = b;
  }

  release(&bcache.lock[id]);
}

void
bpin(struct buf *b) {
  uint id;

  id = hash(b->blockno);
  acquire(&bcache.lock[id]);
  b->refcnt++;
  release(&bcache.lock[id]);
}

void
bunpin(struct buf *b) {
  uint id;

  id = hash(b->blockno);
  acquire(&bcache.lock[id]);
  b->refcnt--;
  release(&bcache.lock[id]);
}


