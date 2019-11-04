#include "kernel/param.h"
#include "kernel/fcntl.h"
#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/riscv.h"
#include "kernel/fs.h"
#include "user/user.h"

void mmap_test();
void fork_test();
char buf[BSIZE];

#define MAP_FAILED ((char *) -1)

int
main(int argc, char *argv[])
{
  mmap_test();
  fork_test();
  printf("mmaptest: all tests succeeded\n");
  exit(0);
}

char *testname = "???";

void
err(char *why)
{
  printf("mmaptest: %s failed: %s, pid=%d\n", testname, why, getpid());
  exit(1);
}

//
// check the content of the two mapped pages.
//
void
_v1(char *p)
{
  int i;
  for (i = 0; i < PGSIZE*2; i++) {
    if (i < PGSIZE + (PGSIZE/2)) {
      if (p[i] != 'A') {
        printf("mismatch at %d, wanted 'A', got 0x%x\n", i, p[i]);
        err("mismatch");
      }
    } else {
      if (p[i] != 0) {
        printf("mismatch at %d, wanted zero, got 0x%x\n", i, p[i]);
        err("mismatch");
      }
    }
  }
}

//
// create a file to be mapped, containing
// 1.5 pages of 'A' and half a page of zeros.
//
void
makefile(const char *f)
{
  int i;
  int n = PGSIZE/BSIZE;

  unlink(f);
  int fd = open(f, O_WRONLY | O_CREATE);
  if (fd == -1)
    err("open");
  memset(buf, 'A', BSIZE);
  // write 1.5 page
  for (i = 0; i < n + n/2; i++) {
    if (write(fd, buf, BSIZE) != BSIZE)
      err("write 0 makefile");
  }
  if (close(fd) == -1)
    err("close");
}

void
mmap_test(void)
{
  int fd;
  int i;
  const char * const f = "mmap.dur";
  printf("mmap_test\n");
  testname = "mmap_test";

  makefile(f);

  if ((fd = open(f, O_RDONLY)) == -1)
    err("open");
  char *p = mmap(0, PGSIZE*2, PROT_READ, MAP_PRIVATE, fd, 0);
  if (p == MAP_FAILED)
    err("mmap");
  _v1(p);
  if (munmap(p, PGSIZE*2) == -1)
    err("munmap");

  // should be able to map file opened read-only with private writable
  // mapping
  p = mmap(0, PGSIZE*2, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
  if (p == MAP_FAILED)
    err("mmap");
  if (close(fd) == -1)
    err("close");
  _v1(p);
  for (i = 0; i < PGSIZE*2; i++)
    p[i] = 'Z';
  if (munmap(p, PGSIZE*2) == -1)
    err("munmap");

  if ((fd = open(f, O_RDONLY)) == -1)
    err("open");
  p = mmap(0, PGSIZE*3, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (p != MAP_FAILED)
    err("mmap succeeded");
  if (close(fd) == -1)
    err("close");

  if ((fd = open(f, O_RDWR)) == -1)
    err("open");
  p = mmap(0, PGSIZE*3, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (p == MAP_FAILED)
    err("mmap");
  if (close(fd) == -1)
    err("close");

  _v1(p);
  for (i = 0; i < PGSIZE*2; i++)
    p[i] = 'Z';
  if (munmap(p, PGSIZE*2) == -1)
    err("munmap");

  if ((fd = open(f, O_RDWR)) == -1)
    err("open");
  char b;
  for (i = 0; i < PGSIZE + (PGSIZE/2); i++)
    if (read(fd, &b, 1) != 1 || b != 'Z')
      err("read");
  if (close(fd) == -1)
    err("close");

  if (munmap(p+PGSIZE*2, PGSIZE) == -1)
    err("munmap");

  printf("mmap_test ok\n");
}

void
fork_test(void)
{
  int fd;
  int fds[2];
  int pid;
  char buf[1];
  const char * const f = "mmap.dur";
  
  printf("fork_test\n");
  testname = "fork_test";
  
  makefile(f);
  if ((fd = open(f, O_RDONLY)) == -1)
    err("open");
  unlink(f);
  char *p = mmap(0, PGSIZE*2, PROT_READ, MAP_SHARED, fd, 0);
  if (p == MAP_FAILED)
    err("mmap");

  pipe(fds);
  if((pid = fork()) < 0)
    err("fork");
  if (pid == 0) {
    close(fds[0]);
    _v1(p);
    if (write(fds[1], "x", 1) != 1)
      err("write");
    exit(1);
  }
  close(fds[1]);
  if(read(fds[0], buf, sizeof(buf)) != 1)
    err("read");
  wait(0);
  printf("fork_test ok\n");
}

