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
  exit(0);
}

void test0()
{
  int fd;
  char buf[4];
  struct stat st;
  
  printf("test0 start\n");

  mknod("disk1", DISK, 1);
  mkdir("/m");
  
  if (mount("/disk1", "/m") < 0) {
    printf("mount failed\n");
    exit(-1);
  }    

  if (stat("/m", &st) < 0) {
    printf("stat /m failed\n");
    exit(-1);
  }

  if (st.ino != 1 || minor(st.dev) != 1) {
    printf("stat wrong inum/minor %d %d\n", st.ino, minor(st.dev));
    exit(-1);
  }
  
  if ((fd = open("/m/README", O_RDONLY)) < 0) {
    printf("open read failed\n");
    exit(-1);
  }
  if (read(fd, buf, sizeof(buf)-1) != sizeof(buf)-1) {
    printf("read failed\n");
    exit(-1);
  }
  if (strcmp("xv6", buf) != 0) {
    printf("read failed\n", buf);
  }
  close(fd);
  
  if ((fd = open("/m/a", O_CREATE|O_WRONLY)) < 0) {
    printf("open write failed\n");
    exit(-1);
  }
  
  if (write(fd, buf, sizeof(buf)) != sizeof(buf)) {
    printf("write failed\n");
    exit(-1);
  }

  close(fd);

  if (stat("/m/a", &st) < 0) {
    printf("stat /m/a failed\n");
    exit(-1);
  }

  if (minor(st.dev) != 1) {
    printf("stat wrong minor %d\n", minor(st.dev));
    exit(-1);
  }


  if (link("m/a", "/a") == 0) {
    printf("link m/a a succeeded\n");
    exit(-1);
  }

  if (unlink("m/a") < 0) {
    printf("unlink m/a failed\n");
    exit(-1);
  }

  if (chdir("/m") < 0) {
    printf("chdir /m failed\n");
    exit(-1);
  }

  if (stat(".", &st) < 0) {
    printf("stat . failed\n");
    exit(-1);
  }

  if (st.ino != 1 || minor(st.dev) != 1) {
    printf("stat wrong inum/minor %d %d\n", st.ino, minor(st.dev));
    exit(-1);
  }

  if (chdir("..") < 0) {
    printf("chdir .. failed\n");
    exit(-1);
  }

  if (stat(".", &st) < 0) {
    printf("stat . failed\n");
    exit(-1);
  }

  if (st.ino == 1 && minor(st.dev) == 0) {
    printf("stat wrong inum/minor %d %d\n", st.ino, minor(st.dev));
    exit(-1);
  }

  printf("test0 done\n");
}

// depends on test0
void test1() {
  struct stat st;
  int fd;
  int i;
  
  printf("test1 start\n");

  if (mount("/disk1", "/m") == 0) {
    printf("mount should fail\n");
    exit(-1);
  }    

  if (umount("/m") < 0) {
    printf("umount /m failed\n");
    exit(-1);
  }    

  if (umount("/m") == 0) {
    printf("umount /m succeeded\n");
    exit(-1);
  }    

  if (umount("/") == 0) {
    printf("umount / succeeded\n");
    exit(-1);
  }    

  if (stat("/m", &st) < 0) {
    printf("stat /m failed\n");
    exit(-1);
  }

  if (minor(st.dev) != 0) {
    printf("stat wrong inum/dev %d %d\n", st.ino, minor(st.dev));
    exit(-1);
  }

  // many mounts and umounts
  for (i = 0; i < 100; i++) {
    if (mount("/disk1", "/m") < 0) {
      printf("mount /m should succeed\n");
      exit(-1);
    }    

    if (umount("/m") < 0) {
      printf("umount /m failed\n");
      exit(-1);
    }
  }

  if (mount("/disk1", "/m") < 0) {
    printf("mount /m should succeed\n");
    exit(-1);
  }    

  if ((fd = open("/m/README", O_RDONLY)) < 0) {
    printf("open read failed\n");
    exit(-1);
  }

  if (umount("/m") == 0) {
    printf("umount /m succeeded\n");
    exit(-1);
  }

  close(fd);
  
  if (umount("/m") < 0) {
    printf("final umount failed\n");
    exit(-1);
  }

  printf("test1 done\n");
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

  printf("test2\n");

  mkdir("/m");
  
  if (mount("/disk1", "/m") < 0) {
      printf("mount failed\n");
      exit(-1);
  }    

  for (i = 0; i < NPID; i++) {
    if ((pid[i] = fork()) < 0) {
      printf("fork failed\n");
      exit(-1);
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
      printf("open write failed");
      exit(-1);
    }
    if (unlink("/m/b") < 0) {
      printf("unlink failed\n");
      exit(-1);
    }
    if (write(fd, buf, sizeof(buf)) != sizeof(buf)) {
      printf("write failed\n");
      exit(-1);
    }
    close(fd);
  }
  for (i = 0; i < NPID; i++) {
    kill(pid[i]);
    wait(0);
  }
  if (umount("/m") < 0) {
    printf("umount failed\n");
    exit(-1);
  }    

  printf("test2 ok\n");
}


// Mount/unmount concurrently with creating files on the mounted fs
void test3() {
  int pid[NPID];
  int fd;
  int i;
  char buf[1];

  printf("test3\n");

  mkdir("/m");
  for (i = 0; i < NPID; i++) {
    if ((pid[i] = fork()) < 0) {
      printf("fork failed\n");
      exit(-1);
    }
    if (pid[i] == 0) {
      while(1) {
        if ((fd = open("/m/b", O_CREATE|O_WRONLY)) < 0) {
          printf("open write failed");
          exit(-1);
        }
        // may file, because fs was mounted/unmounted
        unlink("/m/b");
        if (write(fd, buf, sizeof(buf)) != sizeof(buf)) {
          printf("write failed\n");
          exit(-1);
        }
        close(fd);
        sleep(1);
      }
    }
  }
  for (i = 0; i < NOP; i++) {
    if (mount("/disk1", "/m") < 0) {
      printf("mount failed\n");
      exit(-1);
    }    
    while (umount("/m") < 0) {
      printf("umount failed; try again %d\n", i);
    }    
  }
  for (i = 0; i < NPID; i++) {
    kill(pid[i]);
    wait(0);
  }
  printf("test3 ok\n");
}

void
test4()
{
  printf("test4\n");

  mknod("disk1", DISK, 1);
  mkdir("/m");
  if (mount("/disk1", "/m") < 0) {
      printf("mount failed\n");
      exit(-1);
  }
  crash("/m/crashf", 1);
}
