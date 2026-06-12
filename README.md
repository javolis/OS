# OS

[![Build](https://github.com/javolis/OS/actions/workflows/build.yml/badge.svg)](https://github.com/javolis/OS/actions/workflows/build.yml)

A hobby operating system / kernel, built from scratch.

## Status

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
into its own virtual region. Ring 3 works: at boot the kernel drops into
an embedded user-mode program that prints through an int 0x80 write
syscall and returns via exit (GDT user segments + TSS, DPL-3 syscall
gate, user-bit page mappings).

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
make test       # headless smoke test (boot marker + shell responds)
make clean
```

## Layout

```
boot/        bootstrap assembly (multiboot header, entry point)
kernel/      kernel C sources
include/     shared headers
linker.ld    kernel link script
Makefile     build orchestration
```

## References

- https://wiki.osdev.org — the canonical hobby-OS resource
- https://os.phil-opp.com — "Writing an OS in Rust" (good architectural reading)

## License

MIT — see [LICENSE](LICENSE).
