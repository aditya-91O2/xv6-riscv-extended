#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"

#define MAXLINES 128
#define LINELEN  256

void tail(int fd, int n) {
  static char lines[MAXLINES][LINELEN];
  int lens[MAXLINES];
  int total = 0;
  char buf[1];
  int pos = 0;
  int col = 0;

  // suppress unused warning
  (void)pos;

  while (read(fd, buf, 1) == 1) {
    int idx = total % MAXLINES;
    if (col < LINELEN - 1)
      lines[idx][col++] = buf[0];
    if (buf[0] == '\n') {
      lines[idx][col] = '\0';
      lens[idx] = col;
      col = 0;
      total++;
    }
  }
  // flush last partial line
  if (col > 0) {
    int idx = total % MAXLINES;
    lines[idx][col] = '\0';
    lens[idx] = col;
    total++;
  }

  int start = (total > n) ? total - n : 0;
  for (int i = start; i < total; i++) {
    write(1, lines[i % MAXLINES], lens[i % MAXLINES]);
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
    tail(0, n);
  } else {
    for (; i < argc; i++) {
      int fd = open_or_locate("tail", argv[i], 0);
      if (fd < 0) { exit(1); }
      tail(fd, n);
      close(fd);
    }
  }
  exit(0);
}
