#include "kernel/param.h"
#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/riscv.h"
#include "kernel/memlayout.h"
#include "user/user.h"

#define NCHILD 2
#define N 100000

void test0();
void test1();

int
main(int argc, char *argv[])
{
  test0();
  test1();
  exit();
}

void test0()
{
  void *a, *a1;
  printf("start test0\n");  
  int n = ntas();
  for(int i = 0; i < NCHILD; i++){
    int pid = fork();
    if(pid < 0){
      printf("fork failed");
      exit();
    }
    if(pid == 0){
      for(i = 0; i < N; i++) {
        a = sbrk(4096);
        if(a == (char*)0xffffffffffffffffL){
          break;
        }
        *(int *)(a+4) = 1;
        a1 = sbrk(-4096);
        if (a1 != a + 4096) {
          printf("wrong sbrk\n");
          exit();
        }
      }
      exit();
    }
  }

  for(int i = 0; i < NCHILD; i++){
    wait();
  }
  int t = ntas();
  printf("test0 done: #test-and-sets = %d\n", t - n);
}

// Run system out of memory and count tot memory allocated
void test1()
{
  void *a;
  int pipes[NCHILD];
  int tot = 0;
  char buf[1];
  
  printf("start test1\n");  
  for(int i = 0; i < NCHILD; i++){
    int fds[2];
    if(pipe(fds) != 0){
      printf("pipe() failed\n");
      exit();
    }
    int pid = fork();
    if(pid < 0){
      printf("fork failed");
      exit();
    }
    if(pid == 0){
      close(fds[0]);
      for(i = 0; i < N; i++) {
        a = sbrk(PGSIZE);
        if(a == (char*)0xffffffffffffffffL){
          break;
        }
        *(int *)(a+4) = 1;
        if (write(fds[1], "x", 1) != 1) {
          printf("write failed");
          exit();
        }
      }
      exit();
    } else {
      close(fds[1]);
      pipes[i] = fds[0];
    }
  }
  int stop = 0;
  while (!stop) {
    stop = 1;
    for(int i = 0; i < NCHILD; i++){
      if (read(pipes[i], buf, 1) == 1) {
        tot += 1;
        stop = 0;
      }
    }
  }
  int n = (PHYSTOP-KERNBASE)/PGSIZE;
  printf("total allocated number of pages: %d (out of %d)\n", tot, n);
  if(n - tot > 1000) {
    printf("test1 failed: cannot allocate enough memory\n");
    exit();
  }
  printf("test1 done\n");
}

