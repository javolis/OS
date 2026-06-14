#!/usr/bin/env bash
# smoke-disk.sh — boot the UEFI disk image under OVMF as a hard disk. This
# is the Hyper-V Generation 2 path: UEFI firmware reads the GPT, finds the
# EFI System Partition, runs /EFI/BOOT/BOOTX64.EFI (GRUB), which loads the
# kernel from the same partition. If this boots under OVMF from a disk, it
# is strong evidence it boots a Gen 2 VM (which CI cannot run directly).
set -uo pipefail

DISK="${1:-os.img}"
MARKER="KERNEL_BOOT_OK"
SERIAL_LOG="$(mktemp)"
trap 'rm -f "$SERIAL_LOG"' EXIT

if [ ! -f "$DISK" ]; then
    echo "FAIL: disk image not found: $DISK" >&2
    exit 1
fi

CODE=""
for c in /usr/share/OVMF/OVMF_CODE.fd /usr/share/ovmf/OVMF.fd \
         /usr/share/qemu/OVMF.fd /usr/share/edk2-ovmf/x64/OVMF_CODE.fd; do
    [ -f "$c" ] && CODE="$c" && break
done
if [ -z "$CODE" ]; then
    echo "FAIL: no OVMF firmware found" >&2
    exit 1
fi

echo "Booting $DISK under UEFI from a hard disk (OVMF: $CODE)..."
timeout 60 qemu-system-x86_64 \
    -bios "$CODE" \
    -drive format=raw,file="$DISK" \
    -display none \
    -serial "file:$SERIAL_LOG" \
    -no-reboot >/dev/null 2>&1

echo "----- serial output -----"
cat "$SERIAL_LOG"
echo
echo "-------------------------"

fail=0
if grep -q "$MARKER" "$SERIAL_LOG"; then
    echo "PASS: kernel booted from a UEFI disk (reached '$MARKER')"
else
    echo "FAIL: kernel did not boot from the UEFI disk image" >&2
    fail=1
fi

# Disk UEFI boot has no legacy VGA text, so the kernel must be on the GOP
# framebuffer — same condition as Hyper-V Gen 2.
if grep -q "framebuffer console:" "$SERIAL_LOG"; then
    echo "PASS: framebuffer console active on the UEFI disk boot"
else
    echo "FAIL: no framebuffer console on the UEFI disk boot" >&2
    fail=1
fi

exit $fail
