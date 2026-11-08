#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
  if (argc < 3) {
    fprintf(2, "Usage: nice <priority 0-19> <command> [args]\n");
    exit(1);
  }
  int prio = atoi(argv[1]);
  if (nice(prio) < 0) {
    fprintf(2, "nice: syscall failed\n");
    exit(1);
  }
  exec(argv[2], &argv[2]);
  fprintf(2, "nice: exec failed\n");
  exit(1);
}
