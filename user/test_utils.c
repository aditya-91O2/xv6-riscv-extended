#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fcntl.h"

int main(void) {
  // Test touch
  int pid = fork();
  if (pid == 0) {
    char *argv[] = { "touch", "tfile", 0 };
    exec("touch", argv);
    exit(1);
  }
  int s;
  wait(&s);

  int fd = open("tfile", O_RDWR);
  if (fd < 0) {
    printf("test_utils: touch failed\n");
    exit(1);
  }
  write(fd, "line1\nline2\nline3\nline4\nline5\n", 30);
  close(fd);

  // Test cp
  pid = fork();
  if (pid == 0) {
    char *argv[] = { "cp", "tfile", "tfile2", 0 };
    exec("cp", argv);
    exit(1);
  }
  wait(&s);

  int fd2 = open("tfile2", 0);
  if (fd2 < 0) {
    printf("test_utils: cp failed\n");
    exit(1);
  }
  char buf[64];
  int n = read(fd2, buf, sizeof(buf) - 1);
  close(fd2);
  if (n != 30) {
    printf("test_utils: cp content size mismatch\n");
    exit(1);
  }

  // Cleanup
  unlink("tfile");
  unlink("tfile2");

  printf("test_utils passed!\n");
  exit(0);
}
