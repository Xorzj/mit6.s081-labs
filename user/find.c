#include "kernel/types.h"
#include "kernel/fs.h"
#include "kernel/stat.h"
#include "user/user.h"
char* basename(const char* path) {
  char* p = (char*)path;
  char* q = (char*)path;
  while (*q != 0) {
    if (*q == '/') {
      p = q;
    }
    q++;
  }
  return p + 1;
}
void _find(const char* path, const char* messages) {
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;
  fd = open(path, 0);
  fstat(fd, &st);
  // printf("DEBUG : %d \n", st.type);
  // printf("DEBUG : %s \n", path);
  switch (st.type) {
    case T_FILE:
      // printf("basename : %s \n", path);
      if (strcmp(basename(path), messages) == 0) {
        printf("%s\n", path);
      }
      break;

    case T_DIR:
      strcpy(buf, path);
      int len = strlen(buf);
      p = buf + len;
      *p++ = '/';
      while (read(fd, &de, sizeof(de)) == sizeof(de)) {
        if (de.inum == 0) continue;
        if (strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0) continue;
        memmove(p, de.name, strlen(de.name));
        p[strlen(de.name)] = 0;
        _find(buf, messages);
        *p = 0;
      }
      break;
  }
  close(fd);
}
int main(int argc, char* argv[]) {
  if (argc < 3) {
    fprintf(2, "find address messages ...\n");
    exit(1);
  }
  _find(argv[1], argv[2]);
  exit(0);
}