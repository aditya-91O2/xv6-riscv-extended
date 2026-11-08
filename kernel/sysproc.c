#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "vm.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  kexit(n);
  return 0; // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return kfork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return kwait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int t;
  int n;

  argint(0, &n);
  argint(1, &t);
  addr = myproc()->sz;

  if (t == SBRK_EAGER || n < 0) {
    if (growproc(n) < 0) {
      return -1;
    }
  } else {
    // Lazily allocate memory for this process: increase its memory
    // size but don't allocate memory. If the processes uses the
    // memory, vmfault() will allocate it.
    if (addr + n < addr)
      return -1;
    if (addr + n > TRAPFRAME)
      return -1;
    myproc()->sz += n;
  }
  return addr;
}

uint64
sys_pause(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  if (n < 0)
    n = 0;
  acquire(&tickslock);
  ticks0 = ticks;
  while (ticks - ticks0 < n) {
    if (killed(myproc())) {
      release(&tickslock);
      return -1;
    }
    sleep_prepare(&ticks);
    release(&tickslock);
    sleep();
    acquire(&tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kkill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

// sys_getprocs: fill user buffer with process info
uint64
sys_getprocs(void)
{
  uint64 addr;
  int max;
  argaddr(0, &addr);
  argint(1, &max);
  return kgetprocs(addr, max);
}

// sys_sysinfo: return system info (free memory, nproc, uptime)
uint64
sys_sysinfo(void)
{
  uint64 addr;
  struct sysinfo info;
  struct proc *p;
  int nproc = 0;

  argaddr(0, &addr);

  // count non-UNUSED procs
  extern struct proc proc[];
  for (p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if (p->state != UNUSED) nproc++;
    release(&p->lock);
  }

  acquire(&tickslock);
  info.uptime = ticks;
  release(&tickslock);

  info.freemem = kfreemem();
  info.nproc   = nproc;

  if (copyout(myproc()->pagetable, myproc()->sz, addr, (char*)&info, sizeof(info)) < 0)
    return -1;
  return 0;
}

// sys_trace: set syscall trace mask for current process
uint64
sys_trace(void)
{
  int mask;
  argint(0, &mask);
  myproc()->tracemask = mask;
  return 0;
}

// sys_nice: set scheduler priority (0=highest, 19=lowest)
uint64
sys_nice(void)
{
  int prio;
  argint(0, &prio);
  if (prio < 0) prio = 0;
  if (prio > 19) prio = 19;
  myproc()->priority = prio;
  return 0;
}
