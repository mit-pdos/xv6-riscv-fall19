#include "kernel/param.h"
#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/riscv.h"
#include "kernel/fcntl.h"
#include "kernel/spinlock.h"
#include "kernel/sleeplock.h"
#include "kernel/fs.h"
#include "kernel/file.h"
#include "user/user.h"

void test0();

int
main(int argc, char *argv[])
{
  test0();
  exit();
}

void test0()
{
  struct stat st;
  
  printf("test0 start\n");

  mknod("disk1", DISK, 1);

  if (stat("/m/crashf", &st) == 0) {
    printf("stat /m/crashf succeeded\n");
    exit();
  }

  if (mount("/disk1", "/m") < 0) {
    printf("mount failed\n");
    exit();
  }    

  if (stat("/m/crashf", &st) < 0) {
    printf("stat /m/crashf failed\n");
    exit();
  }

  if (minor(st.dev) != 1) {
    printf("stat wrong minor %d\n", minor(st.dev));
    exit();
  }
  
  printf("test0 ok\n");
}
