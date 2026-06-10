# OS

[![Build](https://github.com/javolis/OS/actions/workflows/build.yml/badge.svg)](https://github.com/javolis/OS/actions/workflows/build.yml)

A hobby operating system / kernel, built from scratch.

## Status

Freestanding i686 kernel that boots via the Multiboot protocol (GRUB). It sets
up its own GDT and IDT, remaps the PIC, handles CPU exceptions, and reads the
PS/2 keyboard over IRQ1 — typed characters are echoed to both the VGA console
(with scrolling + hardware cursor) and the serial port.

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
make test       # headless boot smoke test (asserts the kernel runs)
make clean
```

## Layout

```
boot/        bootstrap assembly (multiboot header, entry point)
kernel/      kernel sources
  interrupt.s  GDT/IDT loaders + ISR/IRQ entry stubs
  gdt.c        flat segment descriptors
  idt.c        interrupt descriptor table
  isr.c        exception + IRQ dispatch
  pic.c        8259A PIC (remap, EOI)
  vga.c        VGA text console (scroll + cursor)
  serial.c     COM1 serial output
  keyboard.c   PS/2 keyboard driver
  kernel.c     entry point (kernel_main)
include/     shared headers
test/        smoke.sh — headless QEMU boot + keyboard test
linker.ld    kernel link script
Makefile     build orchestration
```

## References

- https://wiki.osdev.org — the canonical hobby-OS resource
- https://os.phil-opp.com — "Writing an OS in Rust" (good architectural reading)

## License

MIT — see [LICENSE](LICENSE).
