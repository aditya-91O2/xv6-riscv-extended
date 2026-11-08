typedef unsigned int uint;
typedef unsigned short ushort;
typedef unsigned char uchar;

typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int uint32;
typedef unsigned long uint64;

typedef uint64 pde_t;

// System information struct for sysinfo syscall
struct sysinfo {
  uint64 freemem;   // free memory in bytes
  int    nproc;     // number of non-UNUSED processes
  uint   uptime;    // ticks since boot
};

// Process information struct for getprocs syscall
struct procinfo {
  int pid;
  int ppid;
  int state;
  uint64 sz;
  char name[16];
};
