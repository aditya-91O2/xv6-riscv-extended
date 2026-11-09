#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"

void head(int fd, int n) {
  char buf[512];
  int lines = 0;
  int i, r;
  while (lines < n && (r = read(fd, buf, sizeof(buf))) > 0) {
    for (i = 0; i < r && lines < n; i++) {
      write(1, &buf[i], 1);
      if (buf[i] == '\n') lines++;
    }
  }
}

int main(int argc, char *argv[]) {
  int n = 10;
  int i = 1;
  if (argc > 1 && argv[1][0] == '-') {
    n = atoi(argv[1] + 1);
    i = 2;
  }
  if (i >= argc) {
    head(0, n);
  } else {
    for (; i < argc; i++) {
      int fd = open_or_locate("head", argv[i], 0);
      if (fd < 0) { exit(1); }
      head(fd, n);
      close(fd);
    }
  }
  exit(0);
}
