#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  exec("find", argv);
  fprintf(2, "search: exec find failed\n");
  exit(1);
}

