#!/usr/bin/env bash
# smoke.sh — boot the ISO headlessly in QEMU, then exercise the shell.
#
# 1. Capture COM1 serial output to a log file (the kernel writes a boot
#    marker there; see kernel/kernel.c).
# 2. Drive QEMU's monitor over stdin to type "help<enter>" once the kernel
#    is up — 'sendkey' delivers real PS/2 scancodes, exercising the IRQ
#    path, the keyboard ring buffer, and the shell's line editing.
# 3. Assert the boot marker appeared AND the shell answered the command
#    (the shell writes to serial as well as VGA).
#
# The isa-debug-exit device is deliberately absent: the kernel's qemu_exit
# no-ops without it, so the kernel sits in the shell after boot and we can
# type at it before quitting via the monitor.
set -uo pipefail

ISO="${1:-os.iso}"
MARKER="KERNEL_BOOT_OK"
SERIAL_LOG="$(mktemp)"
trap 'rm -f "$SERIAL_LOG"' EXIT

if [ ! -f "$ISO" ]; then
    echo "FAIL: ISO not found: $ISO" >&2
    exit 1
fi

echo "Booting $ISO in QEMU (headless), then typing 'help<enter>'..."

# Feed monitor commands on a delay so the kernel has booted before we type,
# pacing the keys so press/release pairs don't overlap. The sequence runs
# 'help', 'meminfo', and 'sleep 50', then presses Up three times (history
# recall back to 'help') and Enter to re-run it.
{
    sleep 5
    for key in h e l p ret \
               m e m i n f o ret \
               s l e e p spc 5 0 ret \
               up up up ret; do
        echo "sendkey $key"
        sleep 0.3
    done
    sleep 1
    echo "quit"
} | timeout 60 qemu-system-i386 \
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

# The typed command is echoed as "> help"; the response line below is
# printed only by the shell's help command, so it can't false-positive.
if grep -q "commands: help" "$SERIAL_LOG"; then
    echo "PASS: shell answered 'help'"
else
    echo "FAIL: shell did not answer the 'help' command" >&2
    fail=1
fi

if grep -q "frames free" "$SERIAL_LOG"; then
    echo "PASS: physical memory manager reported frame stats"
else
    echo "FAIL: no frame stats from 'meminfo'" >&2
    fail=1
fi

# Printed after CR0.PG is set; the shell passing above also proves the
# system stays alive (interrupts, VGA, serial) with paging on.
if grep -q "Paging enabled" "$SERIAL_LOG"; then
    echo "PASS: paging enabled"
else
    echo "FAIL: paging-enabled marker not found" >&2
    fail=1
fi

# Printed only after kmalloc/kfree round-trips (including a heap growth
# that maps fresh frames into the heap's virtual region).
if grep -q "self-test passed" "$SERIAL_LOG"; then
    echo "PASS: kernel heap self-test"
else
    echo "FAIL: kernel heap self-test marker not found" >&2
    fail=1
fi

if grep -q "slept 50 ms" "$SERIAL_LOG"; then
    echo "PASS: sleep command completed"
else
    echo "FAIL: sleep command did not complete" >&2
    fail=1
fi

# The int3 self-test prints the faulting eip; in the higher half the kernel
# executes at 0xC01xxxxx, so the address must start with c01.
if grep -q "breakpoint at eip=c01" "$SERIAL_LOG"; then
    echo "PASS: kernel executes in the higher half"
else
    echo "FAIL: kernel eip is not in the higher half" >&2
    fail=1
fi

# Printed by the embedded ring-3 program through the write syscall, then
# the kernel confirms the exit syscall brought it back.
if grep -q "Hello from ring 3" "$SERIAL_LOG" \
        && grep -q "Back in ring 0" "$SERIAL_LOG"; then
    echo "PASS: ring-3 round trip (user program + syscalls)"
else
    echo "FAIL: ring-3 round trip did not complete" >&2
    fail=1
fi

# Up-arrow history recall re-runs 'help', so its response must appear twice.
help_count=$(grep -c "commands: help" "$SERIAL_LOG")
if [ "$help_count" -ge 2 ]; then
    echo "PASS: history recall re-ran 'help' (seen ${help_count}x)"
else
    echo "FAIL: history recall did not re-run 'help' (seen ${help_count}x)" >&2
    fail=1
fi

exit $fail
