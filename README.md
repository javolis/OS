# OS

A hobby operating system / kernel, built from scratch.

## Status

Early bootstrap. Currently a minimal freestanding i686 kernel that boots via
the Multiboot protocol (GRUB) and prints to the VGA text buffer.

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
