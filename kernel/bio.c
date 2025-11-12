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
#define BUFMAP_HASH(dev, blockno) ((((dev) << 27) | (blockno)) % NBUFBUCKET)
struct {
  struct spinlock lock;
  struct buf buf[NBUF];
  // bufmap 就是一个head
  struct buf bufmap[NBUFBUCKET];
  struct spinlock bufmap_locks[NBUFBUCKET];
  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
} bcache;

void binit(void) {
  struct buf* b;

  initlock(&bcache.lock, "bcache");
  for (int i = 0; i < NBUFBUCKET; i++) {
    initlock(&bcache.bufmap_locks[i], "bufmap");
    bcache.bufmap[i].next = 0;
  }
  for (int i = 0; i < NBUF; i++) {
    b = &bcache.buf[i];
    initsleeplock(&b->lock, "buffer");
    b->lastuse = 0;
    b->refcnt = 0;
    b->next = bcache.bufmap[0].next;
    bcache.bufmap[0].next = b;
  }
  // Create linked list of buffers
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf* bget(uint dev, uint blockno) {
  struct buf* b;
  uint32 Hash = BUFMAP_HASH(dev, blockno);
  acquire(&bcache.bufmap_locks[Hash]);
  // Is the block already cached?
  for (b = bcache.bufmap[Hash].next; b; b = b->next) {
    if (b->dev == dev && b->blockno == blockno) {
      b->refcnt++;
      release(&bcache.bufmap_locks[Hash]);
      acquiresleep(&b->lock);
      return b;
    }
  }
  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  release(&bcache.bufmap_locks[Hash]);
  acquire(&bcache.lock);
  for (b = bcache.bufmap[Hash].next; b; b = b->next) {
    if (b->dev == dev && b->blockno == blockno) {
      acquire(&bcache.bufmap_locks[Hash]);
      b->refcnt++;
      release(&bcache.bufmap_locks[Hash]);

      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  struct buf* lastly_no_used = 0;
  uint holding = -1;
  for (int i = 0; i < NBUFBUCKET; i++) {
    acquire(&bcache.bufmap_locks[i]);
    int finder = 0;
    for (b = &bcache.bufmap[i]; b->next; b = b->next) {
      if (b->next->refcnt == 0 &&
          (lastly_no_used == 0 || b->next->lastuse < lastly_no_used->lastuse)) {
        lastly_no_used = b;
        finder = 1;
      }
    }
    if (!finder) {
      release(&bcache.bufmap_locks[i]);
    } else {
      if (holding != -1) release(&bcache.bufmap_locks[holding]);
      holding = i;
    }
  }
  if (lastly_no_used == 0) {
    panic("bget:  no buffers");
  }
  b = lastly_no_used->next;
  if (holding != Hash) {
    lastly_no_used->next = b->next;
    release(&bcache.bufmap_locks[holding]);
    acquire(&bcache.bufmap_locks[Hash]);
    b->next = bcache.bufmap[Hash].next;
    bcache.bufmap[Hash].next = b;
  }
  b->refcnt = 1;
  b->valid = 0;
  b->dev = dev;
  b->blockno = blockno;
  release(&bcache.bufmap_locks[Hash]);
  release(&bcache.lock);
  acquiresleep(&b->lock);
  return b;
}

// Return a locked buf with the contents of the indicated block.
struct buf* bread(uint dev, uint blockno) {
  struct buf* b;

  b = bget(dev, blockno);
  if (!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void bwrite(struct buf* b) {
  if (!holdingsleep(&b->lock)) panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void brelse(struct buf* b) {
  if (!holdingsleep(&b->lock)) panic("brelse");

  releasesleep(&b->lock);
  uint key = BUFMAP_HASH(b->dev, b->blockno);
  acquire(&bcache.bufmap_locks[key]);
  b->refcnt--;
  if (b->refcnt == 0) {
    b->lastuse = ticks;
  }

  release(&bcache.bufmap_locks[key]);
}
void bpin(struct buf* b) {
  uint key = BUFMAP_HASH(b->dev, b->blockno);

  acquire(&bcache.bufmap_locks[key]);
  b->refcnt++;
  release(&bcache.bufmap_locks[key]);
}

void bunpin(struct buf* b) {
  uint key = BUFMAP_HASH(b->dev, b->blockno);

  acquire(&bcache.bufmap_locks[key]);
  b->refcnt--;
  release(&bcache.bufmap_locks[key]);
}
