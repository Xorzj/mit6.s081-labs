#include "kernel/types.h"
#include "user/user.h"
int main(int argc, char* argv[]) {
  int p[2];
  int q[2];
  pipe(p);
  pipe(q);
  uint8 c = '?';
  if(fork() == 0){
    char receive;
    close(p[1]);
    close(q[0]);
    read(p[0],&receive,1);
    printf("%d: received ping\n",getpid());
    write(q[1],&receive,1);
    close(p[0]);
    close(q[1]);
    exit(0);
  }else{
    char receive;
    close(p[0]);
    close(q[1]);
    write(p[1],&c,1);
    read(q[0],&receive,1);
    printf("%d: received pong\n",getpid());
    wait(0);
    close(p[1]);
    close(q[0]);
    exit(0);
  }
  exit(0);
}