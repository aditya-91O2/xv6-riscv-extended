#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user/user.h"

int
main(void)
{
  printf("running fast file search & path discovery tests...\n");

  // Create a subfolder and a deeply nested file
  mkdir("/testsearch");
  mkdir("/testsearch/subdir");
  int fd = open("/testsearch/subdir/deepfile.txt", O_CREATE | O_RDWR);
  if (fd < 0) {
    printf("test_find: cannot create deepfile.txt\n");
    exit(1);
  }
  write(fd, "deep file content\n", 18);
  close(fd);

  // Test 1: find_file searches from "/" and discovers the full path
  char path_out[512];
  if (!find_file("/", "deepfile.txt", path_out, sizeof(path_out))) {
    printf("test_find: find_file failed to locate deepfile.txt\n");
    exit(1);
  }
  printf("test_find: search found path: '%s' (OK)\n", path_out);
  if (strcmp(path_out, "/testsearch/subdir/deepfile.txt") != 0) {
    printf("test_find: path mismatch: got '%s'\n", path_out);
    exit(1);
  }

  // Test 2: find_file on non-existent file returns 0
  if (find_file("/", "definitely_not_a_real_file.xyz", path_out, sizeof(path_out))) {
    printf("test_find: falsely found non-existent file\n");
    exit(1);
  }
  printf("test_find: non-existent file correctly returns not found (OK)\n");

  // Test 3: Test open_or_locate on file in another directory vs not exists
  printf("test_find: testing error output for file in another directory vs not exists:\n");
  int failed_fd = open_or_locate("cat", "deepfile.txt", 0);
  if (failed_fd >= 0) {
    printf("test_find: open_or_locate should return -1 for missing local file\n");
    close(failed_fd);
    exit(1);
  }

  int never_fd = open_or_locate("cat", "ghost_file", 0);
  if (never_fd >= 0) {
    close(never_fd);
    exit(1);
  }

  // Cleanup
  unlink("/testsearch/subdir/deepfile.txt");
  unlink("/testsearch/subdir");
  unlink("/testsearch");

  printf("fast file search and locate tests passed!\n");
  exit(0);
}

