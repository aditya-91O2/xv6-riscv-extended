#include "kernel/types.h"
#include "user/user.h"

int main(void) {
  // Trace SYS_uptime (14 -> 1<<14 = 16384)
  if (trace(1 << 14) < 0) {
    printf("test_trace: trace syscall failed\n");
    exit(1);
  }
  int u = uptime();
  printf("test_trace: uptime returned %d\n", u);
  // Turn off trace
  trace(0);
  printf("test_trace passed!\n");
  exit(0);
}

