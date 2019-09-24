//
// mostly copied from JOS
//

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

#define PADDR(x) ((uint64)(x))
typedef uint64 uint64_t;
typedef uint32 uint32_t;
typedef uint16 uint16_t;
typedef uint8 uint8_t;
#define MIN(a, b) ((a) < (b) ? (a) : (b))

int e1000_transmit(const char *buf, unsigned int len);

/* Registers */
#define E1000_ICR      (0x000C0/4)  /* Interrupt Cause Read - R */
#define E1000_IMS      (0x000D0/4)  /* Interrupt Mask Set - RW */
#define E1000_RCTL     (0x00100/4)  /* RX Control - RW */
#define E1000_TCTL     (0x00400/4)  /* TX Control - RW */
#define E1000_TIPG     (0x00410/4)  /* TX Inter-packet gap -RW */
#define E1000_RDBAL    (0x02800/4)  /* RX Descriptor Base Address Low - RW */
#define E1000_RDTR     (0x02820/4)  /* RX Delay Timer */
#define E1000_RADV     (0x0282C/4)  /* RX Interrupt Absolute Delay Timer */
#define E1000_RDH      (0x02810/4)  /* RX Descriptor Head - RW */
#define E1000_RDT      (0x02818/4)  /* RX Descriptor Tail - RW */
#define E1000_RDLEN    (0x02808/4)  /* RX Descriptor Length - RW */
#define E1000_RSRPD    (0x02C00/4)  /* RX Small Packet Detect Interrupt */
#define E1000_TDBAL    (0x03800/4)  /* TX Descriptor Base Address Low - RW */
#define E1000_TDLEN    (0x03808/4)  /* TX Descriptor Length - RW */
#define E1000_TDH      (0x03810/4)  /* TX Descriptor Head - RW */
#define E1000_TDT      (0x03818/4)  /* TX Descripotr Tail - RW */
#define E1000_MTA      (0x05200/4)  /* Multicast Table Array - RW Array */
#define E1000_RA       (0x05400/4)  /* Receive Address - RW Array */

/* Transmit Control */
#define E1000_TCTL_RST    0x00000001    /* software reset */
#define E1000_TCTL_EN     0x00000002    /* enable tx */
#define E1000_TCTL_BCE    0x00000004    /* busy check enable */
#define E1000_TCTL_PSP    0x00000008    /* pad short packets */
#define E1000_TCTL_CT     0x00000ff0    /* collision threshold */
#define E1000_TCTL_CT_SHIFT 4
#define E1000_TCTL_COLD   0x003ff000    /* collision distance */
#define E1000_TCTL_COLD_SHIFT 12
#define E1000_TCTL_SWXOFF 0x00400000    /* SW Xoff transmission */
#define E1000_TCTL_PBE    0x00800000    /* Packet Burst Enable */
#define E1000_TCTL_RTLC   0x01000000    /* Re-transmit on late collision */
#define E1000_TCTL_NRTU   0x02000000    /* No Re-transmit on underrun */
#define E1000_TCTL_MULR   0x10000000    /* Multiple request support */

/* Receive Control */
#define E1000_RCTL_RST            0x00000001    /* Software reset */
#define E1000_RCTL_EN             0x00000002    /* enable */
#define E1000_RCTL_SBP            0x00000004    /* store bad packet */
#define E1000_RCTL_UPE            0x00000008    /* unicast promiscuous enable */
#define E1000_RCTL_MPE            0x00000010    /* multicast promiscuous enab */
#define E1000_RCTL_LPE            0x00000020    /* long packet enable */
#define E1000_RCTL_LBM_NO         0x00000000    /* no loopback mode */
#define E1000_RCTL_LBM_MAC        0x00000040    /* MAC loopback mode */
#define E1000_RCTL_LBM_SLP        0x00000080    /* serial link loopback mode */
#define E1000_RCTL_LBM_TCVR       0x000000C0    /* tcvr loopback mode */
#define E1000_RCTL_DTYP_MASK      0x00000C00    /* Descriptor type mask */
#define E1000_RCTL_DTYP_PS        0x00000400    /* Packet Split descriptor */
#define E1000_RCTL_RDMTS_HALF     0x00000000    /* rx desc min threshold size */
#define E1000_RCTL_RDMTS_QUAT     0x00000100    /* rx desc min threshold size */
#define E1000_RCTL_RDMTS_EIGTH    0x00000200    /* rx desc min threshold size */
#define E1000_RCTL_MO_SHIFT       12            /* multicast offset shift */
#define E1000_RCTL_MO_0           0x00000000    /* multicast offset 11:0 */
#define E1000_RCTL_MO_1           0x00001000    /* multicast offset 12:1 */
#define E1000_RCTL_MO_2           0x00002000    /* multicast offset 13:2 */
#define E1000_RCTL_MO_3           0x00003000    /* multicast offset 15:4 */
#define E1000_RCTL_MDR            0x00004000    /* multicast desc ring 0 */
#define E1000_RCTL_BAM            0x00008000    /* broadcast enable */
/* these buffer sizes are valid if E1000_RCTL_BSEX is 0 */
#define E1000_RCTL_SZ_2048        0x00000000    /* rx buffer size 2048 */
#define E1000_RCTL_SZ_1024        0x00010000    /* rx buffer size 1024 */
#define E1000_RCTL_SZ_512         0x00020000    /* rx buffer size 512 */
#define E1000_RCTL_SZ_256         0x00030000    /* rx buffer size 256 */
/* these buffer sizes are valid if E1000_RCTL_BSEX is 1 */
#define E1000_RCTL_SZ_16384       0x00010000    /* rx buffer size 16384 */
#define E1000_RCTL_SZ_8192        0x00020000    /* rx buffer size 8192 */
#define E1000_RCTL_SZ_4096        0x00030000    /* rx buffer size 4096 */
#define E1000_RCTL_VFE            0x00040000    /* vlan filter enable */
#define E1000_RCTL_CFIEN          0x00080000    /* canonical form enable */
#define E1000_RCTL_CFI            0x00100000    /* canonical form indicator */
#define E1000_RCTL_DPF            0x00400000    /* discard pause frames */
#define E1000_RCTL_PMCF           0x00800000    /* pass MAC control frames */
#define E1000_RCTL_BSEX           0x02000000    /* Buffer size extension */
#define E1000_RCTL_SECRC          0x04000000    /* Strip Ethernet CRC */
#define E1000_RCTL_FLXBUF_MASK    0x78000000    /* Flexible buffer size */
#define E1000_RCTL_FLXBUF_SHIFT   27            /* Flexible buffer shift */

#define DATA_MAX 1518

/* Transmit Descriptor command definitions [E1000 3.3.3.1] */
#define E1000_TXD_CMD_EOP    0x01 /* End of Packet */
#define E1000_TXD_CMD_RS     0x08 /* Report Status */

/* Transmit Descriptor status definitions [E1000 3.3.3.2] */
#define E1000_TXD_STAT_DD    0x00000001 /* Descriptor Done */

// [E1000 3.3.3]
struct tx_desc
{
	uint64_t addr;
	uint16_t length;
	uint8_t cso;
	uint8_t cmd;
	uint8_t status;
	uint8_t css;
	uint16_t special;
};

#define TX_RING_SIZE 16
static struct tx_desc tx_ring[TX_RING_SIZE] __attribute__((aligned(16)));
static char tx_data[TX_RING_SIZE][DATA_MAX];

/* Receive Descriptor bit definitions [E1000 3.2.3.1] */
#define E1000_RXD_STAT_DD       0x01    /* Descriptor Done */
#define E1000_RXD_STAT_EOP      0x02    /* End of Packet */

// [E1000 3.2.3]
struct rx_desc
{
	uint64_t addr;       /* Address of the descriptor's data buffer */
	uint16_t length;     /* Length of data DMAed into data buffer */
	uint16_t csum;       /* Packet checksum */
	uint8_t status;      /* Descriptor status */
	uint8_t errors;      /* Descriptor Errors */
	uint16_t special;
};

#define RX_RING_SIZE 16
static struct rx_desc rx_ring[RX_RING_SIZE] __attribute__((aligned(16)));
static char rx_data[RX_RING_SIZE][2048];

static volatile uint32 *regs;

struct spinlock e1000_lock;

void
e1000init()
{
  int i;

  initlock(&e1000_lock, "e1000");
  
  // qemu -machine virt puts PCIe config space here.
  uint32  *ecam = (uint32 *) 0x30000000L;

  // look at each device on bus 0.
  for(int dev = 0; dev < 32; dev++){
    int bus = 0;
    int func = 0;
    int offset = 0;
    uint32 off = (bus << 16) | (dev << 11) | (func << 8) | (offset);
    volatile uint32 *base = ecam + off;
    uint32 id = base[0];
    
    // 0x100e8086 is e1000
    if(id == 0x100e8086){
      // command and status register.
      // bit 0 : I/O access enable
      // bit 1 : memory access enable
      // bit 2 : enable mastering
      base[1] = 7;
      __sync_synchronize();

      for(int i = 0; i < 6; i++){
        uint32 old = base[4+i];

        // writing all 1's to the BAR causes it to be
        // replaced with its size (!).
        base[4+i] = 0xffffffff;
        __sync_synchronize();

        base[4+i] = old;
      }

      // tell the e1000 to reveal its registers at
      // physical address 0x40000000.
      base[4+0] = 0x40000000;
    }
  }

  regs = (uint32*)0x40000000L;

  // copied from JOS
  
	// [E1000 14.5] Transmit initialization
        memset(tx_ring, 0, sizeof(tx_ring));
	for (i = 0; i < TX_RING_SIZE; i++) {
		tx_ring[i].addr = PADDR(tx_data[i]);
		tx_ring[i].status = E1000_TXD_STAT_DD;
	}
	regs[E1000_TDBAL] = PADDR(tx_ring);
	if(sizeof(tx_ring) % 128 != 0)
          panic("e1000");
	regs[E1000_TDLEN] = sizeof(tx_ring);
	regs[E1000_TDH] = regs[E1000_TDT] = 0;
	regs[E1000_TCTL] = (E1000_TCTL_EN | E1000_TCTL_PSP |
			    (0x10 << E1000_TCTL_CT_SHIFT) |
			    (0x40 << E1000_TCTL_COLD_SHIFT));
	regs[E1000_TIPG] = 10 | (8<<10) | (6<<20);

	// [E1000 14.4] Receive initialization
        memset(rx_ring, 0, sizeof(rx_ring));
	for (i = 0; i < RX_RING_SIZE; i++) {
		rx_ring[i].addr = PADDR(rx_data[i]);
	}
        // filter by qemu's MAC address, 52:54:00:12:34:56
	regs[E1000_RA] = 0x12005452;
	regs[E1000_RA+1] = 0x5634 | (1<<31);
	for (i = 0; i < 4096/32; i++)
		regs[E1000_MTA + i] = 0;
	regs[E1000_RDBAL] = PADDR(rx_ring);
	if(sizeof(rx_ring) % 128 != 0)
          panic("e1000");
	regs[E1000_RDH] = 0;
	regs[E1000_RDT] = RX_RING_SIZE - 1;
	regs[E1000_RDLEN] = sizeof(rx_ring);
	// Strip CRC because that's what the grade script expects
	regs[E1000_RCTL] = E1000_RCTL_EN | E1000_RCTL_BAM | E1000_RCTL_SZ_2048
		| E1000_RCTL_SECRC;

        // ask e1000 for receive interrupts.
        regs[E1000_RDTR] = 0; // interrupt after every received packet (no timer)
        regs[E1000_RADV] = 0; // interrupt after every packet (no timer)
        //regs[E1000_RSRPD] = 1; // small packet size
        regs[E1000_IMS] = (1 << 7); // RXT0 -- RX timer expiry
        //regs[E1000_IMS] = (1 << 16); // SRPD -- RX small packet
}

// convert host byte order to network byte order,
// for a 16-bit int.
// network byte order is big-endian, but
// RISC-V is little-endian.
uint16
htons(uint16 x)
{
  return ((x & 0xff) << 8) | ((x >> 8) & 0xff);
}
#define ntohs htons

// convert network byte order to host byte order,
// for a 32-bit int.
uint32
ntohl(uint32 x)
{
  uint32 y =
    ((x & 0xff) << 24) |
    ((x & 0xff00) << 8) |
    ((x & 0xff0000) >> 8) |
    ((x & 0xff000000) >> 24);
  return y;
}
#define htonl ntohl

// This code is lifted from FreeBSD's ping.c,
// and is copyright by the Regents of the University
// of California.
unsigned short
in_cksum(const unsigned char *addr, int len)
{
  int nleft = len;
  const unsigned short *w = (const unsigned short *)addr;
  unsigned int sum = 0;
  unsigned short answer = 0;

  /*
   * Our algorithm is simple, using a 32 bit accumulator (sum), we add
   * sequential 16 bit words to it, and at the end, fold back all the
   * carry bits from the top 16 bits into the lower 16 bits.
   */
  while (nleft > 1)  {
    sum += *w++;
    nleft -= 2;
  }

  /* mop up an odd byte, if necessary */
  if (nleft == 1) {
    *(unsigned char *)(&answer) = *(const unsigned char *)w ;
    sum += answer;
  }

  /* add back carry outs from top 16 bits to low 16 bits */
  sum = (sum & 0xffff) + (sum >> 16);
  sum += (sum >> 16);
  /* guaranteed now that the lower 16 bits of sum are correct */

  answer = ~sum;              /* truncate to 16 bits */
  return answer;
}

struct ip {
  uint8  ip_vhl;         /* version << 4 | header length >> 2 */
  uint8  ip_tos;         /* type of service */
  uint16 ip_len;         /* total length */
  uint16 ip_id;          /* identification */
  uint16 ip_off;         /* fragment offset field */
  uint8  ip_ttl;         /* time to live */
  uint8  ip_p;           /* protocol */
  uint16 ip_sum;         /* checksum */
  uint32 ip_src, ip_dst;
};

struct udp {
  uint16 sport; // source port
  uint16 dport; // destination port
  uint16 ulen;  // length, including udp header, not including IP header
  uint16 sum;   // checksum
};

struct arp {
  uint16 hrd; // format of hardware address
  uint16 pro; // format of protocol address
  uint8  hln; // length of hardware address
  uint8  pln; // length of protocol address
  uint16 op;  // operation

  char   sha[6]; // sender hardware address
  uint32 sip;    // sender IP address
  char   tha[6]; // target hardware address
  uint32 tip;    // target IP address
} __attribute__((packed));

int
udp_send(uint32 dst, uint16 dport, uint16 sport, const char *payload, int paylen)
{
  int framelen = 14 + sizeof(struct ip) + sizeof(struct udp) + paylen;
  if(framelen > PGSIZE)
    return -1;
  
  char *buf = kalloc();
  if(buf == 0)
    panic("udp_send kalloc");

  memset(buf, 0, framelen);
  uint16 ethertype = htons(0x0800); // ETHERTYPE_IP
  memmove(buf+12, &ethertype, 2);

  uint16 len = sizeof(struct ip) + sizeof(struct udp) + paylen;

  // fake IP header for UDP checksum.
  struct ip ip;
  memset(&ip, 0, sizeof(ip));
  ip.ip_vhl = 0;
  ip.ip_p = 17; // IPPROTO_UDP
  ip.ip_len = htons(len - sizeof(ip));
  ip.ip_src = 0x0100007f; // from 127.0.0.1 (localhost)
  ip.ip_dst = htonl(dst);
  memmove(buf+14, &ip, sizeof(ip));

  // UDP header.
  struct udp udp;
  memset(&udp, 0, sizeof(udp));
  udp.sport = htons(sport); // source port number
  udp.dport = htons(dport); // destination port
  udp.ulen = htons(len - sizeof(ip));
  memmove(buf+14+sizeof(ip), &udp, sizeof(udp));
  memmove(buf+14+sizeof(ip)+sizeof(udp), payload, paylen);
  udp.sum = in_cksum((unsigned char *)buf+14, sizeof(ip) + sizeof(udp) + paylen);
  memmove(buf+14+sizeof(ip), &udp, sizeof(udp));

  // now the real IP header.
  ip.ip_vhl = (4 << 4) | (20 >> 2);
  ip.ip_len = htons(len);
  ip.ip_ttl = 100;
  ip.ip_sum = in_cksum((unsigned char *)&ip, sizeof(ip));
  memmove(buf+14, &ip, sizeof(ip));

  // qemu checks the headers for validity, but the only
  // fields it really uses are ip.ip_dst and udp.dport,
  // which it passes to sendto().

  e1000_transmit(buf, framelen);

  kfree(buf);

  return 0;
}

int
e1000_transmit(const char *buf, unsigned int len)
{
  if (!regs || len > DATA_MAX)
    return -1;

  acquire(&e1000_lock);
        
  int tail = regs[E1000_TDT];

  // [E1000 3.3.3.2] Check if this descriptor is done.
  // According to [E1000 13.4.39], using TDH for this is not
  // reliable.
  if (!(tx_ring[tail].status & E1000_TXD_STAT_DD)) {
    release(&e1000_lock);
    printf("TX ring overflow\n");
    return 0;
  }
  
  // Fill in the next descriptor
  memmove(tx_data[tail], buf, len);
  tx_ring[tail].length = len;
  tx_ring[tail].status = 0;
  // Set EOP to actually send this packet.  Set RS to get DD
  // status bit when sent.
  tx_ring[tail].cmd = E1000_TXD_CMD_EOP | E1000_TXD_CMD_RS;
  
  __sync_synchronize();
  
  // Move the tail pointer
  regs[E1000_TDT] = (tail + 1) % TX_RING_SIZE;

  release(&e1000_lock);
  
  printf("sent a packet\n");
  
  return 0;
}

// inbuf is an ethernet frame containing an ARP packet.
// parse it, and respond if it is a query for us.
// the point is that qemu's slirp requires us to
// respond to an ARP query for IP address 127.0.0.1.
// returns 1 if it was an ARP packet, 0 otherwise.
int 
handle_arp(char *inbuf, int inlen)
{
  if(inlen < 14 + sizeof(struct arp))
    return 0;

  uint16 ethertype;
  memmove(&ethertype, inbuf+12, sizeof(ethertype));
  ethertype = ntohs(ethertype);

  if(ethertype != 0x0806){ // ETHERTYPE_ARP
    return 0;
  }

  struct arp arp;
  memmove(&arp, inbuf + 14, sizeof(struct arp));

  int op = ntohs(arp.op);
  if(op == 1){ // request
    uint32 sip = ntohl(arp.sip); // sender's IP address (qemu's slirp)
    uint32 tip = ntohl(arp.tip); // target IP address (us)
    if(arp.pln == 4 && tip == 0x7f000001){
      // qemu's slirp is asking for our ethernet address,
      // which is 52:54:00:12:34:56
      char myeth[6] = { 0x52, 0x54, 0x00, 0x12, 0x34, 0x56 };
      char buf[14 + sizeof(struct arp)];
      memset(buf, 0, sizeof(buf));
      memmove(buf+0, inbuf+6, 6); // destination ethernet address
      memmove(buf+6, myeth, 6);   // source ethernet address
      *(uint16*)(buf+12) = htons(0x0806); // ETHERTYPE_ARP
      arp.op = htons(2); // ARPOP_REPLY
      memmove(arp.sha, myeth, 6); // sender's ethernet address
      arp.sip = htonl(tip); // sender's IP address
      memmove(arp.tha, inbuf+6, 6); // target's ethernet address
      arp.tip = htonl(sip); // target's IP address
      memmove(buf+14, &arp, sizeof(arp));
      e1000_transmit(buf, sizeof(buf));
    }
  }

  return 1;
}

// parse an incoming eth/IP/UDP packet.
// inbuf[] should start with a 14-byte ethernet header.
// returns -1 if there was an error, or it isn't a UDP packet.
// if OK, fills in src, sport, dport,
// copies payload to out,
// returns number of payload bytes.
int
parse_udp(char *inbuf, int inlen, char *out, int outmax,
          uint32 *src, uint16 *sport, uint16 *dport)
{
  if(inlen < 14 + sizeof(struct ip) + sizeof(struct udp))
    return -1;

  uint16 ethertype;
  memmove(&ethertype, inbuf+12, sizeof(ethertype));
  ethertype = ntohs(ethertype);

  if(ethertype != 0x0800){ // ETHERTYPE_IP
    printf("unexpected ethertype %x\n", ethertype);
    return -1;
  }

  struct ip ip;
  memmove(&ip, inbuf+14, sizeof(ip));
  if(ip.ip_p != 17) // IPPROTO_UDP
    return -1;

  // XXX IP checksum

  // XXX IP header length

  struct udp udp;
  memmove(&udp, inbuf+14+sizeof(ip), sizeof(udp));

  // XXX UDP checksum

  int iplen = ntohs(ip.ip_len);
  int ulen = ntohs(udp.ulen);
  if(iplen != ulen + sizeof(ip))
    return -1;
  if(ulen > inlen - 14 - sizeof(udp) - sizeof(ip))
    return -1;

  int paylen = ulen - sizeof(udp);
  if(paylen > outmax)
    return -1;

  // looks good
  *src = ntohl(ip.ip_src);
  *sport = ntohs(udp.sport);
  *dport = ntohs(udp.dport);
  memmove(out, inbuf + 14 + sizeof(ip) + sizeof(udp), paylen);
  return paylen;
}

void
e1000_intr(void)
{
  printf("e1000_intr\n");

  acquire(&e1000_lock);

  // XXX tx complete, for waiting sender when ring is full?

  // tell the e1000 we've seen this interrupt;
  // without this the e1000 won't raise any
  // further interrupts.
  regs[E1000_ICR];

  wakeup(&e1000_lock);

  release(&e1000_lock);
}

// wait for a UDP packet to arrive, return it to the user process.
// while waiting, process ARP requests.
int
udp_recv(uint32 *srcp, uint16 *sportp, uint16 *dportp, char *payload, int paymax)
{
  while(1){
    int tail;
    
    acquire(&e1000_lock);
  
    // any packets waiting in the receive DMA ring?
    // RDT points to one before the place the
    // driver should next look.
    while(1){
      tail = (regs[E1000_RDT] + 1) % RX_RING_SIZE;

      if (rx_ring[tail].status & E1000_RXD_STAT_DD)
        break;

      // there's no packet in the next descriptor;
      // wait for an interrupt.
      sleep(&e1000_lock, &e1000_lock);
    }

    __sync_synchronize();
    int len = rx_ring[tail].length;
    if(len > PGSIZE)
      panic("e1000 len");
    char *buf = kalloc();
    if(buf == 0)
      panic("e1000 kalloc");
    memmove(buf, rx_data[tail], len);

    rx_ring[tail].status = 0;
    __sync_synchronize();
  
    // Move the tail pointer
    regs[E1000_RDT] = tail % RX_RING_SIZE;

    release(&e1000_lock);

    printf("e1000_recv got pkt index=%d len=%d\n", tail, len);

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
