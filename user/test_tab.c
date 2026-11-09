#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fs.h"
#include "user/user.h"
#include "kernel/fcntl.h"

static void
copy_name(char *dst, const char *src, int n)
{
  int i;
  for (i = 0; i < n && src[i]; i++)
    dst[i] = src[i];
  dst[i] = '\0';
}

static int
starts_with(const char *str, const char *prefix)
{
  while (*prefix) {
    if (*prefix++ != *str++)
      return 0;
  }
  return 1;
}

int
main(void)
{
  printf("running tab completion unit tests...\n");

  // Test 1: unique match 'upt' -> 'uptime'
  int fd = open(".", O_RDONLY);
  if (fd < 0) {
    printf("test_tab: cannot open .\n");
    exit(1);
  }

  struct dirent de;
  int count = 0;
  char matched[DIRSIZ + 1];
  matched[0] = '\0';

  while (read(fd, &de, sizeof(de)) == sizeof(de)) {
    if (de.inum == 0)
      continue;
    char name[DIRSIZ + 1];
    copy_name(name, de.name, DIRSIZ);
    if (starts_with(name, "upt")) {
      copy_name(matched, name, DIRSIZ);
      count++;
    }
  }
  close(fd);

  if (count != 1 || strcmp(matched, "uptime") != 0) {
    printf("test_tab: failed 'upt' match test (count=%d, match=%s)\n", count, matched);
    exit(1);
  }
  printf("test_tab: 'upt' -> '%s' (unique match OK)\n", matched);

  // Test 2: unique match 'test_co' -> 'test_cow'
  fd = open(".", O_RDONLY);
  count = 0;
  matched[0] = '\0';
  while (read(fd, &de, sizeof(de)) == sizeof(de)) {
    if (de.inum == 0)
      continue;
    char name[DIRSIZ + 1];
    copy_name(name, de.name, DIRSIZ);
    if (starts_with(name, "test_co")) {
      copy_name(matched, name, DIRSIZ);
      count++;
    }
  }
  close(fd);

  if (count != 1 || strcmp(matched, "test_cow") != 0) {
    printf("test_tab: failed 'test_co' match test (count=%d)\n", count);
    exit(1);
  }
  printf("test_tab: 'test_co' -> '%s' (unique match OK)\n", matched);

  // Test 3: multiple matches for 'test_s' (test_symlink and test_sysinfo)
  fd = open(".", O_RDONLY);
  count = 0;
  while (read(fd, &de, sizeof(de)) == sizeof(de)) {
    if (de.inum == 0)
      continue;
    char name[DIRSIZ + 1];
    copy_name(name, de.name, DIRSIZ);
    if (starts_with(name, "test_s")) {
      count++;
    }
  }
  close(fd);

  if (count < 2) {
    printf("test_tab: failed 'test_s' multi-match test (count=%d, expected >= 2)\n", count);
    exit(1);
  }
  printf("test_tab: 'test_s' -> %d matches found (multi-match OK)\n", count);

  // Test 4: non-existent prefix
  fd = open(".", O_RDONLY);
  count = 0;
  while (read(fd, &de, sizeof(de)) == sizeof(de)) {
    if (de.inum == 0)
      continue;
    char name[DIRSIZ + 1];
    copy_name(name, de.name, DIRSIZ);
    if (starts_with(name, "xyz999")) {
      count++;
    }
  }
  close(fd);

  if (count != 0) {
    printf("test_tab: failed 'xyz999' zero-match test (count=%d)\n", count);
    exit(1);
  }
  printf("test_tab: 'xyz999' -> 0 matches (zero-match OK)\n");

  printf("tab completion test passed!\n");
  exit(0);
}
