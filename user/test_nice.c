#include "kernel/types.h"
#include "user/user.h"

int main(void) {
  int pid1, pid2;
  int s;

  // Fork two children with different priorities
  pid1 = fork();
  if (pid1 == 0) {
    nice(1); // high priority
    for (volatile int i = 0; i < 10000000; i++);
    printf("high-priority child done (pid=%d)\n", getpid());
    exit(0);
  }
  pid2 = fork();
  if (pid2 == 0) {
    nice(18); // low priority
    for (volatile int i = 0; i < 10000000; i++);
    printf("low-priority child done (pid=%d)\n", getpid());
    exit(0);
  }
  wait(&s); wait(&s);
  printf("nice test done\n");
  exit(0);
}
