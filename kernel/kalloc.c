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
} kmem;


// Reference counts for physical pages (COW)
// 128MB / 4096 = 32768 pages max
struct {
  struct spinlock lock;
  int count[32768];
} pageref;

#define PA2IDX(pa) (((uint64)(pa) - KERNBASE) / PGSIZE)

// Increment reference count for a physical page
void
incref(void *pa)
{
  if ((uint64)pa < KERNBASE || (uint64)pa >= PHYSTOP) return;
  acquire(&pageref.lock);
  pageref.count[PA2IDX(pa)]++;
  release(&pageref.lock);
}

// Decrement reference count for a physical page
void
decref(void *pa)
{
  if ((uint64)pa < KERNBASE || (uint64)pa >= PHYSTOP) return;
  acquire(&pageref.lock);
  if (pageref.count[PA2IDX(pa)] > 0)
    pageref.count[PA2IDX(pa)]--;
  release(&pageref.lock);
}

// Get reference count for a physical page
int
getref(void *pa)
{
  if ((uint64)pa < KERNBASE || (uint64)pa >= PHYSTOP) return 0;
  return pageref.count[PA2IDX(pa)];
}

// Return number of bytes of free memory.
uint64
kfreemem(void)
{
  struct run *r;
  uint64 count = 0;
  acquire(&kmem.lock);
  r = kmem.freelist;
  while (r) {
    count += PGSIZE;
    r = r->next;
  }
  release(&kmem.lock);
  return count;
}

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&pageref.lock, "pageref");
  freerange(end, (void *)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char *)PGROUNDUP((uint64)pa_start);
  for (; p + PGSIZE <= (char *)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if (((uint64)pa % PGSIZE) != 0 || (char *)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Decrement refcount; only free if it reaches 0
  if ((uint64)pa >= KERNBASE && (uint64)pa < PHYSTOP) {
    acquire(&pageref.lock);
    pageref.count[PA2IDX(pa)]--;
    int rc = pageref.count[PA2IDX(pa)];
    release(&pageref.lock);
    if (rc > 0) return; // still referenced
  }

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run *)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
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
  if (r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if (r) {
    memset((char *)r, 5, PGSIZE); // fill with junk
    // Set initial reference count to 1
    acquire(&pageref.lock);
    pageref.count[PA2IDX(r)] = 1;
    release(&pageref.lock);
  }
  return (void *)r;
}
