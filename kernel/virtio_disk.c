//
// driver for qemu's virtio disk device.
// uses qemu's mmio interface to virtio.
// qemu presents a "legacy" virtio interface.
//
// qemu ... -drive file=fs.img,if=none,format=raw,id=x0 -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0
//

#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "buf.h"
#include "virtio.h"

// the address of virtio mmio register r.
#define R(n, r) ((volatile uint32 *)(VIRTION(n) + (r)))

// this many virtio descriptors.
// must be a power of two.
#define NUM 8

struct disk {
  // memory for virtio descriptors &c for queue 0.
  // this is a global instead of allocated because it has
  // to be multiple contiguous pages, which kalloc()
  // doesn't support.
  char pages[2*PGSIZE];
  
  struct virtq_desc *desc;
  struct virtq_avail *avail;
  struct virtq_used *used;

  // our own book-keeping.
  char free[NUM];  // is a descriptor free?
  uint16 used_idx; // we've looked this far in used->ring.

  // track info about in-flight operations,
  // for use when completion interrupt arrives.
  // indexed by first descriptor index of chain.
  struct {
    struct buf *b;
    char status;
  } info[NUM];

  // initialized?
  int init;

  struct spinlock vdisk_lock;
} __attribute__ ((aligned (PGSIZE))) disk[NDISK];
  


void
virtio_disk_init(int n)
{
  uint32 status = 0;

  __sync_synchronize();
  if(disk[n].init)
    return;

  printf("virtio disk init %d\n", n);
  
  initlock(&disk[n].vdisk_lock, "virtio_disk");

  if(*R(n, VIRTIO_MMIO_MAGIC_VALUE) != 0x74726976 ||
     *R(n, VIRTIO_MMIO_VERSION) != 1 ||
     *R(n, VIRTIO_MMIO_DEVICE_ID) != 2 ||
     *R(n, VIRTIO_MMIO_VENDOR_ID) != 0x554d4551){
    panic("could not find virtio disk");
  }

  status |= VIRTIO_CONFIG_S_ACKNOWLEDGE;
  *R(n, VIRTIO_MMIO_STATUS) = status;

  status |= VIRTIO_CONFIG_S_DRIVER;
  *R(n, VIRTIO_MMIO_STATUS) = status;

  // negotiate features
  uint64 features = *R(n, VIRTIO_MMIO_DEVICE_FEATURES);
  features &= ~(1 << VIRTIO_BLK_F_RO);
  features &= ~(1 << VIRTIO_BLK_F_SCSI);
  features &= ~(1 << VIRTIO_BLK_F_CONFIG_WCE);
  features &= ~(1 << VIRTIO_BLK_F_MQ);
  features &= ~(1 << VIRTIO_F_ANY_LAYOUT);
  features &= ~(1 << VIRTIO_RING_F_EVENT_IDX);
  features &= ~(1 << VIRTIO_RING_F_INDIRECT_DESC);
  *R(n, VIRTIO_MMIO_DRIVER_FEATURES) = features;

  // tell device that feature negotiation is complete.
  status |= VIRTIO_CONFIG_S_FEATURES_OK;
  *R(n, VIRTIO_MMIO_STATUS) = status;

  // tell device we're completely ready.
  status |= VIRTIO_CONFIG_S_DRIVER_OK;
  *R(n, VIRTIO_MMIO_STATUS) = status;

  *R(n, VIRTIO_MMIO_GUEST_PAGE_SIZE) = PGSIZE;

  // initialize queue 0.
  *R(n, VIRTIO_MMIO_QUEUE_SEL) = 0;
  uint32 max = *R(n, VIRTIO_MMIO_QUEUE_NUM_MAX);
  if(max == 0)
    panic("virtio disk has no queue 0");
  if(max < NUM)
    panic("virtio disk max queue too short");
  *R(n, VIRTIO_MMIO_QUEUE_NUM) = NUM;
  memset(disk[n].pages, 0, sizeof(disk[n].pages));
  *R(n, VIRTIO_MMIO_QUEUE_PFN) = ((uint64)disk[n].pages) >> PGSHIFT;

  // desc = pages -- num * virtq_desc
  // avail = pages + 0x40 -- 2 * uint16, then num * uint16
  // used = pages + 4096 -- 2 * uint16, then num * virtq_used_elem

  disk[n].desc = (struct virtq_desc *) disk[n].pages;
  disk[n].avail = (struct virtq_avail *)(((char*)disk[n].desc) + NUM*sizeof(struct virtq_desc));
  disk[n].used = (struct virtq_used *) (disk[n].pages + PGSIZE);

  for(int i = 0; i < NUM; i++)
    disk[n].free[i] = 1;

  disk[n].init = 1;
  // plic.c and trap.c arrange for interrupts from VIRTIO0_IRQ.
}

// find a free descriptor, mark it non-free, return its index.
static int
alloc_desc(int n)
{
  for(int i = 0; i < NUM; i++){
    if(disk[n].free[i]){
      disk[n].free[i] = 0;
      return i;
    }
  }
  return -1;
}

// mark a descriptor as free.
static void
free_desc(int n, int i)
{
  if(i >= NUM)
    panic("virtio_disk_intr 1");
  if(disk[n].free[i])
    panic("virtio_disk_intr 2");
  disk[n].desc[i].addr = 0;
  disk[n].free[i] = 1;
  wakeup(&disk[n].free[0]);
}

// free a chain of descriptors.
static void
free_chain(int n, int i)
{
  while(1){
    free_desc(n, i);
    if(disk[n].desc[i].flags & VIRTQ_DESC_F_NEXT)
      i = disk[n].desc[i].next;
    else
      break;
  }
}

static int
alloc3_desc(int n, int *idx)
{
  for(int i = 0; i < 3; i++){
    idx[i] = alloc_desc(n);
    if(idx[i] < 0){
      for(int j = 0; j < i; j++)
        free_desc(n, idx[j]);
      return -1;
    }
  }
  return 0;
}

void
virtio_disk_rw(int n, struct buf *b, int write)
{
  uint64 sector = b->blockno * (BSIZE / 512);

  acquire(&disk[n].vdisk_lock);

  // the spec says that legacy block operations use three
  // descriptors: one for type/reserved/sector, one for
  // the data, one for a 1-byte status result.

  // allocate the three descriptors.
  int idx[3];
  while(1){
    if(alloc3_desc(n, idx) == 0) {
      break;
    }
    sleep(&disk[n].free[0], &disk[n].vdisk_lock);
  }
  
  // format the three descriptors.
  // qemu's virtio-blk.c reads them.

  struct virtio_blk_outhdr {
    uint32 type;
    uint32 reserved;
    uint64 sector;
  } buf0;

  if(write)
    buf0.type = VIRTIO_BLK_T_OUT; // write the disk
  else
    buf0.type = VIRTIO_BLK_T_IN; // read the disk
  buf0.reserved = 0;
  buf0.sector = sector;

  // buf0 is on a kernel stack, which is not direct mapped,
  // thus the call to kvmpa().
  disk[n].desc[idx[0]].addr = (uint64) kvmpa((uint64) &buf0);
  disk[n].desc[idx[0]].len = sizeof(buf0);
  disk[n].desc[idx[0]].flags = VIRTQ_DESC_F_NEXT;
  disk[n].desc[idx[0]].next = idx[1];

  disk[n].desc[idx[1]].addr = (uint64) b->data;
  disk[n].desc[idx[1]].len = BSIZE;
  if(write)
    disk[n].desc[idx[1]].flags = 0; // device reads b->data
  else
    disk[n].desc[idx[1]].flags = VIRTQ_DESC_F_WRITE; // device writes b->data
  disk[n].desc[idx[1]].flags |= VIRTQ_DESC_F_NEXT;
  disk[n].desc[idx[1]].next = idx[2];

  disk[n].info[idx[0]].status = 0;
  disk[n].desc[idx[2]].addr = (uint64) &disk[n].info[idx[0]].status;
  disk[n].desc[idx[2]].len = 1;
  disk[n].desc[idx[2]].flags = VIRTQ_DESC_F_WRITE; // device writes the status
  disk[n].desc[idx[2]].next = 0;

  // record struct buf for virtio_disk_intr().
  b->disk = 1;
  disk[n].info[idx[0]].b = b;

  // avail->idx tells the device how far to look in avail->ring.
  // avail->ring[...] are desc[] indices the device should process.
  // we only tell device the first index in our chain of descriptors.
  disk[n].avail->ring[disk[n].avail->idx % NUM] = idx[0];
  __sync_synchronize();
  disk[n].avail->idx += 1;

  *R(n, VIRTIO_MMIO_QUEUE_NOTIFY) = 0; // value is queue number

  // Wait for virtio_disk_intr() to say request has finished.
  while(b->disk == 1) {
    sleep(b, &disk[n].vdisk_lock);
  }

  disk[n].info[idx[0]].b = 0;
  free_chain(n, idx[0]);

  release(&disk[n].vdisk_lock);
}

void
virtio_disk_intr(int n)
{
  acquire(&disk[n].vdisk_lock);

  while((disk[n].used_idx % NUM) != (disk[n].used->idx % NUM)){
    int id = disk[n].used->ring[disk[n].used_idx].id;

    if(disk[n].info[id].status != 0)
      panic("virtio_disk_intr status");
    
    disk[n].info[id].b->disk = 0;   // disk is done with buf
    wakeup(disk[n].info[id].b);

    disk[n].used_idx = (disk[n].used_idx + 1) % NUM;
  }

  release(&disk[n].vdisk_lock);
}

