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
struct spinlock count_lock;

#define GETID(x) (((((uint64)x) & (0x80000000 - 1)) >> 12) - 70)

// 32730
#define MAXALLOCSIZE 0x7fba

int shared_count[MAXALLOCSIZE];

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

void
kinit()
{
  for (int i = 0; i < MAXALLOCSIZE; ++i) {
    shared_count[i] = 1;
  }
  initlock(&kmem.lock, "kmem");
  initlock(&count_lock, "count_lock");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
  {
    kfree(p);
  } 



}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
int 
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");
  
  acquire(&count_lock);
  if (shared_count[GETID(pa)] <= 0) {
    printf("pa is %p and GETID(pa) is %d\n", (uint64)pa, GETID(pa));
    panic("kfree panic with shread_count <= 0");
  }

  // if (GETID(pa) == 32544) {
  //   printf("kfree %d\n", shared_count[GETID(pa)]);
  // }

  if (--shared_count[GETID(pa)] > 0) {
    release(&count_lock);
    return 0;
  }

  release(&count_lock);

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);

  return 1;
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r)
  {
    memset((char*)r, 5, PGSIZE); // fill with junk
  //   if (shared_count[GETID(r)] != 0) {
  //     printf("shared_count[%d] is %d\n", GETID(r), shared_count[GETID(r)]);
  //     panic("kalloc panic !");
  //   }
  //   if (GETID(r) == 32544) {
  //   printf("kalloc %d\n", shared_count[GETID(r)]);
  // }
    acquire(&count_lock);
    shared_count[GETID(r)] = 1;
    release(&count_lock);
  }  
  return (void*)r;
}

int increase_shared(uint64 pa) {
  int id = GETID(pa);
  if (id < 0 || id >= MAXALLOCSIZE) {
    panic("increase_shread panic");
  }

  acquire(&count_lock);
  shared_count[id]++;
  release(&count_lock);
  return shared_count[id];
}

void decrease_shared(uint64 pa) {
    // not used
  // int id = GETID(pa);
  // if (id < 0 || id >= MAXALLOCSIZE || shared_count[id] <= 0) {
  //   panic("decrease_shared panic");
  // }

  // if (shared_count[id] == 1) {
  //   kfree((void*)pa);
  // }
  // else {
  //   --shared_count[id];
  // }
}
