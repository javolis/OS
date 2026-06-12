#!/usr/bin/env bash
# smoke.sh — boot the ISO headlessly in QEMU, then verify keyboard input.
#
# 1. Capture COM1 serial output to a log file (the kernel writes a boot
#    marker there; see kernel/kernel.c).
# 2. Drive QEMU's monitor over stdin to inject keystrokes once the kernel is
#    up — 'sendkey' delivers real PS/2 scancodes, exercising the IRQ path.
# 3. Assert the boot marker appeared AND the typed characters were echoed
#    back (the kernel echoes keystrokes to serial as well as VGA).
#
# The isa-debug-exit device is deliberately absent: the kernel's qemu_exit
# no-ops without it, so the kernel idles after boot and we can type at it
# before quitting via the monitor.
set -uo pipefail

ISO="${1:-os.iso}"
MARKER="KERNEL_BOOT_OK"
SERIAL_LOG="$(mktemp)"
trap 'rm -f "$SERIAL_LOG"' EXIT

if [ ! -f "$ISO" ]; then
    echo "FAIL: ISO not found: $ISO" >&2
    exit 1
fi

echo "Booting $ISO in QEMU (headless), then typing 'qz'..."

# Feed monitor commands on a delay so the kernel has booted before we type.
# 'qz' is chosen because it appears nowhere in the kernel's boot output, so
# the grep below can't false-positive on boot messages.
{
    sleep 5
    echo "sendkey q"
    echo "sendkey z"
    sleep 1
    echo "quit"
} | timeout 40 qemu-system-i386 \
        -cdrom "$ISO" \
        -display none \
        -serial "file:$SERIAL_LOG" \
        -monitor stdio \
        -no-reboot >/dev/null 2>&1

echo "----- serial output -----"
cat "$SERIAL_LOG"
echo
echo "-------------------------"

fail=0
if grep -q "$MARKER" "$SERIAL_LOG"; then
    echo "PASS: kernel reached boot marker '$MARKER'"
else
    echo "FAIL: boot marker '$MARKER' not found" >&2
    fail=1
fi

if grep -q "qz" "$SERIAL_LOG"; then
    echo "PASS: keyboard input was echoed ('qz')"
else
    echo "FAIL: typed characters were not echoed back" >&2
    fail=1
fi

exit $fail
