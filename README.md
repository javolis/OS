# OS

[![Build](https://github.com/javolis/OS/actions/workflows/build.yml/badge.svg)](https://github.com/javolis/OS/actions/workflows/build.yml)

A hobby operating system / kernel, built from scratch.

## Status

Displays on a linear framebuffer (with an 8x8 font console) when the
bootloader provides one — so it shows on modern UEFI VMs too — and falls
back to legacy VGA text otherwise; serial mirrors everything.

Early bootstrap. A freestanding i686 higher-half kernel (linked at
0xC0100000, loaded at 1 MiB) that boots via the Multiboot protocol (GRUB),
installs its own GDT and IDT (with handlers for all 32 CPU exceptions),
and prints to the VGA text buffer and COM1 serial via a small `kprintf`.
Unhandled exceptions panic with a register dump instead of
triple-faulting. The 8259 PIC is remapped and hardware IRQs are live: a
100 Hz PIT tick and a PS/2 keyboard feeding a tiny interactive shell
(help / echo / clear / ticks / meminfo / sleep / uptime / history) with
line editing and arrow-key history. A bitmap physical frame allocator is
seeded from the Multiboot memory map, all physical RAM is offset-mapped in
the higher half (the identity mapping is dropped after boot, so NULL
dereferences fault), and a kernel heap (kmalloc / kfree) grows on demand
into its own virtual region. Preemptive multitasking works: at boot the
kernel loads two real ELF executables from a USTAR initrd (compiled
separately from C in user/, shipped as a GRUB Multiboot module), each
into its own address space (own page directory, kernel half shared, both
linked at 0x08048000), and the PIT round-robins between them — each task has its
own kernel stack, the TSS esp0 follows the running task, and the boot
flow doubles as the idle task. The scheduler stays on after boot: the
shell is the idle task and its `run <file>` command spawns initrd
programs live with arguments — argc/argv are built on the user stack per
the C ABI; foreground programs own the keyboard (sys_readline gives them
line-edited input) while `run ... &` runs in the background (zombies are
reaped at the prompt; kprintf is interrupt-atomic so output lines never
interleave; ps shows the task table with names; kill terminates a task
by pid and Ctrl+C kills the foreground task). User code talks to the
kernel via int 0x80 (write / exit / sleep / getpid / readline /
spawn / wait / sysinfo / open / read / writefd / close / pipe / spawn_io
/ time — user programs can launch other programs, collect their exit
codes via wait (which also reclaims the child), stream initrd files
through per-process file descriptors (cat.elf), connect through pipes,
read/write a small in-RAM filesystem (ramfs, alongside the read-only
initrd, plus /dev/null and /dev/zero) and list it with sizes (ls.elf via
readdir), filter text through coreutils (cat / wc / head / grep / sort / uniq /
tee / nl / rev / yes) composed in pipelines, read the wall clock
from the CMOS RTC (date.elf), and self-test the whole syscall surface
(runtests.elf); ush.elf is a
complete shell running in ring 3 with multi-stage `a | b | c` pipelines and
`>` / `>>` / `<` file redirection, `rm` / `kill` / `set NAME=VAL`
builtins with `$VAR` expansion, and can run script files (`ush demo.ush`);
plus a tiny user libc providing uprintf); sleep blocks properly — the scheduler runs other tasks,
including the shell, until the wake tick. Faults are isolated: a ring-3
exception kills only the offending task (text/rodata segments are mapped
read-only per ELF flags, and syscalls validate user pointers), while
ring-0 faults still panic with a register dump. Teardown reclaims every
frame.

## Prerequisites (Linux dev host)

Install a cross-compiler toolchain and the build/emulation tools:

```sh
# Debian/Ubuntu
sudo apt install build-essential nasm xorriso grub-pc-bin grub-common qemu-system-x86
```

For a proper `i686-elf` / `x86_64-elf` cross-compiler, see
https://wiki.osdev.org/GCC_Cross-Compiler (recommended over the host gcc).

## Build

```sh
make            # build the kernel
make iso        # produce a bootable os.iso
make run        # boot the ISO in QEMU
make test       # headless smoke test (boot, shell, pipes, leak check)
make clean
```

## Layout

```
boot/        bootstrap assembly (multiboot header, entry point, stubs)
kernel/      kernel C sources
include/     shared kernel headers
user/        userland programs (built as standalone ELF executables)
linker.ld    kernel link script
Makefile     build orchestration
```

## Running it in a VM

`make iso` produces `os.iso`, a bootable CD image. Boot it however you like:

```sh
make run                              # QEMU (what CI uses)
qemu-system-i386 -cdrom os.iso        # QEMU directly
```

It also boots in VirtualBox/VMware and Hyper-V. The ISO is hybrid (BIOS +
UEFI), so it works on both firmware types:

- **Hyper-V Generation 1** (BIOS): works out of the box.
- **Hyper-V Generation 2** (UEFI): turn off Settings → Security → *Enable
  Secure Boot* first (our bootloader isn't Microsoft-signed).

Create a VM with ~128 MB RAM and `os.iso` as the optical drive. You'll
land at the kernel shell; type `help`, then `run ush.elf` for the
user-mode shell (pipelines, redirection, history, `run <prog>`). If you
don't want to build the toolchain locally, download the `os-iso` artifact
from any green [CI run](https://github.com/javolis/OS/actions). CI boots
the ISO under both BIOS (QEMU) and UEFI (OVMF) to keep both paths working.

## Writing your own app

Userland programs are freestanding ELF executables that talk to the
kernel through `int 0x80` (see `user/usys.h`) and may use the small C
library `user/ulib.h` (`uprintf`, `umalloc`/`ufree`, the `ustr*`/`umem*`
helpers, and the `ufopen`/`ufgets` stdio reader). To add one:

1. Copy `user/template.c` to `user/myapp.c` and edit `_start`.
2. Add `user/myapp.elf` to `USER_ELFS` in the `Makefile`.
3. `make iso`, boot, and `run myapp.elf` (or `run myapp.elf arg1 arg2`).

```c
#include "ulib.h"
void _start(int argc, char **argv) {
    uprintf("hello from %s\n", argc >= 2 ? argv[1] : "my app");
    sys_exit(0);
}
```

Programs get argc/argv, a 16 KiB stack, an on-demand heap (`umalloc`), and
fds 0/1 wired to the console (or to pipes/files when launched with `|`,
`<`, `>` from a shell). A ring-3 fault kills only your program, not the
system, so it's safe to experiment.

## References

- https://wiki.osdev.org — the canonical hobby-OS resource
- https://os.phil-opp.com — "Writing an OS in Rust" (good architectural reading)

## License

MIT — see [LICENSE](LICENSE).
