#include "kernel/types.h"
#include "user/user.h"
void new_proc(int read_pipe) {
  int prime;
  read(read_pipe, &prime, 4);
  printf("prime %d\n", prime);
  int q[2];
  pipe(q);
  int num;
  uint8 exist_nums = 0;
  while (read(read_pipe, &num, 4) != 0) {
    // printf("DEBUG: %d\n", num);
    if (num % prime == 0) continue;
    exist_nums = 1;
    write(q[1], &num, 4);
  }
  close(read_pipe);
  close(q[1]);
  if (exist_nums) {
    if (fork() == 0) {
      close(q[1]);
      new_proc(q[0]);
    } else {
      close(q[0]);
      wait(0);
    }
  }
  exit(0);
}
int main(int argc, char* argv[]) {
  int i = 0;
  const int N = 35;
  int p[2];
  pipe(p);
  if (fork() == 0) {
    close(p[1]);
    new_proc(p[0]);
    wait(0);
    close(p[0]);
  } else {
    close(p[0]);
    for (i = 2; i <= N; i++) {
      write(p[1], &i, 4);
    }
    close(p[1]);
    wait(0);
  }
  exit(0);
}