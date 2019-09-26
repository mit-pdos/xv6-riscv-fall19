//
// network system calls.
//

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

// send(uint32 dst, uint16 dport, uint16 sport, char *buf, int n)
uint64
sys_send(void)
{
  int ret = -1;

  //
  // Your code here.
  //
  // Format a UDP packet, send it out the e1000.
  //
  
  // SOL_E1000
  struct proc *p = myproc();
  int udp_send(uint32, uint16, uint16, const char *, int);
  uint32 dst;
  uint32 dport;
  uint32 sport;
  uint64 addr;
  int n;
  
  argint(0, (int*)&dst);
  argint(1, (int*)&dport);
  argint(2, (int*)&sport);
  argaddr(3, &addr);
  argint(4, &n);

  if(n < 0 || n > PGSIZE)
    return -1;

  char *buf = kalloc();
  if(buf == 0)
    panic("sys_send kalloc");

  if(copyin(p->pagetable, buf, addr, n) != 0){
    kfree(buf);
    return -1;
  }

  ret = udp_send(dst, dport, sport, buf, n);

  kfree(buf);
  // END_E1000
  
  return ret;
}

// recv(uint32 *src, uint16 *sport, uint16 *dport, char *buf, int n);
uint64
sys_recv(void)
{
  int ret = -1;
  
  //
  // Your code here.
  //
  // Wait for a UDP packet to arrive from the e1000,
  // parse it, return its src/sport/dport/payload.
  //
  // SOL_E1000
  struct proc *p = myproc();
  int udp_recv(uint32 *srcp, uint16 *sportp, uint16 *dportp, char *buf, int bufmax);
  uint64 srcp, sportp, dportp;
  uint64 addr;
  int n;

  argaddr(0, &srcp);
  argaddr(1, &sportp);
  argaddr(2, &dportp);
  argaddr(3, &addr);
  argint(4, &n);

  if(n < 0 || n > PGSIZE)
    return -1;

  char *buf = kalloc();
  if(buf == 0)
    panic("sys_recv kalloc");

  uint32 src;
  uint16 sport, dport;
  int cc = udp_recv(&src, &sport, &dport, buf, PGSIZE);

  if(cc > n)
    cc = n;

  if(cc >= 0){
    copyout(p->pagetable, srcp, (char*)&src, 4);
    copyout(p->pagetable, sportp, (char*)&sport, 2);
    copyout(p->pagetable, dportp, (char*)&dport, 2);
    copyout(p->pagetable, addr, buf, cc);
  }

  kfree(buf);

  ret = cc;
  // END_E1000

  return ret;
}

// SOL_E1000
// wait for a UDP packet to arrive, return it to the user process.
// while waiting, process ARP requests.
int
udp_recv(uint32 *srcp, uint16 *sportp, uint16 *dportp,
         char *payload, int paymax)
{
  while(1){
    char *buf;
    int len;

    e1000_recv(&buf, &len);

    if(handle_arp(buf, len)){
      kfree(buf);
    } else {
      int plen = parse_udp(buf, len, payload, paymax,
                           srcp, sportp, dportp);
      printf(" plen=%d src=%x sport=%d dport=%d\n", plen, *srcp, *sportp, *dportp);
      
      kfree(buf);

      if(plen >= 0)
        return plen;
    }
  }
}

int
udp_send(uint32 dst, uint16 dport, uint16 sport,
         const char *payload, int paylen)
{
  
  char *buf = kalloc();
  if(buf == 0)
    panic("udp_send kalloc");

  // 10.0.2.15 is qemu's idea of the guest's IP address.
  uint32 src = (10 << 24) | (0 << 16) | (2 << 8) | (15 << 0);

  int framelen = format_udp(buf, PGSIZE, dst, dport, src, sport,
                            payload, paylen);

  if(framelen < 0){
    printf("udp_format failed\n");
  }  else {
    e1000_transmit(buf, framelen);
  }

  kfree(buf);

  return 0;
}
// END_E1000
