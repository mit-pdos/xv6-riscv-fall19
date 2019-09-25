//
// send a UDP packet to port 3000 on the host (outside of qemu).
//

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  char obuf[4] = { 1, 2, 3, 4 };

  send(0x7f000001, 3000, 2000, obuf, sizeof(obuf));

  uint32 src;
  uint16 sport;
  uint16 dport;
  char ibuf[128];
  int cc = recv(&src, &sport, &dport, ibuf, sizeof(ibuf));
  printf("recv: n=%d src=%x sport=%d dport=%d\n", cc, src, sport, dport);

  exit(0);
}
