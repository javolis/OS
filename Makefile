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

# Userland: standalone ELF executables, shipped to the kernel in a USTAR
# initrd that GRUB loads as a Multiboot module.
USER_ELFS := user/hello_a.elf user/hello_b.elf user/clock.elf \
             user/crash.elf
INITRD    := initrd.tar

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

$(INITRD): $(USER_ELFS)
	tar --format=ustar -cf $@ -C user $(notdir $(USER_ELFS))

iso: $(KERNEL) $(INITRD)
	mkdir -p isodir/boot/grub
	cp $(KERNEL) isodir/boot/kernel.elf
	cp $(INITRD) isodir/boot/initrd.tar
	cp grub.cfg isodir/boot/grub/grub.cfg
	grub-mkrescue -o $(ISO) isodir

run: iso
	qemu-system-i386 -cdrom $(ISO)

test: iso
	bash test/smoke.sh $(ISO)

clean:
	rm -rf $(OBJ) $(KERNEL) $(ISO) $(INITRD) isodir user/*.o user/*.elf
