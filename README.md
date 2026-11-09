# xv6-riscv Extended Operating System — Course Project

**Course**: Operating Systems (CS 301 / CS 428)  
**Date**: November 2026  
**Team Members**:
- [Team Member 1] (Student ID: XXXXXXXX)
- [Team Member 2] (Student ID: XXXXXXXX)
- [Team Member 3] (Student ID: XXXXXXXX)

---

## 1. Executive Summary & Goals

This project extends the MIT **xv6-riscv** teaching operating system with a production-grade suite of seven system features spanning user-space utilities, process introspection, resource accounting, execution tracing, priority-based CPU scheduling, file system symbolic links, and virtual-memory Copy-on-Write (COW) page sharing.

The overarching design objective was to implement classic operating system mechanisms that demonstrate core theoretical concepts—paging, page-fault handling, reference counting, dynamic scheduling heuristics, namespace resolution, and system call boundaries—while preserving xv6's modularity and zero-warning build integrity.

---

## 2. Environment & Quickstart (Windows + WSL2 + QEMU)

The project is built and tested inside **WSL2 (Ubuntu 24.04/26.04 LTS)** using the RISC-V 64-bit cross-compilation toolchain and QEMU emulator.

### 2.1 One-Time Host Setup (WSL2 Shell)
```bash
sudo apt update && sudo apt install -y \
  git build-essential gdb-multiarch \
  qemu-system-misc qemu-system-riscv \
  gcc-riscv64-linux-gnu binutils-riscv64-linux-gnu
```

### 2.2 Clone & Build
```bash
# Clone directly into WSL native Linux filesystem for optimal I/O speed
cd ~
git clone <repo-url> xv6-riscv
cd xv6-riscv
git checkout features

# Clean build
make clean
make
```

### 2.3 Running in QEMU
```bash
make qemu
```
*(To exit QEMU at any time, press `Ctrl-A` followed by `X`)*

---

## 3. Implemented Features Matrix

| # | Feature | Category | Difficulty | New Syscalls / Trap Handlers | Status |
|---|---|---|---|---|---|
| **1** | Core Shell Utilities (`touch`, `cp`, `head`, `tail`, `clear`, `uptime`) | User Space | Easy | None | **Completed & Verified** |
| **2** | Process Introspection (`ps`) | Process Management | Easy | `getprocs` (sys #23) | **Completed & Verified** |
| **3** | Kernel Telemetry & Resource Accounting (`sysinfo`) | Memory / Monitoring | Easy | `sysinfo` (sys #24) | **Completed & Verified** |
| **4** | Syscall Tracing Subsystem (`trace`) | Observability / Kernel | Easy-Med | `trace` (sys #25) | **Completed & Verified** |
| **5** | Priority CPU Scheduler (`nice`) | Kernel Scheduling | Medium | `nice` (sys #26) | **Completed & Verified** |
| **6** | Symbolic Links & Path Resolution (`ln -s`) | File System (VFS/Vnode) | Medium | `symlink` (sys #27) | **Completed & Verified** |
| **7** | Copy-on-Write (COW) Page Allocation | Virtual Memory | Hard | Store Page-Fault (`scause 15`) | **Completed & Verified** |
| **8** | Shell Tab-Completion & Line Editing | Shell / I/O Subsystem | Medium | Console Read/Interrupt Interception | **Completed & Verified** |
| **9** | Fast System-Wide File Search & Path Discovery | File System / Userland | Medium | Recursive Directory Scanner (`find`, `search`) | **Completed & Verified** |

---

## 4. Deep-Dive Feature Documentation

### Feature 1: Core Shell Utilities (`touch`, `cp`, `head`, `tail`, `clear`, `uptime`)

#### Plain-Language Explanation
Expands the rudimentary xv6 shell userland with standard POSIX commands:
- `touch <file>`: Creates an empty file if it doesn't exist or opens it without truncating.
- `cp <src> <dst>`: Copies file contents via streaming 512-byte block transfers.
- `head [-n] [file]`: Prints the first $n$ lines of stdin or target file (default: 10).
- `tail [-n] [file]`: Reads a stream or file and prints the final $n$ lines.
- `clear`: Emits standard ANSI terminal escape sequences (`\033[2J\033[H`) to wipe screen and reset cursor.
- `uptime`: Queries timer ticks since boot and converts ticks to seconds.

#### OS Concept Demonstrated
- Standard streams (`0=stdin`, `1=stdout`, `2=stderr`).
- File descriptor life cycles (`open()`, `read()`, `write()`, `close()`).
- File creation flags (`O_CREATE`, `O_WRONLY`, `O_RDWR`).

#### Implementation Walkthrough
- Files added: `user/touch.c`, `user/cp.c`, `user/head.c`, `user/tail.c`, `user/clear.c`, `user/uptime.c`.
- Registered in `Makefile` under `UPROGS`.
```c
// user/touch.c: File creation via O_CREATE
int fd = open(argv[i], O_CREATE | O_RDWR);
if (fd < 0) {
  fprintf(2, "touch: cannot create %s\n", argv[i]);
  exit(1);
}
close(fd);
```

#### Demo & Verification
```bash
$ touch file_a
$ echo line1 > file_a
$ echo line2 >> file_a
$ cp file_a file_b
$ head -1 file_b
line1
$ tail -1 file_b
line2
$ uptime
uptime: 45 ticks (0 seconds)
```

---

### Feature 2: Process Table Introspection (`ps`)

#### Plain-Language Explanation
A diagnostic command displaying current processes in the operating system: PID, parent PID (PPID), scheduler state (`SLEEPING`, `RUNNABLE`, `RUNNING`, `ZOMBIE`), allocated memory footprint in kilobytes, and binary name.

#### OS Concept Demonstrated
Process Control Blocks (PCBs), process life cycle state transitions, kernel-to-user memory isolation, and safe user-space payload serialization.

#### Implementation Details
- **Data Structure (`kernel/types.h`)**:
  ```c
  struct procinfo {
    int pid;
    int ppid;
    int state;
    uint64 sz;
    char name[16];
  };
  ```
- **Kernel Function (`kernel/proc.c: kgetprocs()`)**:
  Loops through the global `proc[NPROC]` table. For each process with `state != UNUSED`, acquires `p->lock`, populates a `struct procinfo`, releases `p->lock`, and copies it to user space via `copyout()`.
- **System Call Plumbing**:
  - `kernel/syscall.h`: `#define SYS_getprocs 23`
  - `kernel/syscall.c`: added `[SYS_getprocs] = sys_getprocs`
  - `user/usys.pl`: `entry("getprocs");`
  - `user/user.h`: `int getprocs(struct procinfo *, int);`
  - `user/ps.c`: User-space formatting frontend.

#### Demo Output
```bash
$ ps
PID     PPID    STATE           MEM(KB) NAME
1       0       SLEEPING        16      init
2       1       SLEEPING        20      sh
4       2       RUNNING         16      ps
```

---

### Feature 3: Resource Accounting & Telemetry (`sysinfo`)

#### Plain-Language Explanation
A system call and tool that reports total physical free memory available in bytes/megabytes, active process count, and kernel uptime.

#### OS Concept Demonstrated
Free-page frame tracking, physical memory allocator bookkeeping, and dynamic OS telemetry.

#### Implementation Details
- **Memory Walk (`kernel/kalloc.c: kfreemem()`)**:
  Traverses the physical page allocator linked list `kmem.freelist` with `kmem.lock` held, multiplying the count of free pages by `PGSIZE` (4096 bytes).
  ```c
  uint64 kfreemem(void) {
    struct run *r;
    uint64 count = 0;
    acquire(&kmem.lock);
    r = kmem.freelist;
    while(r) {
      count += PGSIZE;
      r = r->next;
    }
    release(&kmem.lock);
    return count;
  }
  ```
- **Syscall (`kernel/sysproc.c: sys_sysinfo()`)**: Fills `struct sysinfo` and safely calls `copyout()`.

#### Demo Output
```bash
$ sysinfo
Free memory : 126 MB (130008 KB)
Processes   : 3
Uptime      : 43 ticks (~0 seconds)
```

---

### Feature 4: Syscall Tracing Subsystem (`trace`)

#### Plain-Language Explanation
Allows a user or developer to monitor specific system calls made by a process and its children. Tracing is configured via a bitmask (e.g., bit $k$ active enables tracing of system call $k$).

#### OS Concept Demonstrated
System call dispatch layer, trap interception, parent-to-child state inheritance across `fork()`.

#### Implementation Details
- Added `int tracemask;` field to `struct proc` in `kernel/proc.h`.
- Initialized `p->tracemask = 0;` in `allocproc()`.
- Copied `np->tracemask = p->tracemask;` in `kfork()` to preserve tracing across child processes.
- Intercepted dispatch in `kernel/syscall.c: syscall()`:
  ```c
  p->trapframe->a0 = syscalls[num]();
  if (p->tracemask & (1 << num)) {
    printk("trace: pid=%d %s -> syscall %d = %d\n",
           p->pid, p->name, num, (int)p->trapframe->a0);
  }
  ```
- User command: `user/trace.c` sets trace mask, then `exec()`s the requested command.

#### Demo Output
```bash
$ trace 16384 uptime
trace: pid=4 uptime -> syscall 14 = 34
uptime: 34 ticks (0 seconds)
```
*(14 is `SYS_uptime`, $1 \ll 14 = 16384$. The kernel prints trace notification when the uptime syscall executes).*

---

### Feature 5: Priority CPU Scheduler (`nice`)

#### Plain-Language Explanation
Replaces default round-robin scheduling with a priority-aware scheduler. Processes are assigned a priority value between 0 (highest priority) and 19 (lowest priority, default 10). When picking a `RUNNABLE` task, the scheduler always prioritizes the process with the highest priority (lowest integer value).

#### OS Concept Demonstrated
CPU scheduling algorithms, preemptive multitasking, priority-based dispatching, starvation mitigation concepts.

#### Implementation Details
- `struct proc`: added `int priority;` (default 10 in `allocproc()`).
- Syscall `nice(int prio)`: clamps to $[0, 19]$ and assigns to `myproc()->priority`.
- **Scheduler Core (`kernel/proc.c: scheduler()`)**:
  ```c
  struct proc *chosen = 0;
  for (p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if (p->state == RUNNABLE) {
      if (chosen == 0 || p->priority < chosen->priority) {
        if (chosen != 0) release(&chosen->lock);
        chosen = p;
      } else {
        release(&p->lock);
      }
    } else {
      release(&p->lock);
    }
  }
  if (chosen != 0) {
    chosen->state = RUNNING;
    c->proc = chosen;
    swtch(&c->context, &chosen->context);
    ...
    release(&chosen->lock);
  }
  ```
- **Test (`user/test_nice.c`)**: Forks two CPU-intensive children (one with `nice(1)`, one with `nice(18)`). Demonstrates that `pid=8` (high priority) finishes before `pid=9` (low priority).

#### Demo Output
```bash
$ test_nice
high-priority child done (pid=13)
low-priority child done (pid=14)
nice test done
```

---

### Feature 6: Symbolic Links & Recursive Path Traversal (`ln -s`)

#### Plain-Language Explanation
Introduces soft/symbolic links to xv6. A symbolic link is an inode of type `T_SYMLINK` containing a string path pointing to a target file. When opened, `sys_open` transparently resolves the path, dereferencing through up to 10 hops to prevent infinite cyclic link recursion.

#### OS Concept Demonstrated
Virtual file systems, inode types, directory traversal, soft vs. hard links, loop detection and recursion depth limits.

#### Implementation Details
- Inode type in `kernel/stat.h`: `#define T_SYMLINK 4`.
- Added `sys_symlink(target, path)`: allocates a `T_SYMLINK` inode via `create()` and writes the path string into data block 0 using `writei()`.
- Enhanced `sys_open()` in `kernel/sysfile.c`:
  ```c
  for (int depth = 0; depth < 10; depth++) {
    if (ip->type != T_SYMLINK) break;
    if (omode & O_NOFOLLOW) break;
    if (readi(ip, 0, (uint64)path, 0, MAXPATH) < 0) {
      iunlockput(ip); end_op(); return -1;
    }
    iunlockput(ip);
    if ((ip = namei(path)) == 0) {
      end_op(); return -1;
    }
    ilock(ip);
  }
  ```
- Extended `user/ln.c` with the `-s` flag to invoke `symlink()`.

#### Demo Output
```bash
$ echo hello_world > file1
$ ln -s file1 link1
$ cat link1
hello_world
$ test_symlink
read via symlink: hello symlink!
symlink test passed!
```

---

### Feature 7: Copy-on-Write (COW) Fork

#### Plain-Language Explanation
In traditional xv6 `fork()`, the kernel allocates physical memory and copies all user pages from parent to child immediately. This is inefficient when a process promptly calls `exec()`.

Our COW implementation modifies `fork()` to **share** physical memory pages between parent and child, marking both page table entries (PTEs) as **read-only** with a special `PTE_COW` software bit. When either process attempts to write to a shared page, the RISC-V MMU generates a store page fault (`scause == 15`). The kernel intercepts the trap, allocates a new physical frame, copies the 4096-byte page, updates the page table with read-write permissions, and resumes execution seamlessly.

#### OS Concept Demonstrated
Virtual memory management, RISC-V Sv39 multi-level page tables, page-fault exception handling, physical memory reference counting.

#### Implementation Details
1. **Reference Counting (`kernel/kalloc.c`)**:
   - Maintains `int count[32768]` guarded by a dedicated `pageref.lock`.
   - `kalloc()` initializes reference count to 1.
   - `incref(pa)` increments count when a page is mapped into another address space.
   - `kfree(pa)` decrements count and only returns the page frame to `kmem.freelist` when count reaches 0.
2. **COW Flag in Sv39 PTE (`kernel/riscv.h`)**:
   - Uses bit 8 (reserved for supervisor software): `#define PTE_COW (1L << 8)`.
3. **Lazy Page Duplication (`kernel/vm.c: uvmcopy()`)**:
   - Instead of allocating memory, clears `PTE_W`, sets `PTE_COW`, and calls `incref()`.
4. **Trap Interception (`kernel/trap.c: usertrap()`)**:
   - Identifies `r_scause() == 15` (Store/AMO page fault).
   - Invokes `cowcopy(p->pagetable, r_stval())`.
   - `cowcopy()` allocates a fresh physical page, performs `memmove()`, decrements original refcount, and remaps the faulting address with `PTE_W`.

#### Demo Output
```bash
$ test_cow
child: buf[0]=B (expected B)
parent: buf[0]=A (expected A)
COW fork test passed!
```

---

### Feature 8: Shell Tab-Completion & Line Editing

#### Plain-Language Explanation
Enables interactive auto-completion of commands and filenames in the xv6 shell when pressing the `[Tab]` key.
- If a prefix uniquely matches a file or command in the current directory (e.g. `upt[Tab]`), the shell automatically completes the remainder (`uptime `) directly on the command line.
- If multiple candidates match (e.g. `test_s[Tab]`), the shell prints all matching filenames (`test_symlink  test_sysinfo`), redisplays the prompt, and preserves the typed prefix.
- Also supports backspacing across tab-completed words without corrupting the terminal buffer.

#### OS Concept Demonstrated
- Console input architecture: interrupt handler (`consoleintr`), line discipline buffering (`cons.buf`), and synchronous user-space read (`consoleread`).
- Separation of concerns between device drivers and user-space shells.
- Directory stream parsing and prefix matching via `struct dirent`.
- Terminal escape sequences and cursor control.

#### Implementation Details
1. **Console Driver Hook (`kernel/console.c`)**:
   - In `consoleintr()`, intercepted `\t` (ASCII 9). Instead of echoing an unhelpful tab character to UART, it stores `\t` in `cons.buf`, updates `cons.w = cons.e`, and triggers `wakeup(&cons.r)`.
   - In `consoleread()`, added `\t` and `\b` as immediate loop break conditions (`if (c == '\n' || c == '\t' || c == '\b') break;`) so tab keypresses immediately yield control back to the user-space line reader.
   - For backspace when the user has already consumed characters (`cons.e == cons.w`), forwarded `\b` to user space.
2. **Interactive Shell Reader (`user/sh.c`)**:
   - Replaced plain `gets()` in `getcmd()` with an interactive character reader.
   - `tab_complete()` extracts the current word prefix backward from cursor, opens `.` (`O_RDONLY`), traverses directory entries (`struct dirent`), and matches candidates via prefix comparison.
   - On a unique match, writes the suffix string and a trailing space to stdout (`write(1, ...)`) and appends them to the input buffer.
   - On ambiguous matches, prints all options on a new line and reprints the prompt and line buffer.
3. **Automated Unit Test (`user/test_tab.c`)**:
   - Programmatically tests unique prefix completion (`upt` -> `uptime`, `test_co` -> `test_cow`), multiple candidate detection (`test_s` -> 2 matches), and non-existent prefix rejection (`xyz999` -> 0 matches).

#### Demo Output
```bash
$ upt[Tab] -> completes to:
$ uptime 
uptime: 37 ticks (0 seconds)

$ head -1 RE[Tab] -> completes to:
$ head -1 README 
xv6 is a re-implementation of Dennis Ritchie's and Ken Thompson's Unix

$ test_s[Tab]
test_symlink  test_sysinfo  
$ test_s
```

---

### Feature 9: Fast System-Wide File Search & Path Discovery

#### Plain-Language Explanation
Provides a fast, recursive file-search command (`find` and `search`) that traverses the directory tree from root (`/`) or any specified subfolder, outputting the absolute path of matching files.

In addition, standard commands (`cat`, `head`, `tail`, `grep`, `wc`, `cp`, and `sh`) now feature **intelligent path discovery**:
- If a user runs a command on a file that does not exist in the current working directory, instead of a blunt "file not found" error, the command automatically triggers a fast system-wide search.
- If the file exists in another directory (e.g. `/docs/secret.txt`), the command reports its actual path:
  `cat: 'secret.txt' not in current directory; found at: /docs/secret.txt`
- If the file does not exist anywhere in the system, it prints:
  `cat: 'secret.txt': file not exists`

#### OS Concept Demonstrated
- Hierarchical file system traversal (`stat()`, directory inodes, `struct dirent`).
- POSIX stream recursion and path buffer synthesis.
- Userland library integration (`ulib.c`) providing uniform error reporting and path recovery across core utilities.
- File system capacity scaling (`FSSIZE` expansion in `kernel/param.h`).

#### Implementation Details
1. **Recursive Search Engine (`user/ulib.c: find_file()`)**:
   - Recursively opens directories, reads `struct dirent` records, builds absolute path buffers, and compares filenames against target queries.
2. **Intelligent Error Dispatcher (`user/ulib.c: file_not_found()`, `open_or_locate()`)**:
   - `open_or_locate(cmd, path, flags)` wraps standard `open()`. If opening fails, it executes `find_file("/", base, ...)` to determine if the target resides in another folder and outputs the path, or confirms non-existence.
   - Built with raw `write()` system calls to prevent linking dependencies with `forktest`.
3. **Core Utility Updates**:
   - Updated `cat.c`, `head.c`, `tail.c`, `grep.c`, `wc.c`, `cp.c`, and `sh.c` to use `open_or_locate()` and `file_not_found()`.
4. **CLI Utilities & Tests**:
   - Added `user/find.c` (recursive file finder) and `user/search.c` (alias).
   - Added `user/test_find.c` validating deep file discovery, non-existent rejection, and error message paths.
   - Scaled `FSSIZE` to 4000 blocks in `kernel/param.h` to comfortably host all utilities and test binaries.

#### Demo Output
```bash
$ find README
/README

$ mkdir docs
$ echo secret > docs/secret.txt
$ find secret.txt
/docs/secret.txt

$ cat secret.txt
cat: 'secret.txt' not in current directory; found at: /docs/secret.txt

$ cat missing.txt
cat: 'missing.txt': file not exists
```

---

## 5. Live Demonstration Script (~10 Minutes)

Follow this step-by-step sequence when demonstrating the project to the professor:

### Step 1: Boot and Baseline Verification (1 min)
1. In WSL terminal, run: `make clean && make qemu`
2. Show clean build without warnings.
3. Observe successful xv6 boot banner (`init: starting sh`).

### Step 2: System Telemetry & Process Inspection (2 min)
1. Run `sysinfo` to demonstrate dynamic memory accounting:
   ```bash
   $ sysinfo
   Free memory : 126 MB (130008 KB)
   Processes   : 3
   Uptime      : 30 ticks (~0 seconds)
   ```
2. Run `ps` to display process state table:
   ```bash
   $ ps
   PID  PPID    STATE           MEM(KB) NAME
   1    0       SLEEPING        16      init
   2    1       SLEEPING        20      sh
   4    2       RUNNING         16      ps
   ```

### Step 3: Syscall Observability (2 min)
1. Demonstrate runtime syscall tracing using the bitmask:
   ```bash
   $ trace 16384 uptime
   trace: pid=5 uptime -> syscall 14 = 55
   uptime: 55 ticks (0 seconds)
   ```
2. Explain to the professor how bit $14$ selects `SYS_uptime` and prints execution return status.

### Step 4: Priority Scheduling Benchmark (2 min)
1. Run the priority scheduler test:
   ```bash
   $ test_nice
   high-priority child done (pid=6)
   low-priority child done (pid=7)
   nice test done
   ```
2. Explain that both children execute identical CPU loops, but child with priority 1 completes first.

### Step 5: Symbolic Link File System (1.5 min)
1. Demonstrate soft links live in the shell:
   ```bash
   $ echo "Testing XV6 Symbolic Links" > original.txt
   $ ln -s original.txt my_link
   $ cat my_link
   Testing XV6 Symbolic Links
   ```
2. Run automated validation: `test_symlink`

### Step 6: Copy-on-Write Virtual Memory Proof (1.5 min)
1. Run the automated COW verification suite:
   ```bash
   $ test_cow
   child: buf[0]=B (expected B)
   parent: buf[0]=A (expected A)
   COW fork test passed!
   ```
2. Explain: Parent and child initially shared the identical physical address. When the child mutated memory, a hardware page fault safely triggered copy-on-write without modifying the parent.

### Step 7: Interactive Shell Tab-Completion (1 min)
1. In the shell, type partial command name and press Tab:
   ```bash
   $ upt[Tab]
   ```
   Show that the shell immediately completes it to `$ uptime `!
   Press Enter to run it:
   ```bash
   uptime: 45 ticks (0 seconds)
   ```
2. Demonstrate argument completion:
   ```bash
   $ head -1 RE[Tab]
   ```
   Completes to `head -1 README ` and outputs the first line of the README file!
3. Demonstrate multi-match ambiguity listing:
   ```bash
   $ test_s[Tab]
   test_symlink  test_sysinfo  
   $ test_s
   ```
4. Run the automated unit test: `test_tab`

### Step 8: Fast File Search & Smart Path Discovery (1.5 min)
1. Run fast search for a known file:
   ```bash
   $ find README
   /README
   ```
2. Create a nested file in a directory and search for it:
   ```bash
   $ mkdir docs
   $ echo "Course Report" > docs/report.txt
   $ find report.txt
   /docs/report.txt
   ```
3. Demonstrate intelligent path discovery on command failure:
   Attempt to read `report.txt` from `/` without specifying the directory:
   ```bash
   $ cat report.txt
   cat: 'report.txt' not in current directory; found at: /docs/report.txt
   ```
   Show that xv6 automatically searched the filesystem and helped the user locate the file!
4. Show proper non-existence error handling when a file truly doesn't exist:
   ```bash
   $ cat ghost_file
   cat: 'ghost_file': file not exists
   ```
5. Run the automated unit test: `test_find`

---

## 6. Automated Test Suite Summary

All nine features are covered by automated unit/integration tests included in `fs.img`:
- `test_ps`: Validates kernel process table retrieval.
- `test_sysinfo`: Validates memory accounting and tick reporting.
- `test_trace`: Validates mask setting and syscall trap printouts.
- `test_utils`: Validates `touch`, `cp`, `head`, and `tail`.
- `test_nice`: Validates CPU priority enforcement.
- `test_symlink`: Validates soft link inode creation and dereference.
- `test_cow`: Validates copy-on-write fault interception and isolation.
- `test_tab`: Validates shell tab-completion matching and directory scanning.
- `test_find`: Validates fast recursive search and intelligent path error reporting.

---

## 7. References & Acknowledgments
- **xv6-riscv**: MIT 6.S081 / 6.1810 Operating System Engineering (Frans Kaashoek, Robert Morris, Russ Cox).
- **RISC-V Privileged Architecture**: Volume II: Privileged Architecture V20190608 (Sv39 MMU Specification).

