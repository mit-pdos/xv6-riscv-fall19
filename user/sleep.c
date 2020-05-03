#include "kernel/syscall.h"
#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "user/user.h"


int
main(int argc, char *argv[])
{
  int time_to_sleep;

  if(argc < 2){
    fprintf(2, "Usage: sleep <time> \n");
    exit();
  }

  time_to_sleep = atoi(argv[1]);

  fprintf(1,"User sleep for %d \n", time_to_sleep);
  
  sleep(time_to_sleep);

  exit();
}

