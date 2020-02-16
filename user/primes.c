#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

// redirect stdin(0)/stdout(1) to pipe
void 
redirect(int dir, int pd[])
{
  close(dir);
  dup(pd[dir]);
  close(pd[0]);
  close(pd[1]);
}

void
sieve()
{
  int n, p, pd[2];

  if(!read(0, &n, sizeof n)){
    return;
  }

  pipe(pd);
  printf("prime %d\n", n);

  if(fork()){
    // parent
    redirect(1, pd);
    while(read(0, &p, sizeof p)){
      if(p % n){
        write(1, &p, sizeof p);
      }
    }
    close(1);
    wait();
  } else {
    redirect(0, pd);
    sieve();
  }
}

int
main(int argc, char *argv[])
{
  int pd[2];
  pipe(pd);

  if(fork()){
    // parent
    redirect(1, pd);
    for(int i = 2; i < 36; i++){
      write(1, &i, sizeof i);
    }
    // explicitly close pipe
    close(1);
    // wait for child proces to finish
    // if not, parent may finish before child
    // thus mess shell's '$' prompt
    wait();
  } else {
    redirect(0, pd);
    sieve();
  }

  exit();
}
