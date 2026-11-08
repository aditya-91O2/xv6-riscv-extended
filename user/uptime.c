#include "kernel/types.h"
#include "user/user.h"

int
main(void)
{
  int t = uptime();
  printf("uptime: %d ticks (%d seconds)\n", t, t / 100);
  exit(0);
}
