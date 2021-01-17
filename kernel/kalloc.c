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

static char *lock_names[NCPU] = {"kmem1", "kmem2", "kmem3", "kmem4",
                                 "kmem5", "kmem6", "kmem7", "kmem8"};

struct kmem_t{
  struct spinlock lock;
  struct run *freelist;
};

struct kmem_t kmem[NCPU];

void
kinit()
{
  for (int i = 0; i < NCPU; i++)
    initlock(&kmem[i].lock, lock_names[i]);
  freerange(end, (void*)PHYSTOP);
}

void kfree_per_cpu(void *pa, int id);

void
freerange(void *pa_start, void *pa_end)
{
  char *p, *end1, *end2;
  uint64 pages;

  pa_start = (char*)PGROUNDUP((uint64)pa_start);
  p = pa_start;

  pages = ((char *)pa_end - (char *)p) / PGSIZE;

  end1 = pa_start + (pages / 3) * PGSIZE;
  end2 = end1 + (pages / 3) * PGSIZE;

  for(; p + PGSIZE <= end1; p += PGSIZE)
    kfree_per_cpu(p, 0);
  for(; p + PGSIZE <= end2; p += PGSIZE)
    kfree_per_cpu(p, 1);
  for(; p + PGSIZE <= (char *)pa_end; p += PGSIZE)
    kfree_per_cpu(p, 2);
}

void kfree_per_cpu(void *pa, int id) {
  struct kmem_t *kmemp;
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;
  kmemp = &kmem[id];

  acquire(&kmemp->lock);
  r->next = kmemp->freelist;
  kmemp->freelist = r;
  release(&kmemp->lock);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  int id;

  push_off();
  id = cpuid();
  pop_off();

  kfree_per_cpu(pa, id);
}

void *
get_free_page(int cpuid)
{
  struct kmem_t *kmemp;
  void *free;

  free = 0;

  for (int i = 0; i < NCPU; i++) {
    kmemp = &kmem[i];
    acquire(&kmemp->lock);
    if (kmemp->freelist) {
      free = kmemp->freelist;
      kmemp->freelist = ((struct run *)free)->next;
      release(&kmemp->lock);
      break;
    }
    release(&kmemp->lock);
  }

  return free;
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct kmem_t *kmemp;
  struct run *r;
  int id;

  push_off();
  id = cpuid();
  pop_off();

  kmemp = &kmem[id];
  acquire(&kmemp->lock);
  r = kmemp->freelist;
  if(r)
    kmemp->freelist = r->next;
  release(&kmemp->lock);

  if(!r)
    r = get_free_page(id);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
