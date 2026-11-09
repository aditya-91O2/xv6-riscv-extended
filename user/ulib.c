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

// Fast recursive search through 'dir' for a file named 'target'.
// Writes the full path of the match to 'out_path'.
// Returns 1 if found, 0 if not found.
int
find_file(const char *dir, const char *target, char *out_path, int maxlen)
{
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;

  if ((fd = open(dir, O_RDONLY)) < 0)
    return 0;

  if (fstat(fd, &st) < 0 || st.type != T_DIR) {
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

    char fname[DIRSIZ + 1];
    int k;
    for (k = 0; k < DIRSIZ && de.name[k]; k++)
      fname[k] = de.name[k];
    fname[k] = '\0';

    if (strcmp(fname, ".") == 0 || strcmp(fname, "..") == 0)
      continue;

    memmove(p, fname, k);
    p[k] = '\0';

    if (stat(buf, &st) < 0)
      continue;

    if (strcmp(fname, target) == 0) {
      if (strlen(buf) < maxlen) {
        strcpy(out_path, buf);
        close(fd);
        return 1;
      }
    }

    if (st.type == T_DIR) {
      if (find_file(buf, target, out_path, maxlen)) {
        close(fd);
        return 1;
      }
    }
  }

  close(fd);
  return 0;
}

static void
write_str(int fd, const char *s)
{
  write(fd, s, strlen(s));
}

// Report file error: if file exists in another directory, output its path;
// otherwise output 'file not exists'.
void
file_not_found(const char *cmd, const char *path)
{
  char found_path[512];
  const char *base = path;
  for (const char *s = path; *s; s++) {
    if (*s == '/')
      base = s + 1;
  }

  if (base[0] != '\0' && find_file("/", base, found_path, sizeof(found_path))) {
    write_str(2, cmd);
    write_str(2, ": '");
    write_str(2, path);
    write_str(2, "' not in current directory; found at: ");
    write_str(2, found_path);
    write_str(2, "\n");
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
