#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fs.h"
#include "user/user.h"
#include "kernel/fcntl.h"

#define FIND_MAX_DEPTH 8
#define FIND_MAX_ENTRIES 1000
#define FIND_TIMEOUT_TICKS 100

static int matches_found = 0;
static int timed_out = 0;
static int total_steps = 0;
static int start_tick = 0;

static void
find_recursive(const char *dir, const char *target, int depth)
{
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;

  if (depth >= FIND_MAX_DEPTH)
    return;

  if (++total_steps > FIND_MAX_ENTRIES || (uptime() - start_tick > FIND_TIMEOUT_TICKS)) {
    timed_out = 1;
    return;
  }

  if ((fd = open(dir, O_RDONLY | O_NOFOLLOW)) < 0)
    return;

  if (fstat(fd, &st) < 0 || st.type != T_DIR) {
    close(fd);
    return;
  }

  if (strlen(dir) + 1 + DIRSIZ + 1 > sizeof(buf)) {
    close(fd);
    return;
  }

  strcpy(buf, dir);
  p = buf + strlen(buf);
  if (p > buf && *(p - 1) != '/')
    *p++ = '/';
  else if (p == buf)
    *p++ = '/';

  while (read(fd, &de, sizeof(de)) == sizeof(de)) {
    if (de.inum == 0)
      continue;

    if (++total_steps > FIND_MAX_ENTRIES || ((total_steps % 16) == 0 && (uptime() - start_tick > FIND_TIMEOUT_TICKS))) {
      timed_out = 1;
      close(fd);
      return;
    }

    char fname[DIRSIZ + 1];
    int k;
    for (k = 0; k < DIRSIZ && de.name[k]; k++)
      fname[k] = de.name[k];
    fname[k] = '\0';

    if (strcmp(fname, ".") == 0 || strcmp(fname, "..") == 0)
      continue;

    memmove(p, fname, k);
    p[k] = '\0';

    int sfd = open(buf, O_RDONLY | O_NOFOLLOW);
    if (sfd < 0)
      continue;
    if (fstat(sfd, &st) < 0) {
      close(sfd);
      continue;
    }
    close(sfd);

    if (strcmp(fname, target) == 0) {
      printf("%s\n", buf);
      matches_found++;
    }

    if (st.type == T_DIR) {
      find_recursive(buf, target, depth + 1);
      if (timed_out) {
        close(fd);
        return;
      }
    }
  }

  close(fd);
}

int
main(int argc, char *argv[])
{
  const char *dir = "/";
  const char *target;

  if (argc == 2) {
    target = argv[1];
  } else if (argc == 3) {
    dir = argv[1];
    target = argv[2];
  } else {
    fprintf(2, "Usage: find [dir] filename\n");
    exit(1);
  }

  start_tick = uptime();
  find_recursive(dir, target, 0);

  if (timed_out) {
    fprintf(2, "find: '%s': search timed out or limit reached\n", target);
    if (matches_found == 0)
      exit(1);
    exit(0);
  }

  if (matches_found == 0) {
    fprintf(2, "find: '%s': file not exists\n", target);
    exit(1);
  }

  exit(0);
}

