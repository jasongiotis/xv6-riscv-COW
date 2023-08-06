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

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  freerange(end, (void*)PHYSTOP);
}
int refcnt[PHYSTOP/PGSIZE]; // ---- how many processes read this page ----//
void freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char *)PGROUNDUP((uint64)pa_start);
  for (; p + PGSIZE <= (char *)pa_end; p += PGSIZE)
  {
    refcnt[(uint64)p / PGSIZE] = 1;  
    kfree(p); //caling free for each page sto rfcnt is back to 0
  }
}
// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void ref_up(uint64 page_address)
{ 
  //opening the lock to change some memory
  acquire(&kmem.lock);
  int page_number = page_address/ PGSIZE;
  int condition=page_address>PHYSTOP || refcnt[page_number]<1;
  if(condition){
    panic("invalid pa or refcnt value");
  }
  refcnt[(int) (page_address / PGSIZE)]++; //increasing value
  release(&kmem.lock);
}

void kfree(void *pa)
{
  struct run *r;
  r = (struct run *)pa;
  if (((uint64)pa % PGSIZE) != 0 || (char *)pa < end || (uint64)pa >= PHYSTOP)
  panic("kfree");
  acquire(&kmem.lock);
  int pn = (uint64)r / PGSIZE;
  refcnt[pn] -= 1;
  int ref_val = refcnt[pn];
  release(&kmem.lock);

  if (ref_val >0) //in this case we free the page
    return;
  
  memset(pa, 1, PGSIZE); //destroying oldS

  acquire(&kmem.lock);
  r->next = kmem.freelist; //updating freelist
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
  {
    int pn = (uint64)r / PGSIZE;
    if(refcnt[pn]!=0){
      panic("refcnt kalloc");
    }
    refcnt[pn] = 1;
    kmem.freelist = r->next;
  }

  release(&kmem.lock);

  if (r)
    memset((char *)r, 5, PGSIZE); // fill with junk
  return (void *)r;
}
