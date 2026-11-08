#include "kernel/types.h"
#include "user/user.h"

#define MAXPROCS 64

static const char *states[] = {
  "UNUSED  ", "USED    ", "SLEEPING", "RUNNABLE", "RUNNING ", "ZOMBIE  "
};

int main(void) {
  struct procinfo procs[MAXPROCS];
  int n = getprocs(procs, MAXPROCS);
  if (n < 0) {
    fprintf(2, "ps: getprocs failed\n");
    exit(1);
  }
  printf("PID\tPPID\tSTATE   \tMEM(KB)\tNAME\n");
  for (int i = 0; i < n; i++) {
    const char *st = (procs[i].state >= 0 && procs[i].state < 6)
                      ? states[procs[i].state] : "UNKNOWN ";
    printf("%d\t%d\t%s\t%d\t%s\n",
           procs[i].pid, procs[i].ppid, st,
           (int)(procs[i].sz / 1024),
           procs[i].name);
  }
  exit(0);
}
