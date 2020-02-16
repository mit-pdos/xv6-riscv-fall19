#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int parent_fd[2], child_fd[2];
  char buffer[64];

  pipe(parent_fd);
  pipe(child_fd);

  int pid = fork();
  if(pid == 0) {
    // child process
    read(parent_fd[0], buffer, sizeof buffer);
    printf("%d: received %s\n", getpid(), buffer);
    write(child_fd[1], "pong", strlen("pong"));
    exit();
  }

  // parent process
  write(parent_fd[1], "ping", strlen("ping"));
  read(child_fd[0], buffer, sizeof buffer);
  printf("%d: received %s\n", getpid(), buffer);

  exit();
}
