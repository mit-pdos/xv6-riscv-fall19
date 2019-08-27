#include "kernel/fcntl.h"
#include "kernel/param.h"
#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/riscv.h"
#include "user/user.h"

#define NCHILD 3
#define N 1000
#define BIG 100

void test0();
void test1();

int
main(int argc, char *argv[])
{
  test0();
  test1();
  exit();
}

void
createfile(char *file, int nblock)
{
  int fd;
  char buf[512];
  int i;
  
  fd = open(file, O_CREATE | O_RDWR);
  if(fd < 0){
    printf("test0 create %s failed\n", file);
    exit();
  }
  for(i = 0; i < nblock; i++) {
    if(write(fd, buf, sizeof(buf)) != sizeof(buf)) {
      printf("write %s failed\n", file);
    }
  }
  close(fd);
}

void
readfile(char *file, int nblock)
{
  char buf[512];
  int fd;
  int i;
  
  if ((fd = open(file, O_RDONLY)) < 0) {
    printf("test0 open %s failed\n", file);
    exit();
  }
  for (i = 0; i < nblock; i++) {
    if(read(fd, buf, sizeof(buf)) != sizeof(buf)) {
      printf("read %s failed for block %d (%d)\n", file, i, nblock);
      exit();
    }
  }
  close(fd);
}

void
test0()
{
  char file[3];

  file[0] = 'B';
  file[2] = '\0';

  printf("start test0\n");
  int n = ntas();
  for(int i = 0; i < NCHILD; i++){
    file[1] = '0' + i;
    createfile(file, 1);
    int pid = fork();
    if(pid < 0){
      printf("fork failed");
      exit();
    }
    if(pid == 0){
      for (i = 0; i < N; i++) {
        readfile(file, 1);
      }
      unlink(file);
      exit();
    }
  }

  for(int i = 0; i < NCHILD; i++){
    wait();
  }
  printf("test0 done: #test-and-sets: %d\n", ntas() - n);
}

void test1()
{
  char file[3];
  
  printf("start test1\n");
  file[0] = 'B';
  file[2] = '\0';
  for(int i = 0; i < 2; i++){
    file[1] = '0' + i;
    if (i == 0) {
      createfile(file, BIG);
    } else {
      createfile(file, 1);
    }
  }
  for(int i = 0; i < 2; i++){
    file[1] = '0' + i;
    int pid = fork();
    if(pid < 0){
      printf("fork failed");
      exit();
    }
    if(pid == 0){
      if (i==0) {
        for (i = 0; i < N; i++) {
          readfile(file, BIG);
        }
        unlink(file);
        exit();
      } else {
        for (i = 0; i < N; i++) {
          readfile(file, 1);
        }
        unlink(file);
      }
      exit();
    }
  }

  for(int i = 0; i < 2; i++){
    wait();
  }
  printf("test1 done\n");
}
