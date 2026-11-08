#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fcntl.h"

int main(void) {
  int fd;
  char buf[32];

  // Create a file and a symlink to it
  fd = open("sym_target", O_CREATE | O_RDWR);
  if (fd < 0) { printf("cannot create sym_target\n"); exit(1); }
  write(fd, "hello symlink!\n", 15);
  close(fd);

  if (symlink("sym_target", "sym_link") < 0) {
    printf("symlink() failed\n");
    exit(1);
  }

  // Read through the symlink
  fd = open("sym_link", 0);
  if (fd < 0) { printf("cannot open symlink\n"); exit(1); }
  int n = read(fd, buf, sizeof(buf)-1);
  if (n < 0) { printf("read failed\n"); exit(1); }
  buf[n] = '\0';
  printf("read via symlink: %s", buf);
  close(fd);

  // Clean up
  unlink("sym_link");
  unlink("sym_target");
  printf("symlink test passed!\n");
  exit(0);
}
