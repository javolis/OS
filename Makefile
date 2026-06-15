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
DISK    := os.img
VHDX    := os.vhdx

OBJ  := $(patsubst %.s,%.o,$(wildcard boot/*.s)) \
        $(patsubst %.c,%.o,$(wildcard kernel/*.c))
HDRS := $(wildcard include/*.h)

# Anti-aliased font: generated from a libre TTF by tools/genfont.py at build
# time (needs python3 + Pillow + a DejaVu font). Userland (ugfx text) uses
# it; it is a build artifact, not committed.
FONT_AA := include/font_aa.h

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
             user/apptest.elf user/fbtest.elf user/ugfxtest.elf \
             user/gfxdemo.elf user/inputdemo.elf user/gfxcap.elf \
             user/ping.elf user/nslookup.elf user/dhcp.elf \
             user/tcpecho.elf user/netcap.elf user/dirtest.elf \
             user/envtest.elf user/envchild.elf user/badptr.elf \
             user/hardcap.elf user/aafonttest.elf user/avuitest.elf \
             user/avwalltest.elf user/avolis.elf
INITRD_FILES := $(USER_ELFS) user/notes.txt user/demo.ush \
                user/words.txt user/tools.ush
INITRD    := initrd.tar

.PHONY: all iso disk vhdx run test test-uefi test-disk clean

all: $(KERNEL)

$(KERNEL): $(OBJ) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJ) -lgcc

%.o: %.s
	$(AS) $(ASFLAGS) $< -o $@

%.o: %.c $(HDRS)
	$(CC) $(CFLAGS) -c $< -o $@

$(FONT_AA): tools/genfont.py
	python3 tools/genfont.py > $@

user/%.o: user/%.c user/usys.h user/ulib.h user/ugfx.h include/font8x8.h \
          $(FONT_AA)
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

# UEFI-bootable GPT disk image: a FAT EFI System Partition holding a
# standalone GRUB EFI loader plus the kernel and initrd. This boots from a
# hard disk under UEFI, which is what Hyper-V Generation 2 wants (the
# grub-mkrescue ISO's El Torito UEFI image is not accepted there).
disk: $(DISK)

$(DISK): $(KERNEL) $(INITRD) grub-efi.cfg
	grub-mkstandalone -O x86_64-efi -o BOOTX64.EFI \
	    --modules="part_gpt fat multiboot normal search search_fs_file configfile all_video gfxterm boot" \
	    "boot/grub/grub.cfg=grub-efi.cfg"
	rm -f esp.img
	dd if=/dev/zero of=esp.img bs=1M count=64
	mformat -i esp.img -F ::
	mmd -i esp.img ::/EFI ::/EFI/BOOT ::/boot
	mcopy -i esp.img BOOTX64.EFI ::/EFI/BOOT/BOOTX64.EFI
	mcopy -i esp.img $(KERNEL) ::/boot/kernel.elf
	mcopy -i esp.img $(INITRD) ::/boot/initrd.tar
	rm -f $(DISK)
	dd if=/dev/zero of=$(DISK) bs=1M count=80
	sgdisk -n 1:2048:0 -t 1:ef00 -c 1:"EFI System Partition" $(DISK)
	dd if=esp.img of=$(DISK) bs=512 seek=2048 conv=notrunc
	rm -f BOOTX64.EFI esp.img

# Convert the raw disk image to VHDX for attaching to a Hyper-V Gen 2 VM.
vhdx: $(VHDX)
$(VHDX): $(DISK)
	qemu-img convert -O vhdx $(DISK) $(VHDX)

# Boot the disk image under UEFI (OVMF) as a hard disk — the Gen 2 path.
test-disk: disk
	bash test/smoke-disk.sh $(DISK)

clean:
	rm -rf $(OBJ) $(KERNEL) $(ISO) $(DISK) $(VHDX) BOOTX64.EFI esp.img \
	    $(FONT_AA) $(INITRD) isodir user/*.o user/*.elf
