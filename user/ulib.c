#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "kernel/fs.h"
#include "kernel/riscv.h"
#include "kernel/vm.h"
#include "user/user.h"

//
// wrapper so that it's OK if main() does not call exit().
//
void
start(int argc, char **argv)
{
  int r;
  extern int main(int argc, char **argv);
  r = main(argc, argv);
  exit(r);
}

char *
strcpy(char *s, const char *t)
{
  char *os;

  os = s;
  while ((*s++ = *t++) != 0)
    ;
  return os;
}

int
strcmp(const char *p, const char *q)
{
  while (*p && *p == *q)
    p++, q++;
  return (uchar)*p - (uchar)*q;
}

uint
strlen(const char *s)
{
  int n;

  for (n = 0; s[n]; n++)
    ;
  return n;
}

void *
memset(void *dst, int c, uint n)
{
  char *cdst = (char *)dst;
  int i;
  for (i = 0; i < n; i++) {
    cdst[i] = c;
  }
  return dst;
}

char *
strchr(const char *s, char c)
{
  for (; *s; s++)
    if (*s == c)
      return (char *)s;
  return 0;
}

char *
gets(char *buf, int max)
{
  int i, cc;
  char c;

  for (i = 0; i + 1 < max;) {
    cc = read(0, &c, 1);
    if (cc < 1)
      break;
    buf[i++] = c;
    if (c == '\n' || c == '\r')
      break;
  }
  buf[i] = '\0';
  return buf;
}

int
stat(const char *n, struct stat *st)
{
  int fd;
  int r;

  fd = open(n, O_RDONLY);
  if (fd < 0)
    return -1;
  r = fstat(fd, st);
  close(fd);
  return r;
}

int
atoi(const char *s)
{
  int n;

  n = 0;
  while ('0' <= *s && *s <= '9')
    n = n * 10 + *s++ - '0';
  return n;
}

void *
memmove(void *vdst, const void *vsrc, int n)
{
  char *dst;
  const char *src;

  dst = vdst;
  src = vsrc;
  if (src > dst) {
    while (n-- > 0)
      *dst++ = *src++;
  } else {
    dst += n;
    src += n;
    while (n-- > 0)
      *--dst = *--src;
  }
  return vdst;
}

int
memcmp(const void *s1, const void *s2, uint n)
{
  const char *p1 = s1, *p2 = s2;
  while (n-- > 0) {
    if (*p1 != *p2) {
      return *p1 - *p2;
    }
    p1++;
    p2++;
  }
  return 0;
}

void *
memcpy(void *dst, const void *src, uint n)
{
  return memmove(dst, src, n);
}

char *
sbrk(int n)
{
  return sys_sbrk(n, SBRK_EAGER);
}

char *
sbrklazy(int n)
{
  return sys_sbrk(n, SBRK_LAZY);
}

struct search_state {
  const char *target;
  char *out_path;
  int maxlen;
  int start_tick;
  int timeout_ticks;
  int max_depth;
  int max_entries;
  int steps;
  int timed_out;
};

static int
find_file_rec(const char *dir, int depth, struct search_state *st)
{
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat sbuf;

  // Fail-safe 1: depth limit (prevents stack overflow & cycle traps)
  if (depth >= st->max_depth)
    return 0;

  // Fail-safe 2: step / entry count limit
  if (++st->steps > st->max_entries) {
    st->timed_out = 1;
    return -1;
  }

  // Fail-safe 3: timer tick limit
  if (st->timeout_ticks > 0 && (uptime() - st->start_tick >= st->timeout_ticks)) {
    st->timed_out = 1;
    return -1;
  }

  if ((fd = open(dir, O_RDONLY | O_NOFOLLOW)) < 0)
    return 0;

  if (fstat(fd, &sbuf) < 0 || sbuf.type != T_DIR) {
    close(fd);
    return 0;
  }

  if (strlen(dir) + 1 + DIRSIZ + 1 > sizeof(buf)) {
    close(fd);
    return 0;
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

    if (++st->steps > st->max_entries) {
      st->timed_out = 1;
      close(fd);
      return -1;
    }
    if (st->timeout_ticks > 0 && (st->steps % 16) == 0 && (uptime() - st->start_tick >= st->timeout_ticks)) {
      st->timed_out = 1;
      close(fd);
      return -1;
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

    // Inspect child with O_NOFOLLOW to avoid symlink recursion
    int sfd = open(buf, O_RDONLY | O_NOFOLLOW);
    if (sfd < 0)
      continue;
    if (fstat(sfd, &sbuf) < 0) {
      close(sfd);
      continue;
    }
    close(sfd);

    if (strcmp(fname, st->target) == 0) {
      if (strlen(buf) < st->maxlen) {
        strcpy(st->out_path, buf);
        close(fd);
        return 1;
      }
    }

    if (sbuf.type == T_DIR) {
      int res = find_file_rec(buf, depth + 1, st);
      if (res != 0) {
        close(fd);
        return res; // 1 if found, -1 if timed out
      }
    }
  }

  close(fd);
  return 0;
}

// Configurable file search with fail-safe bounds
int
find_file_timeout(const char *dir, const char *target, char *out_path, int maxlen,
                  int timeout_ticks, int max_depth, int max_entries)
{
  struct search_state st;
  st.target = target;
  st.out_path = out_path;
  st.maxlen = maxlen;
  st.start_tick = uptime();
  st.timeout_ticks = timeout_ticks;
  st.max_depth = (max_depth > 0) ? max_depth : 8;
  st.max_entries = (max_entries > 0) ? max_entries : 500;
  st.steps = 0;
  st.timed_out = 0;

  int res = find_file_rec(dir, 0, &st);
  if (st.timed_out)
    return -1;
  return res;
}

// Fast recursive search through 'dir' for a file named 'target'.
// Protected with default fail-safes (timeout 50 ticks, max depth 8, max 500 entries).
// Returns 1 if found, 0 if not found, -1 if timed out/budget exceeded.
int
find_file(const char *dir, const char *target, char *out_path, int maxlen)
{
  return find_file_timeout(dir, target, out_path, maxlen, 50, 8, 500);
}

static void
write_str(int fd, const char *s)
{
  write(fd, s, strlen(s));
}

// Report file error: if file exists in another directory, output its path;
// if timed out, report search timeout; otherwise output 'file not exists'.
void
file_not_found(const char *cmd, const char *path)
{
  char found_path[512];
  const char *base = path;
  for (const char *s = path; *s; s++) {
    if (*s == '/')
      base = s + 1;
  }

  int res = 0;
  if (base[0] != '\0')
    res = find_file("/", base, found_path, sizeof(found_path));

  if (res > 0) {
    write_str(2, cmd);
    write_str(2, ": '");
    write_str(2, path);
    write_str(2, "' not in current directory; found at: ");
    write_str(2, found_path);
    write_str(2, "\n");
  } else if (res == -1) {
    write_str(2, cmd);
    write_str(2, ": '");
    write_str(2, path);
    write_str(2, "': search timed out, file not exists\n");
  } else {
    write_str(2, cmd);
    write_str(2, ": '");
    write_str(2, path);
    write_str(2, "': file not exists\n");
  }
}

// Safe open: attempts to open path. If failing, runs fast system search to report
// either the actual path where the file resides, or 'file not exists'.
int
open_or_locate(const char *cmd, const char *path, int flags)
{
  int fd = open(path, flags);
  if (fd < 0) {
    file_not_found(cmd, path);
    return -1;
  }
  return fd;
}
