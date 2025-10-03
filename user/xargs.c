#include "kernel/param.h"
#include "kernel/types.h"
#include "user/user.h"
const int BUF_SIZE = 512;
int main(int argc, char* argv[]) {
  char buf[BUF_SIZE + 1] = {};
  uint now = 0;
  char* xargv[MAXARG];
  for (int i = 1; i < argc; i++) {
    xargv[i - 1] = argv[i];
  }
  char c;
  while (read(0, &c, 1) > 0) {
    if (c == '\n') {
      buf[now] = 0;
      xargv[argc - 1] = buf;
      xargv[argc] = 0;
      if (fork() == 0) {
        exec(argv[1], xargv);
      } else {
        wait(0);
        now = 0;
      }
    } else {
      buf[now++] = c;
    }
  }
  if (now > 0) {
    buf[now] = 0;
    xargv[argc - 1] = buf;
    xargv[argc] = 0;
    if (fork() == 0) {
      exec(argv[1], xargv);
    } else {
      wait(0);
      now = 0;
    }
  }
  close(0);
  exit(0);
}