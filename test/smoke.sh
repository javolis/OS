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
               c a t dot e l f spc n o t e s dot t x t spc shift-backslash spc u p p e r dot e l f spc shift-backslash spc u p p e r dot e l f ret \
               e m i t dot e l f spc shift-dot spc s dot t x t ret \
               c a t dot e l f spc shift-comma spc s dot t x t ret \
               c a t dot e l f spc n o t e s dot t x t spc shift-backslash spc w c dot e l f ret \
               c a t dot e l f spc n o t e s dot t x t spc shift-backslash spc h e a d dot e l f spc minus 1 spc shift-backslash spc w c dot e l f ret \
               s e t spc x equal w o r k e d ret \
               e c h o dot e l f spc v a r minus shift-4 x ret \
               e x i t ret \
               e c h o spc b a c k ret \
               r u n spc s y s i n f o dot e l f ret \
               r u n spc c a t dot e l f spc n o t e s dot t x t ret \
               r u n spc c l o c k dot e l f ret \
               ctrl-c \
               e c h o spc i n t a c t ret \
               r u n spc d a t e dot e l f ret \
               r u n spc r a m t e s t dot e l f ret \
               r u n spc a p p e n d t e s t dot e l f ret \
               r u n spc k i l l t e s t dot e l f ret \
               r u n spc s p a w n s t o r m dot e l f ret; do
        echo "sendkey $key"
        sleep 0.2
    done
    sleep 8
    echo "quit"
} | timeout 240 qemu-system-i386 \
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

# Exit codes: runner waits on exitcode.elf (exits 42) and reports the
# status sys_wait returned.
if grep -q "runner: child status=42" "$SERIAL_LOG"; then
    echo "PASS: sys_wait returned the child exit code"
else
    echo "FAIL: exit code did not reach the parent" >&2
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

# Three-stage pipeline inside ush: 'cat notes.txt | upper | upper'.
# Uppercasing is idempotent, so correct output through all three stages
# still yields the shouted line; a broken middle stage drops it or hangs.
if grep -q "NEXT STOP: PIPES." "$SERIAL_LOG"; then
    echo "PASS: ush multi-stage pipeline ran (cat | upper | upper)"
else
    echo "FAIL: pipeline produced no uppercased output" >&2
    fail=1
fi

# RTC: date.elf prints a well-formed wall-clock timestamp from the CMOS
# clock (value depends on the host; only the format is asserted).
if grep -qE "date: [0-9]{4}-[0-9]{2}-[0-9]{2} [0-9]{2}:[0-9]{2}:[0-9]{2}" "$SERIAL_LOG"; then
    echo "PASS: RTC reported a well-formed timestamp"
else
    echo "FAIL: date produced no valid timestamp" >&2
    fail=1
fi

# Shell variables: 'set x=worked' then 'echo.elf var-$x'. The expanded
# token 'var-worked' appears in no typed command (the set line has
# 'worked', the echo line has 'var-$x'), so it proves $-expansion ran.
if grep -q "var-worked" "$SERIAL_LOG"; then
    echo "PASS: ush expanded a shell variable"
else
    echo "FAIL: shell variable not expanded" >&2
    fail=1
fi

# Coreutils: 'cat notes.txt | wc' counts notes.txt's 3 lines; then
# 'cat notes.txt | head -1 | wc' cuts to the first line (9 words). The
# wc output format ('<lines> <words> <bytes>') makes both deterministic.
if grep -qE "^3 [0-9]+ [0-9]+$" "$SERIAL_LOG"; then
    echo "PASS: wc counted piped input (3 lines)"
else
    echo "FAIL: wc did not report 3 lines" >&2
    fail=1
fi
if grep -qE "^1 9 [0-9]+$" "$SERIAL_LOG"; then
    echo "PASS: head limited the stream (cat | head -1 | wc = 1 line)"
else
    echo "FAIL: head did not limit to one line" >&2
    fail=1
fi

# Redirection inside ush: 'emit.elf > s.txt' sends the sentinel to a file
# (NOT the console), then 'cat.elf < s.txt' reads it back to the console.
# The sentinel never appears in any typed command, so its presence proves
# both > (create+write) and < (open+read) moved data through the file.
if grep -q "redirect-sentinel-ok" "$SERIAL_LOG"; then
    echo "PASS: ush redirection > and < round-tripped a file"
else
    echo "FAIL: redirection did not move data through a file" >&2
    fail=1
fi

# ramfs: create a file, write, reopen, read back. The readback line
# only appears if the writable filesystem round-trips.
if grep -q "ramtest: ramfs round-trip works" "$SERIAL_LOG"; then
    echo "PASS: ramfs create/write/read round-trip"
else
    echo "FAIL: ramfs round-trip did not work" >&2
    fail=1
fi

# Append + unlink: create+write, append, read back 'A\nB\n', then unlink
# and confirm the reopen fails. All in one self-contained program.
if grep -q "appendtest: append+unlink ok" "$SERIAL_LOG"; then
    echo "PASS: ramfs append and unlink"
else
    echo "FAIL: append or unlink misbehaved" >&2
    fail=1
fi

# Userland kill: a process spawns a child, kills it by pid via SYS_KILL,
# and wait reports the killed status (-1).
if grep -q "killtest: killed child cleanly" "$SERIAL_LOG"; then
    echo "PASS: userland SYS_KILL terminated a child"
else
    echo "FAIL: userland kill did not work" >&2
    fail=1
fi

# Stress test: many spawn/wait + pipe cycles, self-checking that every
# frame came back. A leak (or a teardown crash) fails this.
if grep -q "spawnstorm: no leak" "$SERIAL_LOG"; then
    echo "PASS: spawnstorm completed with no frame leak"
else
    echo "FAIL: spawnstorm reported a leak or did not finish" >&2
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

# File descriptors: cat.elf opens notes.txt from the initrd and streams
# it to stdout in chunks.
if grep -q "initrd now holds plain files" "$SERIAL_LOG" \
        && grep -q "Next stop: pipes." "$SERIAL_LOG"; then
    echo "PASS: cat read an initrd file through file descriptors"
else
    echo "FAIL: cat did not stream the initrd file" >&2
    fail=1
fi

# User libc: sysinfo.elf formats live kernel stats with uprintf.
if grep -qE "sysinfo: ticks=[0-9]+ frames=[0-9]+/[0-9]+ tasks=[0-9]+" "$SERIAL_LOG"; then
    echo "PASS: user libc formatted sysinfo output"
else
    echo "FAIL: sysinfo output missing or malformed" >&2
    fail=1
fi

# Ctrl+C: a foreground clock is interrupted mid-run; the ^C marker
# appears and the shell answers afterwards. 'clock: done' must still be
# seen exactly once (kill-test run) — this third clock never finishes.
ctrlc_line=$(grep -nF "^C" "$SERIAL_LOG" | head -n 1 | cut -d: -f1)
intact_line=$(grep -nx "intact" "$SERIAL_LOG" | head -n 1 | cut -d: -f1)
final_done_count=$(grep -c "clock: done" "$SERIAL_LOG")
if [ -n "$ctrlc_line" ] && [ -n "$intact_line" ] \
        && [ "$ctrlc_line" -lt "$intact_line" ] \
        && [ "$final_done_count" -eq 1 ]; then
    echo "PASS: Ctrl+C killed the foreground task (shell intact)"
else
    echo "FAIL: Ctrl+C handling broken (done=${final_done_count}x)" >&2
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
