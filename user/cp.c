#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"

int main(int argc, char *argv[]) {
  if (argc != 3) {
    fprintf(2, "Usage: cp src dst\n");
    exit(1);
  }
  int src = open(argv[1], 0);
  if (src < 0) {
    fprintf(2, "cp: cannot open %s\n", argv[1]);
    exit(1);
  }
  int dst = open(argv[2], O_CREATE | O_WRONLY);
  if (dst < 0) {
    fprintf(2, "cp: cannot create %s\n", argv[2]);
    close(src);
    exit(1);
  }
  char buf[512];
  int n;
  while ((n = read(src, buf, sizeof(buf))) > 0) {
    if (write(dst, buf, n) != n) {
      fprintf(2, "cp: write error\n");
      close(src); close(dst);
      exit(1);
    }
  }
  close(src);
  close(dst);
  exit(0);
}
