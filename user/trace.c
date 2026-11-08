#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
  if (argc < 3) {
    fprintf(2, "Usage: trace <mask> <command> [args]\n");
    fprintf(2, "  mask is a bitmask of syscall numbers\n");
    fprintf(2, "  e.g.: trace 32 grep hello README\n");
    exit(1);
  }
  int mask = atoi(argv[1]);
  if (trace(mask) < 0) {
    fprintf(2, "trace: syscall failed\n");
    exit(1);
  }
  exec(argv[2], &argv[2]);
  fprintf(2, "trace: exec %s failed\n", argv[2]);
  exit(1);
}
