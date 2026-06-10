# Cross-compiler toolchain. Override on the command line if yours differs,
# e.g. `make CC=i686-elf-gcc`. See https://wiki.osdev.org/GCC_Cross-Compiler
CC      := i686-elf-gcc
AS      := nasm
LD      := i686-elf-gcc

CFLAGS  := -std=gnu11 -ffreestanding -O2 -Wall -Wextra -Iinclude
ASFLAGS := -f elf32
LDFLAGS := -ffreestanding -O2 -nostdlib -T linker.ld

KERNEL  := kernel.elf
ISO     := os.iso

OBJ := boot/boot.o kernel/kernel.o

.PHONY: all iso run clean

all: $(KERNEL)

$(KERNEL): $(OBJ) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJ) -lgcc

%.o: %.s
	$(AS) $(ASFLAGS) $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

iso: $(KERNEL)
	mkdir -p isodir/boot/grub
	cp $(KERNEL) isodir/boot/kernel.elf
	cp grub.cfg isodir/boot/grub/grub.cfg
	grub-mkrescue -o $(ISO) isodir

run: iso
	qemu-system-i386 -cdrom $(ISO)

clean:
	rm -rf $(OBJ) $(KERNEL) $(ISO) isodir
