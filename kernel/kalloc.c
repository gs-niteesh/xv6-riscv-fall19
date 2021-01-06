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

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
  uint *refcounts;
} kmem;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  kmem.refcounts = (uint *) end;
  /* Allocate size of he refcounts array */
  uint64 ref_size = ((PHYSTOP - (uint64)end) / 4096 + 1) * sizeof(uint);
  freerange(end + ref_size, (void*)PHYSTOP);
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

  /* Due COW pages might still be used by other process */
  uint index = ((uint64)pa - (uint64)end) / PGSIZE;

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  kmem.refcounts[index] = 0;
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;
  uint index;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r) {
    kmem.freelist = r->next;
    index = ((uint64)r - (uint64)end) / PGSIZE;
    kmem.refcounts[index] = 1;
  }
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}

void
kpage_ref(void *pa)
{
  uint index = ((uint64)pa - (uint64)end) / PGSIZE;

  acquire(&kmem.lock);
  kmem.refcounts[index] ++;
  release(&kmem.lock);
}

void
kpage_deref(void *pa)
{
  uint index = ((uint64)pa - (uint64)end) / PGSIZE;
  int free = 0;

  acquire(&kmem.lock);
  kmem.refcounts[index] --;
  if (kmem.refcounts[index] == 0)
    free = 1;
  release(&kmem.lock);

  if (free)
    kfree(pa);
}

uint
get_freepages()
{
  struct run *r;
  uint count = 0;

  acquire(&kmem.lock);
  r = kmem.freelist;
  while (r) {
    count ++;
    r = r->next;
  }
  release(&kmem.lock);
  return count;
}