#include "kernel/types.h"
#include "user/user.h"

#define N 4096

int main(void) {
  char *p = sbrk(N);
  int i;
  for (i = 0; i < N; i++) p[i] = 'A';

  int pid = fork();
  if (pid == 0) {
    // Child writes to verify COW copy happens without corruption
    for (i = 0; i < N; i++) p[i] = 'B';
    printf("child: buf[0]=%c (expected B)\n", p[0]);
    exit(0);
  } else {
    int s;
    wait(&s);
    printf("parent: buf[0]=%c (expected A)\n", p[0]);
  }
  printf("COW fork test passed!\n");
  exit(0);
}
