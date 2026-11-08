#include "kernel/types.h"
#include "user/user.h"

int main(void) {
  struct sysinfo info;
  if (sysinfo(&info) < 0) {
    fprintf(2, "sysinfo: syscall failed\n");
    exit(1);
  }
  printf("Free memory : %d MB (%d KB)\n",
         (int)(info.freemem / (1024*1024)),
         (int)(info.freemem / 1024));
  printf("Processes   : %d\n", info.nproc);
  printf("Uptime      : %d ticks (~%d seconds)\n",
         info.uptime, info.uptime / 100);
  exit(0);
}
