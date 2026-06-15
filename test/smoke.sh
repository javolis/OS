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
ECHO_PID=""
trap 'rm -f "$SERIAL_LOG"; [ -n "$ECHO_PID" ] && kill "$ECHO_PID" 2>/dev/null' EXIT

if [ ! -f "$ISO" ]; then
    echo "FAIL: ISO not found: $ISO" >&2
    exit 1
fi

# Host TCP echo server for the guest TCP test. The guest reaches it through
# SLIRP guestfwd at 10.0.2.100:9, which QEMU forwards to 127.0.0.1:12345.
if command -v python3 >/dev/null 2>&1; then
    python3 -c "
import socket
s=socket.socket()
s.setsockopt(socket.SOL_SOCKET,socket.SO_REUSEADDR,1)
s.bind(('127.0.0.1',12345))
s.listen(1)
while True:
    c,a=s.accept()
    while True:
        d=c.recv(1024)
        if not d: break
        c.sendall(d)
    c.close()
" &
    ECHO_PID=$!
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
               e c h o dot e l f spc z q x ret \
               up ret \
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
               r u n spc d i r t e s t dot e l f ret \
               r u n spc e n v t e s t dot e l f ret \
               r u n spc b a d p t r dot e l f ret \
               r u n spc h a r d c a p dot e l f ret \
               r u n spc a a f o n t t e s t dot e l f ret \
               r u n spc a v u i t e s t dot e l f ret \
               r u n spc a v w a l l t e s t dot e l f ret \
               r u n spc a v o l i s dot e l f spc t e s t ret \
               ret s d esc p ret ret slash d a t e ret q \
               r u n spc k i l l t e s t dot e l f ret \
               r u n spc l s dot e l f ret \
               r u n spc u s h dot e l f spc d e m o dot u s h ret \
               r u n spc r u n t e s t s dot e l f ret \
               r u n spc d e v t e s t dot e l f ret \
               r u n spc f b t e s t dot e l f ret \
               r u n spc u g f x t e s t dot e l f ret \
               r u n spc g f x d e m o dot e l f ret \
               r u n spc i n p u t d e m o dot e l f ret \
               d d s d q \
               r u n spc g f x c a p dot e l f ret \
               r u n spc d h c p dot e l f ret \
               r u n spc p i n g dot e l f ret \
               r u n spc n s l o o k u p dot e l f ret \
               r u n spc t c p e c h o dot e l f ret \
               r u n spc n e t c a p dot e l f ret \
               r u n spc c o r e t e s t dot e l f ret \
               r u n spc u l i b t e s t dot e l f ret \
               r u n spc s b r k t e s t dot e l f ret \
               r u n spc m a l l o c t e s t dot e l f ret \
               r u n spc s t a c k t e s t dot e l f ret \
               r u n spc s t d i o t e s t dot e l f ret \
               r u n spc b i g b i n dot e l f ret \
               r u n spc t e m p l a t e dot e l f ret \
               r u n spc a p p t e s t dot e l f ret \
               r u n spc u s h dot e l f spc t o o l s dot u s h ret \
               r u n spc s p a w n s t o r m dot e l f ret \
               r u n spc m o u s e t e s t dot e l f ret; do
        echo "sendkey $key"
        sleep 0.2
    done
    # Drive the PS/2 mouse via the QEMU monitor while mousetest polls.
    sleep 1
    echo "mouse_move 160 0"; sleep 0.4
    echo "mouse_move 0 120"; sleep 0.4
    echo "mouse_move 80 60"; sleep 0.4
    echo "mouse_button 1"; sleep 0.4
    echo "mouse_button 0"; sleep 0.4
    sleep 8
    echo "quit"
} | timeout 240 qemu-system-i386 \
        -cdrom "$ISO" \
        -display none \
        -serial "file:$SERIAL_LOG" \
        -monitor stdio \
        -netdev user,id=net0,guestfwd=tcp:10.0.2.100:9-tcp:127.0.0.1:12345 \
        -device rtl8139,netdev=net0 \
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

# Framebuffer console: GRUB (configured for graphics in grub.cfg) hands
# the kernel a linear 32bpp surface; the kernel switches the console to it
# and renders the banner with the 8x8 font. Require it - a 'none' here
# means the graphics handoff regressed and the console would be invisible
# on UEFI.
if grep -q "framebuffer console:" "$SERIAL_LOG" &&
        ! grep -qE "fbcon checksum 00000000$" "$SERIAL_LOG"; then
    echo "PASS: framebuffer console renders glyphs (non-zero checksum)"
else
    echo "FAIL: no working framebuffer console (graphics handoff regressed)" >&2
    fail=1
fi

# Console color: the kernel renders the same glyph in two colors at a
# scratch cell and logs both region checksums. They must both be non-zero
# (the glyph drew) and differ from each other (color actually changed the
# pixels) - proof the framebuffer console honors per-glyph color.
fbcolor=$(grep -oE "fbcolor red=[0-9a-f]{8} cyan=[0-9a-f]{8}" "$SERIAL_LOG" | head -n1)
red=${fbcolor#fbcolor red=}; red=${red% cyan=*}
cyan=${fbcolor##*cyan=}
if [ -n "$fbcolor" ] && [ "$red" != "00000000" ] && [ "$cyan" != "00000000" ] \
        && [ "$red" != "$cyan" ]; then
    echo "PASS: framebuffer console honors color (red=${red} cyan=${cyan})"
else
    echo "FAIL: console color not honored (${fbcolor:-no fbcolor line})" >&2
    fail=1
fi

# PCI enumeration: the bus scan must find the emulated RTL8139 NIC
# (vendor 10ec, device 8139) that QEMU attaches, proving config-space
# access and device discovery work.
if grep -qi "pci: .*10ec:8139" "$SERIAL_LOG"; then
    echo "PASS: PCI enumeration found the RTL8139 NIC"
else
    echo "FAIL: PCI scan did not find the NIC" >&2
    fail=1
fi

# RTL8139 driver: init resets the NIC and reads its MAC over the I/O BAR.
# QEMU's default NIC MAC starts with the 52:54:00 QEMU OUI, so a matching
# MAC line proves register access and driver bring-up.
if grep -qi "rtl8139: MAC 52:54:00" "$SERIAL_LOG"; then
    echo "PASS: RTL8139 driver initialized and read its MAC"
else
    echo "FAIL: RTL8139 driver did not initialize" >&2
    fail=1
fi

# Ethernet round-trip: the kernel broadcasts an ARP request for the SLIRP
# gateway at boot; SLIRP replies, the NIC RX IRQ fires, and the Ethernet
# layer demuxes it. Seeing a received ARP frame (ethertype 0806) proves
# TX, RX-over-IRQ, and frame parsing all work end to end.
if grep -qi "eth: rx ethertype 0806" "$SERIAL_LOG"; then
    echo "PASS: Ethernet TX/RX round-trip (ARP reply received)"
else
    echo "FAIL: no Ethernet frame received from the wire" >&2
    fail=1
fi

# ARP: the kernel requests the gateway's MAC at boot; the reply is parsed
# and cached. Seeing the gateway (10.0.2.2) learned proves ARP packet
# parsing and the cache work.
if grep -q "arp: 10.0.2.2 is " "$SERIAL_LOG"; then
    echo "PASS: ARP resolved and cached the gateway MAC"
else
    echo "FAIL: ARP did not resolve the gateway" >&2
    fail=1
fi

# IPv4: the checksum is the error-prone part, so verify it against the
# canonical RFC 1071 example (0xB861) deterministically at boot. The send,
# routing and demux paths are exercised end to end by ICMP ping next.
if grep -q "ip: checksum self-test ok" "$SERIAL_LOG"; then
    echo "PASS: IPv4 checksum verified against the RFC 1071 vector"
else
    echo "FAIL: IPv4 checksum self-test failed" >&2
    fail=1
fi

# DHCP: dhcp.elf runs the DISCOVER/OFFER/REQUEST/ACK handshake over UDP
# broadcast against SLIRP's DHCP server and applies the lease. SLIRP hands
# out 10.0.2.15, so a bound line with that address proves DHCP works.
if grep -q "dhcp: bound 10.0.2.15" "$SERIAL_LOG"; then
    echo "PASS: DHCP obtained a lease (10.0.2.15)"
else
    echo "FAIL: DHCP did not bind a lease" >&2
    fail=1
fi

# ICMP ping: the first full network round-trip from userland. ping.elf
# sends an echo to the gateway via SYS_PING (which blocks the task until
# the reply arrives over the RX IRQ) and prints the reply. This exercises
# IP send/routing, ICMP, the scheduler block/wake wait, and RX demux.
if grep -q "ping: reply from 10.0.2.2" "$SERIAL_LOG"; then
    echo "PASS: ICMP ping round-trip to the gateway (userland)"
else
    echo "FAIL: ping got no reply" >&2
    fail=1
fi

# UDP + DNS: nslookup queries SLIRP's DNS server (10.0.2.3:53) over UDP and
# the reply comes back to our ephemeral port. The UDP datagram arriving
# from 10.0.2.3:53 proves UDP send/receive and demux; SLIRP responds as
# long as the host has DNS (CI runners do).
if grep -qE "udp: rx [0-9]+ bytes from 10.0.2.3:53" "$SERIAL_LOG"; then
    echo "PASS: UDP round-trip with the DNS server (DNS query/reply)"
else
    echo "FAIL: no UDP reply from the DNS server" >&2
    fail=1
fi

# TCP: tcpecho opens a connection to a host echo server (reached via SLIRP
# guestfwd at 10.0.2.100:9), sends a line and verifies the echo. This needs
# the full handshake, sequence/ack tracking and the pseudo-header checksum.
if grep -q "tcp: echo ok" "$SERIAL_LOG"; then
    echo "PASS: TCP connect/send/recv echo round-trip"
else
    echo "FAIL: TCP echo did not round-trip" >&2
    fail=1
fi

# Networking capstone: one program exercises DHCP, the config read, ICMP
# ping, DNS and TCP echo, tallying the result. ALL PASS prints only if the
# whole stack worked end to end from ring 3.
if grep -qE "netcap: ALL PASS \([0-9]+ tests\)" "$SERIAL_LOG"; then
    echo "PASS: networking capstone all green"
else
    echo "FAIL: networking capstone had failures" >&2
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

# ramfs directories: dirtest makes a directory, writes a file inside it via
# a slash path, round-trips it, and confirms the dir shows up in readdir.
if grep -q "dirtest: ok" "$SERIAL_LOG"; then
    echo "PASS: ramfs directories (mkdir + nested file + readdir)"
else
    echo "FAIL: ramfs directories misbehaved" >&2
    fail=1
fi

# Environment: envtest sets GREETING then spawns envchild, which reads it
# back, proving a spawned program inherits the environment.
if grep -q "envchild: GREETING=hello-env" "$SERIAL_LOG"; then
    echo "PASS: environment variable inherited across spawn"
else
    echo "FAIL: environment not inherited" >&2
    fail=1
fi

# Syscall hardening: badptr throws kernel-space/NULL/unmapped pointers at
# every pointer-taking syscall. Each must be rejected with -1 - and crucially
# the kernel must not fault on any of them (a ring-0 fault would panic and
# kill this whole run). Reaching this line proves user pointers are validated.
if grep -q "badptr: all hostile pointers rejected" "$SERIAL_LOG"; then
    echo "PASS: syscalls reject hostile user pointers without faulting"
else
    echo "FAIL: a syscall mishandled a hostile pointer" >&2
    fail=1
fi

# Hardening capstone: one program rolls up ramfs directories, the
# environment, and hostile-pointer rejection. ALL PASS only if every
# robustness feature from this batch holds together.
if grep -qE "hardcap: ALL PASS \([0-9]+ tests\)" "$SERIAL_LOG"; then
    echo "PASS: hardening capstone all green"
else
    echo "FAIL: hardening capstone had failures" >&2
    fail=1
fi

# Anti-aliased font: aafonttest renders heading text in pure-R orange and
# counts partial-coverage edge pixels (0 < R < 255), which can only come
# from alpha blending. A healthy count proves text is genuinely antialiased.
if grep -q "aafont: antialiased ok" "$SERIAL_LOG"; then
    echo "PASS: anti-aliased font renders with blended edges"
else
    echo "FAIL: AA font missing or not antialiased" >&2
    fail=1
fi

# Avolis UI primitives: avuitest draws a gradient, a focused panel (orange
# ring) and an orange button, and checks representative pixels.
if grep -q "avui: ok" "$SERIAL_LOG"; then
    echo "PASS: Avolis UI primitives render (gradient, panel, button)"
else
    echo "FAIL: Avolis UI primitives misbehaved" >&2
    fail=1
fi

# Constellation wallpaper: avwalltest renders the AI-network background and
# confirms glowing orange node/line pixels are present.
if grep -q "avwall: ok" "$SERIAL_LOG"; then
    echo "PASS: constellation wallpaper renders (nodes + mesh)"
else
    echo "FAIL: constellation wallpaper missing" >&2
    fail=1
fi

# PS/2 mouse: CI injects movement + a click via the QEMU monitor; mousetest
# reads SYS_MOUSE and reports the cursor moving and the left button.
if grep -q "mouse: moved to" "$SERIAL_LOG" \
        && grep -q "mouse: button left" "$SERIAL_LOG"; then
    echo "PASS: PS/2 mouse reports movement and clicks"
else
    echo "FAIL: mouse did not report movement/click" >&2
    fail=1
fi

# Avolis shell, full v1 flow: unlock -> settings (change wallpaper, Esc back,
# which also exercises the new Esc keymap) -> move taskbar -> applications
# overview (launch gfxdemo) -> command palette (type "date", launch date.elf)
# -> quit. CI drives it all and checks every transition + both real launches.
if grep -q "avolis: mouse ready" "$SERIAL_LOG" \
        && grep -q "avolis: unlocked" "$SERIAL_LOG" \
        && grep -q "avolis: settings" "$SERIAL_LOG" \
        && grep -q "avolis: wallpaper ember" "$SERIAL_LOG" \
        && grep -q "avolis: taskbar left" "$SERIAL_LOG" \
        && grep -q "avolis: apps" "$SERIAL_LOG" \
        && grep -q "avolis: run gfxdemo.elf" "$SERIAL_LOG" \
        && grep -q "avolis: palette" "$SERIAL_LOG" \
        && grep -q "avolis: run date.elf" "$SERIAL_LOG" \
        && grep -q "avolis: bye" "$SERIAL_LOG"; then
    echo "PASS: Avolis v1 full flow (lock, settings, taskbar, apps, palette, mouse)"
else
    echo "FAIL: Avolis shell interactions broke" >&2
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

# Directory listing: ls.elf enumerates files via SYS_READDIR with sizes
# and a r/w flag. notes.txt is a 135-byte read-only initrd file.
if grep -qE "135 r notes.txt" "$SERIAL_LOG"; then
    echo "PASS: ls listed a file with its size via readdir"
else
    echo "FAIL: ls did not report notes.txt size" >&2
    fail=1
fi

# Shell scripts: 'ush demo.ush' runs a script that sets tag=ok and runs
# 'echo.elf script-ran-$tag'. The expanded 'script-ran-ok' is in no typed
# command and the script file is never cat'd, so it proves the script
# was read, a variable set, expanded, and a command spawned.
if grep -q "script-ran-ok" "$SERIAL_LOG"; then
    echo "PASS: ush executed a script file"
else
    echo "FAIL: ush did not run the script" >&2
    fail=1
fi

# Capstone: one program exercises the whole syscall surface (process,
# file, ramfs, pipe, time, kill) and tallies results. ALL PASS only
# prints if every sub-test succeeded.
if grep -qE "runtests: ALL PASS \([0-9]+ tests\)" "$SERIAL_LOG"; then
    echo "PASS: capstone self-test all green"
else
    echo "FAIL: capstone self-test had failures" >&2
    fail=1
fi

# Device files: /dev/null discards writes and reads EOF; /dev/zero reads
# endless zero bytes.
if grep -q "devtest: dev files ok" "$SERIAL_LOG"; then
    echo "PASS: /dev/null and /dev/zero behave"
else
    echo "FAIL: device files misbehaved" >&2
    fail=1
fi

# Framebuffer device + SYS_FBINFO: fbtest queries the geometry, then writes
# a known byte pattern to /dev/fb and reads it back through a fresh fd. The
# round-trip line only prints when both directions moved the exact bytes.
if grep -q "fbtest: dev/fb round-trip ok" "$SERIAL_LOG"; then
    echo "PASS: /dev/fb + SYS_FBINFO round-trip"
else
    echo "FAIL: /dev/fb or SYS_FBINFO misbehaved" >&2
    fail=1
fi

# ugfx library: ugfxtest draws shapes and text into a backbuffer, verifies
# the exact pixel bytes in memory (including lit and transparent glyph
# pixels), then flushes to /dev/fb and reads the first pixel back. The ok
# line prints only if every check passed.
if grep -q "ugfxtest: ok" "$SERIAL_LOG"; then
    echo "PASS: ugfx draws shapes and text and flushes to the framebuffer"
else
    echo "FAIL: ugfx library misbehaved" >&2
    fail=1
fi

# gfxdemo: a composed visual scene (gradient, title, swatches, figure,
# border). It self-checks the blit by reading its top-left border pixel
# back from /dev/fb, printing the rendered line only when that matches.
if grep -q "gfxdemo: rendered" "$SERIAL_LOG"; then
    echo "PASS: gfxdemo composed and rendered a scene"
else
    echo "FAIL: gfxdemo did not render" >&2
    fail=1
fi

# inputdemo: an interactive graphics loop. CI types 'd d s d q', so the box
# moves 4 times then quits. Require at least one logged move and a clean
# exit, proving SYS_GETKEY delivered raw keys to the graphics loop.
if grep -q "inputdemo: key " "$SERIAL_LOG" \
        && grep -qE "inputdemo: bye after [1-9]" "$SERIAL_LOG"; then
    echo "PASS: graphical input loop moved on keypresses (SYS_GETKEY)"
else
    echo "FAIL: graphical input loop did not respond to keys" >&2
    fail=1
fi

# Graphics capstone: one program exercises SYS_FBINFO, /dev/fb, every ugfx
# primitive (clear/putpixel/fillrect/clipping/text), the flush-to-device
# path and a raw byte round-trip. ALL PASS prints only if every check did.
if grep -qE "gfxcap: ALL PASS \([0-9]+ tests\)" "$SERIAL_LOG"; then
    echo "PASS: graphics capstone all green"
else
    echo "FAIL: graphics capstone had failures" >&2
    fail=1
fi

# ush readline history: 'echo.elf zqx' then Up + Enter re-runs it, so the
# bare 'zqx' output line must appear at least twice (the typed/recalled
# command lines carry the 'ush$ ' prefix, so only echo's output matches).
zqx_count=$(grep -c "^zqx$" "$SERIAL_LOG")
if [ "$zqx_count" -ge 2 ]; then
    echo "PASS: readline history recalled a command (zqx x${zqx_count})"
else
    echo "FAIL: history recall did not re-run the command (x${zqx_count})" >&2
    fail=1
fi

# SYS_SBRK: grow the heap and use it across a page boundary; a second
# sbrk hands back a higher contiguous region.
if grep -q "sbrktest: heap grows ok" "$SERIAL_LOG"; then
    echo "PASS: SYS_SBRK grows a per-process heap"
else
    echo "FAIL: SYS_SBRK misbehaved" >&2
    fail=1
fi

# App-readiness capstone: one program exercising heap (many + large
# allocs), strings, and the stdio file reader together. PLATFORM READY
# prints only if every check passed.
if grep -q "apptest: PLATFORM READY" "$SERIAL_LOG"; then
    echo "PASS: userland platform ready for real apps"
else
    echo "FAIL: app-readiness capstone had failures" >&2
    fail=1
fi

# App template: the starter app (also the "write your own app" example)
# builds and runs, exercising argv + umalloc + uprintf.
if grep -q "template: hello, world" "$SERIAL_LOG"; then
    echo "PASS: app template builds and runs"
else
    echo "FAIL: app template did not run" >&2
    fail=1
fi

# Large binary: a 64 KiB BSS (16 pages) loads zero-filled and usable,
# proving the ELF loader handles multi-page segments.
if grep -q "bigbin: 64k bss ok" "$SERIAL_LOG"; then
    echo "PASS: multi-page binary (64k BSS) loads"
else
    echo "FAIL: large-binary BSS load" >&2
    fail=1
fi

# stdio: the buffered ufile reader returns words.txt's first line "pear"
# (this exact string appears in no other output, since the file is only
# otherwise reversed by tools.ush).
if grep -q "stdiotest: first=pear" "$SERIAL_LOG"; then
    echo "PASS: ulib stdio file reader"
else
    echo "FAIL: stdio file reader" >&2
    fail=1
fi

# Multi-page user stack: an 8 KiB local buffer (would fault on a single
# 4 KiB stack page).
if grep -q "stacktest: big stack ok" "$SERIAL_LOG"; then
    echo "PASS: multi-page user stack"
else
    echo "FAIL: large stack buffer faulted" >&2
    fail=1
fi

# umalloc/ufree: alloc/write/free, freed-block reuse, and a large alloc
# that grows the heap via sbrk.
if grep -q "malloctest: heap ok" "$SERIAL_LOG"; then
    echo "PASS: user malloc/free over sbrk"
else
    echo "FAIL: user malloc/free misbehaved" >&2
    fail=1
fi

# ulib mem/string suite: ulibtest exercises memset/cpy/move/strcmp/etc.
if grep -q "ulibtest: all ok" "$SERIAL_LOG"; then
    echo "PASS: ulib mem/string suite"
else
    echo "FAIL: ulib mem/string suite" >&2
    fail=1
fi

# Coreutils filters: coretest pipes known input through each filter and
# checks the output. 'all ok' only prints if every filter passed.
if grep -q "coretest: all ok" "$SERIAL_LOG"; then
    echo "PASS: coreutils filters behave"
else
    echo "FAIL: a coreutils filter misbehaved" >&2
    fail=1
fi

# Coreutils capstone: tools.ush runs cat words.txt | sort | uniq | rev.
# 'raep' (pear reversed) appears in no input or command, so it proves the
# four filters composed correctly through a real 4-stage pipeline; the
# done marker confirms the script finished.
if grep -q "^raep$" "$SERIAL_LOG" \
        && grep -q "coreutils-capstone-done" "$SERIAL_LOG"; then
    echo "PASS: coreutils compose in a multi-stage pipeline"
else
    echo "FAIL: coreutils capstone pipeline broke" >&2
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
