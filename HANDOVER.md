# xv6-riscv Engineering Handover & Teammate Reference

**Repository Branch**: `features`  
**Last Updated**: November 9, 2026  
**Status**: All 9 planned features completed, tested, and validated in QEMU.

---

## 1. Project Status Summary

| Area / Feature | Status | Ownership | Test Suite |
|---|---|---|---|
| **Shell Utilities** (`touch`, `cp`, `head`, `tail`, `clear`, `uptime`) | Done | Team Member 1 | `test_utils` |
| **Process Introspection** (`ps`, `getprocs`) | Done | Team Member 1 | `test_ps` |
| **Kernel Telemetry** (`sysinfo`) | Done | Team Member 2 | `test_sysinfo` |
| **Execution Tracing** (`trace`) | Done | Team Member 2 | `test_trace` |
| **Priority Scheduling** (`nice`) | Done | Team Member 3 | `test_nice` |
| **Symbolic Links** (`ln -s`, `symlink`) | Done | Team Member 3 | `test_symlink` |
| **Copy-on-Write Fork** (`COW`, `cowcopy`) | Done | All / Senior Lead | `test_cow` |
| **Shell Tab-Completion & Line Editing** (`sh`, `test_tab`) | Done | Team Member 1 | `test_tab` |
| **Fast File Search & Path Discovery** (`find`, `search`, `test_find`) | Done | Team Member 2 | `test_find` |

---

## 2. Environment Setup & Troubleshooting Guide

### 2.1 Clean Environment Setup
If setting up on a new Windows machine with WSL2 (Ubuntu 24.04 or 26.04 LTS):
```bash
sudo apt update && sudo apt install -y \
  git build-essential gdb-multiarch \
  qemu-system-misc qemu-system-riscv \
  gcc-riscv64-linux-gnu binutils-riscv64-linux-gnu
```

### 2.2 Common WSL / QEMU Issues & Solutions

1. **Issue: `qemu-system-riscv64: not found` during `make qemu`**
   - *Cause*: On Ubuntu 24.04/26.04, `qemu-system-misc` does **not** contain the RISC-V 64 emulator.
   - *Fix*: Install `qemu-system-riscv`:
     ```bash
     sudo apt install -y qemu-system-riscv
     ```

2. **Issue: Line-ending issues or slow compilation (`/mnt/c`)**
   - *Cause*: Compiling across the 9P Windows filesystem mount (`/mnt/c/`) introduces latency and CRLF line-ending mismatches.
   - *Fix*: Always clone inside native WSL ext4 filesystem (`~/xv6-riscv/`). Windows tools can access it via the UNC path `\\wsl.localhost\Ubuntu-26.04\home\<user>\xv6-riscv`.

3. **Issue: How to exit QEMU?**
   - Press `Ctrl-A` then `X`. If stuck, run `killall qemu-system-riscv64` in another WSL tab.

---

## 3. Repository Architecture & File Modification Map

```
xv6-riscv/
├── kernel/
│   ├── types.h          <-- Added struct sysinfo, struct procinfo
│   ├── proc.h           <-- Added int priority, int tracemask to struct proc
│   ├── proc.c           <-- Added kgetprocs(), priority scheduler loop
│   ├── kalloc.c         <-- Added pageref ref-counting array, incref(), decref(), kfreemem()
│   ├── vm.c             <-- Updated uvmcopy() for COW; added cowcopy() fault handler
│   ├── trap.c           <-- Handled scause == 15 (Store Page Fault) in usertrap()
│   ├── riscv.h          <-- Added #define PTE_COW (1L << 8)
│   ├── stat.h           <-- Added #define T_SYMLINK 4
│   ├── syscall.h        <-- Defined SYS_getprocs(23), SYS_sysinfo(24), SYS_trace(25), SYS_nice(26), SYS_symlink(27)
│   ├── syscall.c        <-- Registered new syscall handlers, trace logging hook
│   ├── sysproc.c        <-- Implemented sys_getprocs, sys_sysinfo, sys_trace, sys_nice
│   ├── sysfile.c        <-- Implemented sys_symlink, modified sys_open for symlink following
│   └── defs.h           <-- Declared kgetprocs, kfreemem, incref, decref, cowcopy
├── user/
│   ├── user.h           <-- User syscall prototypes
│   ├── usys.pl          <-- Syscall assembly stub generator
│   ├── ps.c             <-- ps CLI utility
│   ├── sysinfo.c        <-- sysinfo CLI utility
│   ├── trace.c          <-- trace wrapper tool
│   ├── nice.c           <-- nice execution tool
│   ├── ln.c             <-- Extended to support 'ln -s'
│   ├── touch.c          <-- touch command
│   ├── cp.c             <-- cp command
│   ├── head.c           <-- head command
│   ├── tail.c           <-- tail command
│   ├── clear.c          <-- clear command
│   ├── uptime.c         <-- uptime command
│   ├── test_ps.c        <-- Automated test for ps
│   ├── test_sysinfo.c   <-- Automated test for sysinfo
│   ├── test_trace.c     <-- Automated test for trace
│   ├── test_utils.c     <-- Automated test for extra utilities
│   ├── test_nice.c      <-- Automated test for priority scheduling
│   ├── test_symlink.c   <-- Automated test for symlinks
│   └── test_cow.c       <-- Automated test for Copy-on-Write
└── Makefile             <-- Added UPROGS entries for tools and test runners
```

---

## 4. How to Add a New System Call (Step-by-Step Checklist)

Follow this strict 7-step checklist whenever introducing a new syscall:
1. **Assign number**: In `kernel/syscall.h`, define `#define SYS_<name> <number>` (current next number is 28).
2. **User stub**: In `user/usys.pl`, append `entry("<name>");`.
3. **User prototype**: In `user/user.h`, declare `int <name>(...);`.
4. **Syscall dispatch**: In `kernel/syscall.c`:
   - Add `extern uint64 sys_<name>(void);`.
   - Add entry to `syscalls[]` array: `[SYS_<name>] sys_<name>,`.
5. **Syscall wrapper**: In `kernel/sysproc.c` (or `kernel/sysfile.c`), implement `uint64 sys_<name>(void)` using `argint`, `argaddr`, `argstr` to safely extract registers from `myproc()->trapframe`.
6. **Kernel logic**: In `kernel/proc.c`, `kernel/vm.c`, or appropriate kernel module, implement core functionality. Declare function in `kernel/defs.h`.
7. **Build & verify**: Run `make clean && make` to confirm zero compilation errors.

---

## 5. Per-Feature Design Notes, Gotchas & Solved Bugs

### 5.1 `ps` & `sysinfo` Formatting in xv6 Userland
- **Gotcha**: Standard xv6 `printf()` does **not** support POSIX format modifiers like `%-6s` or `%10d`. Attempting to use them causes corrupted output.
- **Solution**: Formatted output using tab stops (`\t`) and clean column separators.
- **Kernel-Header Boundary**: User programs like `ps.c` should never `#include "kernel/proc.h"` because `proc.h` requires kernel-internal types (`spinlock`, `pagetable_t`, `NOFILE`, `NCPU`). Common structures like `struct procinfo` and `struct sysinfo` were moved to `kernel/types.h`.

### 5.2 Copy-on-Write Page Fault & Reference Counting
- **Design Decision**: RISC-V Sv39 page table entries reserve bits 8 and 9 for supervisor software (`RSW`). We allocated bit 8: `#define PTE_COW (1L << 8)`.
- **Reference Count Array**: Memory spans up to `PHYSTOP` (128 MB), resulting in at most 32,768 physical 4096-byte pages. An array `int count[32768]` indexed by `(pa - KERNBASE) / PGSIZE` is tracked under `pageref.lock`.
- **Trap Handling**: In `usertrap()`, store page faults trigger with `r_scause() == 15`. If the page is marked `PTE_COW`, `cowcopy()` allocates a fresh physical frame, copies the content, decrements the source frame's refcount, and remaps the faulting address with `PTE_W`. If the page only has 1 reference left, it simply restores `PTE_W` without an extra allocation.

### 5.3 Priority Scheduler Starvation
- **Design Decision**: The scheduler scans the process table and selects the `RUNNABLE` process with the lowest numeric `priority` ($0 \le priority \le 19$).
- **Locking Rule**: Lock order must be strictly preserved: acquire process lock before checking state and priority; release previous candidate's lock when a higher-priority candidate is found.

### 5.4 Symbolic Links Loop Protection
- **Design Decision**: A malicious or recursive symlink chain (e.g., `ln -s a b; ln -s b a`) could lock the kernel in an infinite loop inside `sys_open`.
- **Solution**: We implemented a hard recursion depth limit of 10 hops. If depth exceeds 10, `sys_open` aborts and returns `-1`.

### 5.5 Shell Tab-Completion & Line Editing
- **Design Decision**: `consoleintr` in `kernel/console.c` is an interrupt handler and cannot perform disk I/O to read directories. Therefore, path/command completion must execute in user space inside `user/sh.c`.
- **Interrupt Wakeup Hook**: `consoleintr()` treats `\t` as a wake-up event for `consoleread()` without echoing the tab to the display. `consoleread()` breaks on `\t` and returns immediately.
- **Suffix Output**: `sh.c` reads the prefix, opens `.` to find matching `struct dirent` records, prints only the completion suffix and trailing space to stdout (`write(1, ...)`), and updates its internal command buffer.
- **Multi-match listing**: If more than 1 candidate matches, `sh.c` prints all matching names on a new line and reprints `$ <prefix>`.
- **Backspace forward**: When backspacing past already-delivered input (`cons.e == cons.w`), the console sends `\b` to user space, allowing `sh.c` to pop characters and emit `\b \b`.

### 5.6 Fast File Search & Smart Path Error Discovery
- **Design Decision**: Implemented recursive file system traversal in `user/ulib.c: find_file()` using directory entry descriptors (`struct dirent`) and `stat()`.
- **Fail-Safe Resource Limits & Timeout**:
  - `SEARCH_MAX_DEPTH = 8`: Prevents stack exhaustion in xv6's single 4096-byte user stack frame.
  - `SEARCH_MAX_ENTRIES = 500`: Caps maximum directory entries examined during any single search.
  - `SEARCH_TIMEOUT_TICKS = 50`: Uses `uptime()` system call to enforce a maximum search duration (~0.5s in xv6), aborting with `-1` if exceeded.
  - `O_NOFOLLOW` Symlink Loop Protection: Directory traversal uses `open(..., O_RDONLY | O_NOFOLLOW)`, ensuring circular directory symlinks (e.g. `ln -s / loop`) are never recursed into.
  - Configurable API: Added `find_file_timeout(dir, target, out_path, maxlen, timeout_ticks, max_depth, max_entries)` in `user/user.h` and `user/ulib.c`.
- **Kernel Fix in `kernel/sysfile.c`**:
  - Updated directory validation check to `(omode & ~O_NOFOLLOW) != O_RDONLY` so directories can be safely inspected with `O_NOFOLLOW` without tripping directory write restrictions.
- **Unified Error Dispatcher**: Implemented `open_or_locate()` and `file_not_found()` in `ulib.c` using raw `write(2, ...)` system calls. This avoids introducing a dependency on `printf.o`, preserving the standalone build of `forktest`. If search times out, it reports `<cmd>: '<path>': search timed out, file not exists`.
- **Command Integration**: Integrated into `cat`, `head`, `tail`, `grep`, `wc`, `cp`, and `sh`. When a file cannot be opened locally, the command searches `/`. If found, it outputs the path; if not found anywhere, it outputs `'<file>': file not exists`.
- **Disk Scaling**: Expanded `FSSIZE` in `kernel/param.h` from 2000 to 4000 blocks to accommodate all userland binaries without overflowing `fs.img` sectors.

---

## 6. How to Run Automated Testing

Inside xv6 shell, run:
```bash
test_utils
test_ps
test_sysinfo
test_trace
test_nice
test_symlink
test_cow
test_tab
test_find
```
Or run the host automated regression script from WSL:
```bash
cd ~/xv6-riscv
python3 -c "
import subprocess, time
p = subprocess.Popen(['make', 'qemu'], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
time.sleep(3)
for cmd in [b'test_utils\n', b'test_ps\n', b'test_sysinfo\n', b'test_trace\n', b'test_nice\n', b'test_symlink\n', b'test_cow\n', b'test_tab\n', b'test_find\n']:
    p.stdin.write(cmd); p.stdin.flush(); time.sleep(1)
p.terminate()
raw, _ = p.communicate(timeout=3)
print(raw.decode('latin1', errors='replace'))
"
```

---

## 7. Changelog (Newest First)

- **2026-11-09**: Added fail-safe resource bounding, depth limits (8), step budget (500), tick timeout (`uptime()`), and symlink loop protection (`O_NOFOLLOW`) to `find_file`, `file_not_found`, and `find.c`. Updated `kernel/sysfile.c` to support `O_RDONLY | O_NOFOLLOW` on directories.
- **2026-11-09**: Implemented Feature 9 (Fast File Search `find`/`search`, intelligent path error discovery in core commands, `FSSIZE=4000`, `test_find`).
- **2026-11-09**: Implemented Feature 8 (Shell Tab-Completion and interactive line editor in `user/sh.c` and `kernel/console.c`, added `test_tab`).
- **2026-11-09**: Added `uptime` CLI utility, built and verified automated test runners (`test_ps`, `test_sysinfo`, `test_trace`, `test_utils`).
- **2026-11-09**: Implemented Feature 7 (Copy-on-Write fork) with `pageref` physical frame refcounts and store page fault handler.
- **2026-11-09**: Implemented Feature 6 (`symlink` syscall, `T_SYMLINK` inode, path traversal loop protection, `ln -s`).
- **2026-11-09**: Implemented Feature 5 (Priority scheduler, `nice` syscall, `test_nice`).
- **2026-11-09**: Implemented Feature 4 (`trace` syscall and runtime execution hooks).
- **2026-11-09**: Implemented Feature 3 (`sysinfo` syscall and CLI command).
- **2026-11-09**: Implemented Feature 2 (`getprocs` syscall and `ps` command).
- **2026-11-09**: Implemented Feature 1 (`touch`, `cp`, `head`, `tail`, `clear` userland utilities).
- **2026-11-09**: Baseline setup and verified QEMU boot under WSL2 Ubuntu 26.04.

