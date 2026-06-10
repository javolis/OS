#!/usr/bin/env bash
# smoke.sh — boot the ISO headlessly, then verify the keyboard works.
#
# 1. Capture COM1 serial output to a log file.
# 2. Drive QEMU's monitor over stdin to type a few keys once the kernel is up.
# 3. Assert the boot marker appeared AND the typed characters were echoed back
#    (the kernel echoes keystrokes to serial as well as VGA).
set -uo pipefail

ISO="${1:-os.iso}"
MARKER="KERNEL_BOOT_OK"
SERIAL_LOG="$(mktemp)"
trap 'rm -f "$SERIAL_LOG"' EXIT

echo "Booting $ISO in QEMU (headless), then typing 'qz'..."

# Feed monitor commands on a delay so the kernel has booted before we type.
# 'sendkey' injects PS/2 scancodes exactly as real keypresses would arrive.
# 'qz' is chosen because it appears nowhere in the kernel's boot output.
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

# The prompt is "> "; after typing we expect the echoed "qz".
if grep -q "qz" "$SERIAL_LOG"; then
    echo "PASS: keyboard input was echoed ('qz')"
else
    echo "FAIL: typed characters were not echoed back" >&2
    fail=1
fi

exit $fail
