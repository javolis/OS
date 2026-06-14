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
             user/crash.elf user/echo.elf user/greet.elf user/runner.elf \
             user/ush.elf user/sysinfo.elf user/exitcode.elf user/cat.elf \
             user/upper.elf user/spawnstorm.elf user/date.elf \
             user/ramtest.elf user/emit.elf user/wc.elf user/head.elf \
             user/appendtest.elf user/killtest.elf user/ls.elf \
             user/runtests.elf user/devtest.elf user/grep.elf \
             user/coretest.elf user/sort.elf user/uniq.elf user/tee.elf \
             user/nl.elf user/rev.elf user/yes.elf user/true.elf \
             user/false.elf user/ulibtest.elf user/sbrktest.elf \
             user/malloctest.elf user/stacktest.elf user/stdiotest.elf \
             user/bigbin.elf user/calc.elf user/kv.elf user/template.elf \
             user/apptest.elf user/fbtest.elf user/ugfxtest.elf
INITRD_FILES := $(USER_ELFS) user/notes.txt user/demo.ush \
                user/words.txt user/tools.ush
INITRD    := initrd.tar

.PHONY: all iso run test test-uefi clean

all: $(KERNEL)

$(KERNEL): $(OBJ) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJ) -lgcc

%.o: %.s
	$(AS) $(ASFLAGS) $< -o $@

%.o: %.c $(HDRS)
	$(CC) $(CFLAGS) -c $< -o $@

user/%.o: user/%.c user/usys.h user/ulib.h user/ugfx.h include/font8x8.h
	$(CC) -std=gnu11 -ffreestanding -O2 -Wall -Wextra -Iinclude -c $< -o $@

user/%.elf: user/%.o user/ulib.o user/user.ld
	$(LD) -ffreestanding -O2 -nostdlib -T user/user.ld -o $@ $< user/ulib.o -lgcc

$(INITRD): $(INITRD_FILES)
	tar --format=ustar -cf $@ -C user $(notdir $(INITRD_FILES))

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

# Boot the same ISO under UEFI firmware (OVMF) — the Hyper-V Gen 2 path.
test-uefi: iso
	bash test/smoke-uefi.sh $(ISO)

clean:
	rm -rf $(OBJ) $(KERNEL) $(ISO) $(INITRD) isodir user/*.o user/*.elf
