// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void* pa_start, void* pa_end);

extern char end[];  // first address after kernel.
                    // defined by kernel.ld.
#define PA2PAGEID(addr) ((addr - KERNBASE) / PGSIZE)
#define PA2REFCOUNT(addr) (page_refcounts[PA2PAGEID((uint64)addr)])
int page_refcounts[PA2PAGEID(PHYSTOP)];
struct spinlock pagelocks;
struct run {
  struct run* next;
};

struct {
  struct spinlock lock;
  struct run* freelist;
} kmem;

void kinit() {
  initlock(&kmem.lock, "kmem");
  initlock(&pagelocks, "pagenum");
  freerange(end, (void*)PHYSTOP);
  // memset(page_refcounts, 0, sizeof(page_refcounts));
}

void freerange(void* pa_start, void* pa_end) {
  char* p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for (; p + PGSIZE <= (char*)pa_end; p += PGSIZE) kfree(p);
}
void krefpageadd(uint64 pa) {
  acquire(&pagelocks);
  PA2REFCOUNT(pa)++;
  release(&pagelocks);
}
void krefpagedel(uint64 pa) { PA2REFCOUNT(pa)--; }

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void kfree(void* pa) {
  struct run* r;
  if (((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  acquire(&pagelocks);
  if (--PA2REFCOUNT(pa) <= 0) {
    memset(pa, 1, PGSIZE);
    
    r = (struct run*)pa;
    
    acquire(&kmem.lock);
    r->next = kmem.freelist;
    kmem.freelist = r;
    release(&kmem.lock);
  }
  release(&pagelocks);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void* kalloc(void) {
  struct run* r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if (r) kmem.freelist = r->next;
  release(&kmem.lock);

  if (r) {
    memset((char*)r, 5, PGSIZE);
    PA2REFCOUNT(r) = 1;
  }  // fill with junk
  return (void*)r;
}
void* krefalloc(void* pa) {
  acquire(&pagelocks);
  if (PA2REFCOUNT(pa) <= 1) {
    release(&pagelocks);
    return pa;
  }
  uint64 newpa = (uint64)kalloc();
  if (newpa == 0) {
    release(&pagelocks);
    return 0;
  }
  memmove((void*)newpa, pa, PGSIZE);
  PA2REFCOUNT(pa)--;
  release(&pagelocks);
  return (void*)newpa;
}