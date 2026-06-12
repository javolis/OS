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

OBJ  := $(patsubst %.s,%.o,$(wildcard boot/*.s)) \
        $(patsubst %.c,%.o,$(wildcard kernel/*.c))
HDRS := $(wildcard include/*.h)

# Userland: standalone ELF executables, embedded into the kernel image by
# boot/userblobs.s (no filesystem yet).
USER_ELFS := user/hello_a.elf user/hello_b.elf

.PHONY: all iso run test clean

all: $(KERNEL)

$(KERNEL): $(OBJ) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJ) -lgcc

%.o: %.s
	$(AS) $(ASFLAGS) $< -o $@

%.o: %.c $(HDRS)
	$(CC) $(CFLAGS) -c $< -o $@

user/%.o: user/%.c user/usys.h
	$(CC) -std=gnu11 -ffreestanding -O2 -Wall -Wextra -c $< -o $@

user/%.elf: user/%.o user/user.ld
	$(LD) -ffreestanding -O2 -nostdlib -T user/user.ld -o $@ $< -lgcc

boot/userblobs.o: $(USER_ELFS)

iso: $(KERNEL)
	mkdir -p isodir/boot/grub
	cp $(KERNEL) isodir/boot/kernel.elf
	cp grub.cfg isodir/boot/grub/grub.cfg
	grub-mkrescue -o $(ISO) isodir

run: iso
	qemu-system-i386 -cdrom $(ISO)

test: iso
	bash test/smoke.sh $(ISO)

clean:
	rm -rf $(OBJ) $(KERNEL) $(ISO) isodir user/*.o user/*.elf
