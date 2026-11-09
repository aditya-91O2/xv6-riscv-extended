#include "kernel/types.h"
#include "user/user.h"

int main(void) {
  struct sysinfo s;
  if (sysinfo(&s) < 0) {
    printf("test_sysinfo: syscall failed\n");
    exit(1);
  }
  if (s.freemem == 0 || s.nproc <= 0) {
    printf("test_sysinfo: invalid metrics\n");
    exit(1);
  }
  printf("test_sysinfo: freemem=%d bytes, nproc=%d, uptime=%d\n",
         (int)s.freemem, s.nproc, s.uptime);
  printf("test_sysinfo passed!\n");
  exit(0);
}

