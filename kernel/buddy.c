#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

// Buddy allocator

static int nsizes = 5;  // for debugging

#define LEAF_SIZE     16 // The smallest allocation size (in bytes)
#define MAXSIZE       (nsizes-1) // Largest index in bd_sizes array
#define BLK_SIZE(k)   ((1L << (k)) * LEAF_SIZE) // Size in bytes for size k
#define HEAP_SIZE     BLK_SIZE(MAXSIZE) 
#define NBLK(k)       (1 << (MAXSIZE-k))  // Number of block at size k
#define ROUNDUP(n,sz) (((((n)-1)/(sz))+1)*(sz))  // Round up to the next multiple of sz

typedef struct list Bd_list;

// The allocator has sz_info for each size k. Each sz_info has a free
// list, an array alloc to keep track which blocks have been
// allocated, and an split array to to keep track which blocks have
// been split.  The arrays are of type char (which is 1 byte), but the
// allocator uses 1 bit per block (thus, one char records the info of
// 8 blocks).
struct sz_info {
  Bd_list free;
  char *alloc;
  char *split;
};
typedef struct sz_info Sz_info;

static Sz_info *bd_sizes;
static void *bd_base;   // start address of memory managed by the buddy allocator
static struct spinlock lock;

// Return 1 if bit at position index in array is set to 1
int bit_isset(char *array, int index) {
  char b = array[index/8];
  char m = (1 << (index % 8));
  return (b & m) == m;
}

// Set bit at position index in array to 1
void bit_set(char *array, int index) {
  char b = array[index/8];
  char m = (1 << (index % 8));
  array[index/8] = (b | m);
}

// Clear bit at position index in array
void bit_clear(char *array, int index) {
  char b = array[index/8];
  char m = (1 << (index % 8));
  array[index/8] = (b & ~m);
}

void
bd_print() {
  for (int k = 0; k < nsizes; k++) {
    printf("size %d (%d):", k, BLK_SIZE(k));
    lst_print(&bd_sizes[k].free);
    printf("  alloc:");
    for (int b = 0; b < NBLK(k); b++) {
     printf(" %d", bit_isset(bd_sizes[k].alloc, b));
    }
    printf("\n");
    if(k > 0) {
      printf("  split:");
      for (int b = 0; b < NBLK(k); b++) {
        printf(" %d", bit_isset(bd_sizes[k].split, b));
      }
      printf("\n");
    }
  }
}

// What is the first k such that 2^k >= n?
int
firstk(uint64 n) {
  int k = 0;
  uint64 size = LEAF_SIZE;

  while (size < n) {
    k++;
    size *= 2;
  }
  return k;
}

// Compute the block index for address p at size k
int
blk_index(int k, char *p) {
  int n = p - (char *) bd_base;
  return n / BLK_SIZE(k);
}

// Convert a block index at size k back into an address
void *addr(int k, int bi) {
  int n = bi * BLK_SIZE(k);
  return (char *) bd_base + n;
}

void *
bd_malloc(uint64 nbytes)
{
  int fk, k;

  acquire(&lock);
  // Find a free block >= nbytes, starting with smallest k possible
  fk = firstk(nbytes);
  for (k = fk; k < nsizes; k++) {
    if(!lst_empty(&bd_sizes[k].free))
      break;
  }
  if(k >= nsizes) { // No free blocks?
    release(&lock);
    return 0;
  }

  // Found one; pop it and potentially split it.
  char *p = lst_pop(&bd_sizes[k].free);
  bit_set(bd_sizes[k].alloc, blk_index(k, p));
  for(; k > fk; k--) {
    char *q = p + BLK_SIZE(k-1);
    bit_set(bd_sizes[k].split, blk_index(k, p));
    bit_set(bd_sizes[k-1].alloc, blk_index(k-1, p));
    lst_push(&bd_sizes[k-1].free, q);
  }
  //printf("malloc: %p size class %d\n", p, fk);
  release(&lock);
  return p;
}

// Find the size of the block that p points to.
int
size(char *p) {
  for (int k = 0; k < nsizes; k++) {
    if(bit_isset(bd_sizes[k+1].split, blk_index(k+1, p))) {
      return k;
    }
  }
  return 0;
}

void
bd_free(void *p) {
  void *q;
  int k;

  acquire(&lock);
  for (k = size(p); k < MAXSIZE; k++) {
    int bi = blk_index(k, p);
    int buddy = (bi % 2 == 0) ? bi+1 : bi-1;
    bit_clear(bd_sizes[k].alloc, bi);
    if (bit_isset(bd_sizes[k].alloc, buddy)) {
      break;
    }
    // budy is free; merge with buddy
    q = addr(k, buddy);
    lst_remove(q);
    if(buddy % 2 == 0) {
      p = q;
    }
    bit_clear(bd_sizes[k+1].split, blk_index(k+1, p));
  }
  //printf("free %p @ %d\n", p, k);
  lst_push(&bd_sizes[k].free, p);
  release(&lock);
}

int
blk_index_next(int k, char *p) {
  int n = (p - (char *) bd_base) / BLK_SIZE(k);
  if((p - (char*) bd_base) % BLK_SIZE(k) != 0)
      n++;
  return n ;
}

int
log2(uint64 n) {
  int k = 0;
  while (n > 1) {
    k++;
    n = n >> 1;
  }
  return k;
}

// The buddy allocator manages the memory from base till end.
void
bd_init(void *base, void *end) {

  initlock(&lock, "buddy");

  // YOUR CODE HERE TO INITIALIZE THE BUDDY ALLOCATOR.  FEEL FREE TO
  // BORROW CODE FROM bd_init() in the lecture notes.

  return;
}
