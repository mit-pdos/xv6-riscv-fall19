//
// utilities for IP, UDP, and ARP.
//

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

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

// an IP packet header (comes after an Ethernet header).
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

// a UDP packet header (comes after an IP header).
struct udp {
  uint16 sport; // source port
  uint16 dport; // destination port
  uint16 ulen;  // length, including udp header, not including IP header
  uint16 sum;   // checksum
};

// an ARP packet (comes after an Ethernet header).
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

// call this when an ARP packet arrives from the E1000.
// inbuf is the ethernet frame containing an ARP packet.
// responds if it is a query for us.
// the point is that qemu's slirp requires us to
// respond to an ARP query for IP address 10.0.2.15.
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

  // 10.0.2.15 is qemu's idea of the guest's IP address.
  uint32 our_ip = (10 << 24) | (0 << 16) | (2 << 8) | (15 << 0);

  int op = ntohs(arp.op);
  if(op == 1){ // request
    uint32 sip = ntohl(arp.sip); // sender's IP address (qemu's slirp)
    uint32 tip = ntohl(arp.tip); // target IP address (us)
    if(arp.pln == 4 && tip == our_ip){
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

// parse an eth/IP/UDP packet.
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

  // should check IP checksum ...

  // should check IP header length ...

  struct udp udp;
  memmove(&udp, inbuf+14+sizeof(ip), sizeof(udp));

  // should check UDP checksum ...

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

// format an ethernet frame containing a UDP packet,
// with ethernet header, IP header, UDP header,
// and payload.
// returns the ethernet frame length,
// or -1 for an error.
//
// qemu checks the headers for validity, but the only
// fields it really uses are ip.ip_dst and udp.dport,
// which it passes to the host O/S's sendto().
int
format_udp(char *buf, int buflen,
           uint32 dst, uint16 dport, uint32 src, uint16 sport,
           const char *payload, int paylen)
{
  int framelen = 14 + sizeof(struct ip) + sizeof(struct udp) + paylen;
  if(framelen > buflen)
    return -1;

  memset(buf, 0, framelen);
  uint16 ethertype = htons(0x0800); // ETHERTYPE_IP
  memmove(buf+12, &ethertype, 2);

  uint16 len = sizeof(struct ip) + sizeof(struct udp) + paylen;

  // pseudo IP header for UDP checksum.
  struct ip ip;
  memset(&ip, 0, sizeof(ip));
  ip.ip_vhl = 0;
  ip.ip_p = 17; // IPPROTO_UDP
  ip.ip_len = htons(len - sizeof(ip));
  ip.ip_src = htonl(src);
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

  return framelen;
}
