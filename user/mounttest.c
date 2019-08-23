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
void test1();
void test2();
void test3();
void test4();

int
main(int argc, char *argv[])
{
  test0();
  test1();
  test2();
  test3();
  test4();
  exit();
}

void test0()
{
  int fd;
  char buf[4];
  struct stat st;
  
  printf(1, "test0 start\n");

  mknod("disk1", DISK, 1);
  mkdir("/m");
  
  if (mount("/disk1", "/m") < 0) {
    printf(1, "mount failed\n");
    exit();
  }    

  if (stat("/m", &st) < 0) {
    printf(1, "stat /m failed\n");
    exit();
  }

  if (st.ino != 1 || minor(st.dev) != 1) {
    printf(1, "stat wrong inum/minor %d %d\n", st.ino, minor(st.dev));
    exit();
  }
  
  if ((fd = open("/m/README", O_RDONLY)) < 0) {
    printf(1, "open read failed\n");
    exit();
  }
  if (read(fd, buf, sizeof(buf)-1) != sizeof(buf)-1) {
    printf(1, "read failed\n");
    exit();
  }
  if (strcmp("xv6", buf) != 0) {
    printf(1, "read failed\n", buf);
  }
  close(fd);
  
  if ((fd = open("/m/a", O_CREATE|O_WRONLY)) < 0) {
    printf(1, "open write failed\n");
    exit();
  }
  
  if (write(fd, buf, sizeof(buf)) != sizeof(buf)) {
    printf(1, "write failed\n");
    exit();
  }

  close(fd);

  if (stat("/m/a", &st) < 0) {
    printf(1, "stat /m/a failed\n");
    exit();
  }

  if (minor(st.dev) != 1) {
    printf(1, "stat wrong minor %d\n", minor(st.dev));
    exit();
  }


  if (link("m/a", "/a") == 0) {
    printf(1, "link m/a a succeeded\n");
    exit();
  }

  if (unlink("m/a") < 0) {
    printf(1, "unlink m/a failed\n");
    exit();
  }

  if (chdir("/m") < 0) {
    printf(1, "chdir /m failed\n");
    exit();
  }

  if (stat(".", &st) < 0) {
    printf(1, "stat . failed\n");
    exit();
  }

  if (st.ino != 1 || minor(st.dev) != 1) {
    printf(1, "stat wrong inum/minor %d %d\n", st.ino, minor(st.dev));
    exit();
  }

  if (chdir("..") < 0) {
    printf(1, "chdir .. failed\n");
    exit();
  }

  if (stat(".", &st) < 0) {
    printf(1, "stat . failed\n");
    exit();
  }

  if (st.ino == 1 && minor(st.dev) == 0) {
    printf(1, "stat wrong inum/minor %d %d\n", st.ino, minor(st.dev));
    exit();
  }

  printf(1, "test0 done\n");
}

// depends on test0
void test1() {
  struct stat st;
  int fd;
  int i;
  
  printf(1, "test1 start\n");

  if (mount("/disk1", "/m") == 0) {
    printf(1, "mount should fail\n");
    exit();
  }    

  if (umount("/m") < 0) {
    printf(1, "umount /m failed\n");
    exit();
  }    

  if (umount("/m") == 0) {
    printf(1, "umount /m succeeded\n");
    exit();
  }    

  if (umount("/") == 0) {
    printf(1, "umount / succeeded\n");
    exit();
  }    

  if (stat("/m", &st) < 0) {
    printf(1, "stat /m failed\n");
    exit();
  }

  if (minor(st.dev) != 0) {
    printf(1, "stat wrong inum/dev %d %d\n", st.ino, minor(st.dev));
    exit();
  }

  // many mounts and umounts
  for (i = 0; i < 100; i++) {
    if (mount("/disk1", "/m") < 0) {
      printf(1, "mount /m should succeed\n");
      exit();
    }    

    if (umount("/m") < 0) {
      printf(1, "umount /m failed\n");
      exit();
    }
  }

  if (mount("/disk1", "/m") < 0) {
    printf(1, "mount /m should succeed\n");
    exit();
  }    

  if ((fd = open("/m/README", O_RDONLY)) < 0) {
    printf(1, "open read failed\n");
    exit();
  }

  if (umount("/m") == 0) {
    printf(1, "umount /m succeeded\n");
    exit();
  }

  close(fd);
  
  if (umount("/m") < 0) {
    printf(1, "final umount failed\n");
    exit();
  }

  printf(1, "test1 done\n");
}


#define NPID 4
#define NOP 100

// try to trigger races/deadlocks in namex; it is helpful to add
// sleepticks(1) in if(ip->type != T_DIR) branch in namei, so that you
// will observe some more reliably.
void test2() {
  int pid[NPID];
  int fd;
  int i;
  char buf[1];

  printf(1, "test2\n");

  mkdir("/m");
  
  if (mount("/disk1", "/m") < 0) {
      printf(1, "mount failed\n");
      exit();
  }    

  for (i = 0; i < NPID; i++) {
    if ((pid[i] = fork()) < 0) {
      printf(1, "fork failed\n");
      exit();
    }
    if (pid[i] == 0) {
      while(1) {
        if ((fd = open("/m/b/c", O_RDONLY)) >= 0) {
          close(fd);
        }
      }
    }
  }
  for (i = 0; i < NOP; i++) {
    if ((fd = open("/m/b", O_CREATE|O_WRONLY)) < 0) {
      printf(1, "open write failed");
      exit();
    }
    if (unlink("/m/b") < 0) {
      printf(1, "unlink failed\n");
      exit();
    }
    if (write(fd, buf, sizeof(buf)) != sizeof(buf)) {
      printf(1, "write failed\n");
      exit();
    }
    close(fd);
  }
  for (i = 0; i < NPID; i++) {
    kill(pid[i]);
    wait();
  }
  if (umount("/m") < 0) {
    printf(1, "umount failed\n");
    exit();
  }    

  printf(1, "test2 ok\n");
}


// Mount/unmount concurrently with creating files on the mounted fs
void test3() {
  int pid[NPID];
  int fd;
  int i;
  char buf[1];

  printf(1, "test3\n");

  mkdir("/m");
  for (i = 0; i < NPID; i++) {
    if ((pid[i] = fork()) < 0) {
      printf(1, "fork failed\n");
      exit();
    }
    if (pid[i] == 0) {
      while(1) {
        if ((fd = open("/m/b", O_CREATE|O_WRONLY)) < 0) {
          printf(1, "open write failed");
          exit();
        }
        // may file, because fs was mounted/unmounted
        unlink("/m/b");
        if (write(fd, buf, sizeof(buf)) != sizeof(buf)) {
          printf(1, "write failed\n");
          exit();
        }
        close(fd);
        sleep(1);
      }
    }
  }
  for (i = 0; i < NOP; i++) {
    if (mount("/disk1", "/m") < 0) {
      printf(1, "mount failed\n");
      exit();
    }    
    while (umount("/m") < 0) {
      printf(1, "umount failed; try again %d\n", i);
    }    
  }
  for (i = 0; i < NPID; i++) {
    kill(pid[i]);
    wait();
  }
  printf(1, "test3 ok\n");
}

void
test4()
{
  printf(1, "test4\n");

  mknod("disk1", DISK, 1);
  mkdir("/m");
  if (mount("/disk1", "/m") < 0) {
      printf(1, "mount failed\n");
      exit();
  }
  crash("/m/crashf", 1);
}
