#!/usr/bin/env bash
# smoke.sh — boot the ISO headlessly in QEMU and confirm the kernel runs.
#
# The kernel writes a marker string to COM1 (see kernel/kernel.c) and then
# triggers QEMU's isa-debug-exit device, so this returns promptly instead of
# relying on a timeout. We grep the captured serial output for the marker.
set -uo pipefail

ISO="${1:-os.iso}"
MARKER="KERNEL_BOOT_OK"

if [ ! -f "$ISO" ]; then
    echo "FAIL: ISO not found: $ISO" >&2
    exit 1
fi

echo "Booting $ISO in QEMU (headless)..."
output=$(timeout 30 qemu-system-i386 \
    -cdrom "$ISO" \
    -display none \
    -serial stdio \
    -no-reboot \
    -device isa-debug-exit,iobase=0xf4,iosize=0x04 2>&1)

echo "----- serial output -----"
echo "$output"
echo "-------------------------"

if echo "$output" | grep -q "$MARKER"; then
    echo "PASS: kernel reached boot marker '$MARKER'"
    exit 0
fi

echo "FAIL: boot marker '$MARKER' not found in serial output" >&2
exit 1
