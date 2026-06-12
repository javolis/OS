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
# 'help', 'ls', 'meminfo', and 'sleep 50', then presses Up four times
# (history recall back to 'help') and Enter to re-run it.
{
    sleep 8
    for key in h e l p ret \
               l s ret \
               m e m i n f o ret \
               s l e e p spc 5 0 ret \
               up up up up ret \
               r u n spc h e l l o shift-minus a dot e l f ret \
               r u n spc c l o c k dot e l f spc shift-7 ret \
               p s ret \
               r u n spc c r a s h dot e l f ret \
               e c h o spc s u r v i v e d ret \
               r u n spc e c h o dot e l f spc p r o o f spc o f spc a r g v ret \
               r u n spc c l o c k dot e l f spc shift-7 ret \
               k i l l spc 7 ret \
               r u n spc g r e e t dot e l f ret \
               c i ret \
               r u n spc r u n n e r dot e l f ret \
               r u n spc u s h dot e l f ret \
               e c h o dot e l f spc h i spc f r o m spc u s h ret \
               e x i t ret \
               e c h o spc b a c k ret; do
        echo "sendkey $key"
        sleep 0.3
    done
    sleep 4
    echo "quit"
} | timeout 120 qemu-system-i386 \
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

# Two preemptively scheduled user ELF executables each print three times.
a_count=$(grep -c "ELF process A says hello" "$SERIAL_LOG")
b_count=$(grep -c "ELF process B says hello" "$SERIAL_LOG")
if [ "$a_count" -ge 3 ] && [ "$b_count" -ge 3 ]; then
    echo "PASS: both user ELF processes completed (A=${a_count}, B=${b_count})"
else
    echo "FAIL: user ELF processes incomplete (A=${a_count}, B=${b_count})" >&2
    fail=1
fi

# Printed by process_spawn only after elf_load validated and mapped the
# executable's PT_LOAD segments.
if grep -q "spawned (ELF entry" "$SERIAL_LOG"; then
    echo "PASS: ELF loader staged the executables"
else
    echo "FAIL: ELF loader did not report a load" >&2
    fail=1
fi

# The boot banner proves the Multiboot module arrived; the ls listing
# proves the tar reader walks it.
if grep -q "^initrd: " "$SERIAL_LOG" \
        && grep -q "hello_a.elf" "$SERIAL_LOG" \
        && grep -q "hello_b.elf" "$SERIAL_LOG"; then
    echo "PASS: initrd loaded and listed"
else
    echo "FAIL: initrd missing or ls did not list it" >&2
    fail=1
fi

# 'run hello_a.elf' typed at the shell spawns a fourth task while the
# scheduler stays live: process A's message must appear 3 more times on
# top of the boot demo's 3.
if [ "$a_count" -ge 6 ]; then
    echo "PASS: run command spawned a process from the shell (A=${a_count})"
else
    echo "FAIL: run command did not produce program output (A=${a_count})" >&2
    fail=1
fi

# The clock program does five 300 ms blocking sleeps.
clock_count=$(grep -c "clock: tick" "$SERIAL_LOG")
if [ "$clock_count" -ge 5 ] && grep -q "clock: done" "$SERIAL_LOG"; then
    echo "PASS: clock completed ${clock_count} blocking sleeps"
else
    echo "FAIL: clock program incomplete (${clock_count} ticks)" >&2
    fail=1
fi

# Blocking proof: ps was typed while the clock still had sleeps left, so
# its output must appear BEFORE the clock finishes. A busy-wait sleep
# would starve the shell and push ps output after 'clock: done'.
ps_line=$(grep -n "PID  STATE" "$SERIAL_LOG" | head -n 1 | cut -d: -f1)
done_line=$(grep -n "clock: done" "$SERIAL_LOG" | head -n 1 | cut -d: -f1)
if [ -n "$ps_line" ] && [ -n "$done_line" ] && [ "$ps_line" -lt "$done_line" ]; then
    echo "PASS: shell responsive while clock sleeps (ps at line ${ps_line}, done at ${done_line})"
else
    echo "FAIL: ps output did not appear while clock was sleeping" >&2
    fail=1
fi

# Fault isolation: crash.elf writes to NULL. The kernel must kill just
# that task (never reaching the program's next line) and the shell must
# still work afterwards ('echo survived' typed after the crash).
if grep -q "crash: writing to NULL" "$SERIAL_LOG" \
        && grep -q "killed:" "$SERIAL_LOG" \
        && ! grep -q "still alive" "$SERIAL_LOG"; then
    echo "PASS: faulting user task was killed, not the kernel"
else
    echo "FAIL: user fault was not isolated" >&2
    fail=1
fi

kill_line=$(grep -n "killed:" "$SERIAL_LOG" | head -n 1 | cut -d: -f1)
survived_line=$(grep -n "^survived" "$SERIAL_LOG" | head -n 1 | cut -d: -f1)
if [ -n "$kill_line" ] && [ -n "$survived_line" ] && [ "$kill_line" -lt "$survived_line" ]; then
    echo "PASS: shell survived the user crash"
else
    echo "FAIL: shell did not respond after the crash" >&2
    fail=1
fi

# argv round trip: echo.elf prints its arguments. Exclude the typed
# command line itself; the program's output may share a line with the
# shell's next prompt, so don't anchor to line start.
if grep -v "run echo" "$SERIAL_LOG" | grep -q "proof of argv"; then
    echo "PASS: argc/argv reached the user program"
else
    echo "FAIL: echo.elf did not print its arguments" >&2
    fail=1
fi

# kill: the second clock instance (pid 7) dies mid-run, so 'clock: done'
# appears exactly once — from the first, completed run.
done_count=$(grep -c "clock: done" "$SERIAL_LOG")
if grep -q "killed pid 7" "$SERIAL_LOG" && [ "$done_count" -eq 1 ]; then
    echo "PASS: kill stopped a running process (done seen ${done_count}x)"
else
    echo "FAIL: kill did not stop the process (done seen ${done_count}x)" >&2
    fail=1
fi

# Interactive input: greet.elf runs in the foreground, owns the keyboard,
# and reads a typed name through sys_readline.
if grep -q "What is your name" "$SERIAL_LOG" \
        && grep -q "Hello, ci!" "$SERIAL_LOG"; then
    echo "PASS: user program read keyboard input via sys_readline"
else
    echo "FAIL: greet did not read keyboard input" >&2
    fail=1
fi

# Spawn/wait: runner spawns echo.elf and waits for it, so its completion
# line must come after the child's output.
child_line=$(grep -n "from runner child" "$SERIAL_LOG" | head -n 1 | cut -d: -f1)
fin_line=$(grep -n "runner: child finished" "$SERIAL_LOG" | head -n 1 | cut -d: -f1)
if [ -n "$child_line" ] && [ -n "$fin_line" ] && [ "$child_line" -lt "$fin_line" ]; then
    echo "PASS: sys_spawn + sys_wait (child before parent continuation)"
else
    echo "FAIL: spawn/wait ordering broken" >&2
    fail=1
fi

# The user-mode shell: its prompt appears, a command typed INTO it runs a
# child whose output comes back, and 'exit' returns the keyboard to the
# kernel shell (which then answers 'echo back').
if grep -q 'ush\$' "$SERIAL_LOG" \
        && grep -v "echo.elf" "$SERIAL_LOG" | grep -q "hi from ush" \
        && grep -q "ush: bye" "$SERIAL_LOG"; then
    echo "PASS: user-mode shell ran a program and exited"
else
    echo "FAIL: user-mode shell session broken" >&2
    fail=1
fi

bye_line=$(grep -n "ush: bye" "$SERIAL_LOG" | head -n 1 | cut -d: -f1)
back_line=$(grep -nx "back" "$SERIAL_LOG" | head -n 1 | cut -d: -f1)
if [ -n "$bye_line" ] && [ -n "$back_line" ] && [ "$bye_line" -lt "$back_line" ]; then
    echo "PASS: kernel shell regained the keyboard after ush exited"
else
    echo "FAIL: kernel shell did not regain the keyboard" >&2
    fail=1
fi

# The CPU-bound spins span many 10 ms slices, so genuine preemption yields
# well over the minimum 3 switches of a purely sequential run.
switches=$(sed -n 's/.*\[sched\] \([0-9][0-9]*\) context switches.*/\1/p' "$SERIAL_LOG" | head -n 1)
if [ -n "$switches" ] && [ "$switches" -ge 5 ]; then
    echo "PASS: preemptive scheduling (${switches} context switches)"
else
    echo "FAIL: too few context switches ('${switches:-none}')" >&2
    fail=1
fi

# Printed only when teardown returned every frame the processes took.
if grep -q "reclaimed cleanly" "$SERIAL_LOG"; then
    echo "PASS: process teardown leaked no frames"
else
    echo "FAIL: process teardown leaked frames" >&2
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
