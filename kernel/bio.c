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

#define NBUCKET 13

struct {
  struct spinlock locks[NBUCKET];

  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.


  struct buf* heads[NBUCKET];

  struct spinlock lock;
  struct spinlock steal_lock;
  
} bcache;

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");
  initlock(&bcache.steal_lock, "bcache.steal_lock");

  for (int i = 0 ; i < NBUCKET; ++i) {
    initlock(&bcache.locks[i], "bcache.bucket");
  }

  struct buf dummy;
  // dummy.next = bcache.heads[0];
  struct buf * pre = &dummy;
  

  for (b = bcache.buf; b < bcache.buf + NBUF; ++b) {
    initsleeplock(&b->lock, "buffer");
    pre->next = b;
    pre = pre->next;
  }

  bcache.heads[0] = dummy.next;

  // Create linked list of buffers
  // bcache.head.prev = &bcache.head;
  // bcache.head.next = &bcache.head;
  // for(b = bcache.buf; b < bcache.buf+NBUF; b++){
  //   b->next = bcache.head.next;
  //   b->prev = &bcache.head;
  //   initsleeplock(&b->lock, "buffer");
  //   bcache.head.next->prev = b;
  //   bcache.head.next = b;
  // }
}

int GetLen(struct buf * head) {
  int res = 0;
  while(head) {
    head = head->next;
    res ++;
  }
  return res;
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  int index = blockno % NBUCKET;

  acquire(&bcache.locks[index]);

  // Is the block already cached?
  for (b = bcache.heads[index]; b != 0; b = b->next) {
    if (b->dev == dev && b->blockno == blockno) {
      b->refcnt++;
      release(&bcache.locks[index]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  for (b = bcache.heads[index]; b != 0; b = b->next) {
    if (b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      acquiresleep(&b->lock);
      release(&bcache.locks[index]);
      return b;
    }
  }

  release(&bcache.locks[index]);

  acquire(&bcache.steal_lock);
  acquire(&bcache.locks[index]);

  // after getting steal_lock, check list[index] again, make sure no node has been set to what we are going to do.
  for (b = bcache.heads[index]; b != 0; b = b->next) {
    if (b->dev == dev && b->blockno == blockno) {
      b->refcnt++;
      release(&bcache.locks[index]);
      release(&bcache.steal_lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  for (b = bcache.heads[index]; b != 0; b = b->next) {
    if (b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      acquiresleep(&b->lock);
      release(&bcache.locks[index]);
      release(&bcache.steal_lock);
      return b;
    }
  }

  // get a node from other list
  for (int i = 0; i < NBUCKET; ++i) {
    if (i == index) {
      continue;
    }

    acquire(&bcache.locks[i]);
    // now we has global lock(bcache.lock) and bucket lock (bcache.locks[i])

    struct buf dummy;
    dummy.next = bcache.heads[i];
    struct buf *pre = &dummy;

    // search an aviable cache from list[i]
    for (b = bcache.heads[i]; b != 0; b = b->next, pre = pre->next) {
      if (b->refcnt == 0) {

        // remove b from list[i]
        pre->next = b->next;
        b->next = 0;
        bcache.heads[i] = dummy.next;

        // insert b into list[index] at the end of list[index]
        dummy.next = bcache.heads[index];
        pre = &dummy;
        while (pre->next) {
          pre = pre->next;
        }

        pre->next = b;
        bcache.heads[index] = dummy.next;

        // set b's data
        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;
        acquiresleep(&b->lock);

        // lock handle
        release(&bcache.locks[index]);
        release(&bcache.locks[i]);
        release(&bcache.steal_lock);
        return b;
      }
    }

    release(&bcache.locks[i]);
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

  int index = b->blockno % NBUCKET;

  acquire(&bcache.locks[index]);

  b->refcnt--;

   if (b->refcnt == 0) {
    struct buf dummy;
    dummy.next = bcache.heads[index];
    dummy.blockno = 65536;
    struct buf * pre = &dummy;
    struct buf * cur = bcache.heads[index];

    // printf("%d, %d, %d\n", dummy.blockno, pre->blockno, cur->blockno);

    while (cur != b) {
      pre = pre->next;
      cur = cur->next;
    }

    // printf("%d, %d, %d\n", dummy.blockno, pre->blockno, cur->blockno);

    pre->next = b->next;
    b->next = 0;
    while (pre->next) {
      pre = pre->next;
    }
    pre->next = b;

    bcache.heads[index] = dummy.next;
   }

  release(&bcache.locks[index]);

  return ;

  // acquire(&bcache.lock);
  // b->refcnt--;
  // if (b->refcnt == 0) {
  //   // no one is waiting for it.
  //   b->next->prev = b->prev;
  //   b->prev->next = b->next;
  //   b->next = bcache.heads.next;
  //   b->prev = &bcache.heads;
  //   bcache.heads.next->prev = b;
  //   bcache.heads.next = b;
  // }
  
  // release(&bcache.lock);
}

void
bpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt++;
  release(&bcache.lock);
}

void
bunpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt--;
  release(&bcache.lock);
}


