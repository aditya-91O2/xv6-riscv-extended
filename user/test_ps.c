#include "kernel/types.h"
#include "user/user.h"

int main(void) {
  struct procinfo procs[16];
  int n = getprocs(procs, 16);
  if (n <= 0) {
    printf("test_ps: getprocs failed\n");
    exit(1);
  }
  printf("test_ps: retrieved %d active processes\n", n);
  printf("test_ps passed!\n");
  exit(0);
}
