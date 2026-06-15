/* syscall.h — int 0x80 system call interface.
 * Convention: eax = syscall number (and return value), ebx = first arg. */
#pragma once

#include "idt.h"

#define SYSCALL_VECTOR 0x80

#define SYS_EXIT 0     /* ebx = exit code */
#define SYS_WRITE 1    /* ebx = NUL-terminated string (user address) */
#define SYS_SLEEP 2    /* ebx = milliseconds; blocks the task */
#define SYS_GETPID 3   /* returns the caller's pid in eax */
#define SYS_READLINE 4 /* ebx = buffer, ecx = size; foreground task only;
                        * blocks for a line of keyboard input, returns its
                        * length (or -1) */
#define SYS_SPAWN 5    /* ebx = cmdline string (file name + args); ecx = 1
                        * to pass the foreground on (caller must own it);
                        * returns child pid or -1 */
#define SYS_WAIT 6     /* ebx = pid; blocks until it exits, returns its
                        * exit status (and reclaims it) */
#define SYS_SYSINFO 7  /* ebx = struct sysinfo* (user, writable) */
#define SYS_OPEN 8     /* ebx = initrd file name; returns fd or -1 */
#define SYS_READ 9     /* ebx = fd, ecx = buf, edx = n; returns bytes read,
                        * 0 at EOF (console: one edited line, fg only) */
#define SYS_WRITEFD 10  /* ebx = fd, ecx = buf, edx = n; returns n or -1 */
#define SYS_CLOSE 11    /* ebx = fd */
#define SYS_PIPE 12     /* ebx = int[2]; fills {read fd, write fd} */
#define SYS_SPAWN_IO 13 /* ebx = cmdline, ecx = stdin fd, edx = stdout fd
                         * (-1 = console); background; returns pid or -1 */
#define SYS_TIME 14     /* ebx = struct systime* (user, writable) */
#define SYS_CREATE 15   /* ebx = name; create/truncate a ramfs file for
                         * writing; returns fd or -1 */
#define SYS_APPEND 16   /* ebx = name; open (or create) a ramfs file with
                         * the write offset at end-of-file; returns fd/-1 */
#define SYS_UNLINK 17   /* ebx = name; remove a ramfs file; 0 or -1 */
#define SYS_KILL 18     /* ebx = pid; terminate a task; 0 or -1. No
                         * permission model yet: any task may kill any. */
#define SYS_READDIR 19  /* ebx = index, ecx = struct dirent* (writable);
                         * enumerates initrd then ramfs; 0 or -1 at end */
#define SYS_SBRK 20     /* ebx = signed increment; grows the per-process
                         * heap and returns the old break, or -1 */
#define SYS_FBINFO 21   /* ebx = struct fbinfo* (user, writable); fills the
                         * framebuffer geometry. 0 if a framebuffer exists,
                         * -1 on a VGA-text-only boot. Pair with open("/dev/fb")
                         * and writefd to blit raw pixels. */
#define SYS_GETKEY 22   /* one raw keyboard char (no echo or line editing);
                         * foreground task only. Blocks until a key arrives.
                         * Returns 0..255, or -1 if not foreground. For
                         * interactive graphics (KEY_UP/DOWN come through as
                         * 0x80/0x81). */
#define SYS_PING 23     /* ebx = IPv4 address in host byte order; sends an
                         * ICMP echo and waits. Returns the round-trip time
                         * in milliseconds, or -1 on timeout. */
#define SYS_RESOLVE 24  /* ebx = hostname string, ecx = uint32* (writable);
                         * DNS-resolves the name and writes the IPv4 address
                         * in host byte order. Returns 0, or -1 on failure. */
#define SYS_DHCP 25     /* run the DHCP handshake and apply the lease.
                         * Returns the leased IPv4 address (host order), or
                         * 0 on failure. */
#define SYS_TCP_CONNECT 26 /* ebx = IPv4 (host order), ecx = port; opens a
                            * TCP connection. 0 on success, -1 otherwise. */
#define SYS_TCP_SEND 27 /* ebx = buf, ecx = len; sends on the connection.
                         * Returns bytes queued, or -1. */
#define SYS_TCP_RECV 28 /* ebx = buf, ecx = max; receives. Returns the byte
                         * count, 0 if the peer closed, or -1. */
#define SYS_TCP_CLOSE 29 /* close the connection. */
#define SYS_NETINFO 30  /* ebx = struct netinfo* (writable); fills the IPv4
                         * configuration in host byte order. 0 if networking
                         * is up, -1 if there is no NIC. */
#define SYS_MKDIR 31    /* ebx = path; create a ramfs directory marker.
                         * Returns 0, or -1 if it exists or the table is
                         * full. */
#define SYS_SETENV 32   /* ebx = name, ecx = value; set a global env var
                         * (empty value clears it). 0 or -1. */
#define SYS_GETENV 33   /* ebx = name, ecx = buf, edx = max; copies the
                         * value. Returns its length, or -1 if unset. */
#define SYS_TRYGETKEY 34 /* like getkey but non-blocking: returns the next
                          * raw key 0..255, or -1 if none is waiting (or not
                          * foreground). For UI event loops that also tick. */
#define SYS_MOUSE 35    /* ebx = struct mousestate* (writable); fills the
                         * cursor x/y and button bitmask. 0, or -1 if there
                         * is no mouse. */
#define SYS_BEEP 36     /* ebx = frequency Hz (0 = silence now), ecx = duration
                         * ms; plays a square-wave tone on the PC speaker then
                         * silences it. Returns 0. */
#define SYS_AUDIO 37    /* ebx = int16 PCM samples (user), ecx = sample count
                         * (interleaved L,R, 48 kHz, 16-bit stereo). Plays them
                         * via AC'97 DMA and blocks until drained. Returns the
                         * number of samples played, or -1 if no codec. */
#define SYS_AUDIO_REC 38 /* ebx = int16 buffer (user, writable), ecx = sample
                          * count. Captures that many samples via AC'97 DMA,
                          * blocks until filled, copies them out. Returns the
                          * number captured, or -1 if no codec. */
#define SYS_PS 39 /* ebx = index, ecx = struct procinfo* (writable); fills the
                   * index-th listed task. Returns 0, or -1 past the end. */
#define SYS_AUDIO_VOL 40 /* ebx = level 0-100; sets the AC'97 master volume.
                          * Returns the clamped level, or -1 if no codec. */
#define SYS_POWEROFF 41 /* power the machine off (does not return). */
#define SYS_REBOOT 42   /* restart the machine (does not return). */
#define SYS_MOUSE_SPEED 43 /* ebx = pointer speed percent (25-400); scales the
                            * mouse deltas. Returns the clamped value. */

/* Keep in sync with the userland copy in user/usys.h. */
struct procinfo {
    uint32_t pid;
    uint32_t state; /* 0 free 1 ready 2 blocked 3 waitkbd 4 waitpid 5 waitchan
                     * 6 zombie 7 running */
    char name[16];
};

/* Keep in sync with the userland copy in user/usys.h. */
struct dirent {
    char name[32];
    uint32_t size;
    uint32_t kind; /* 0 = initrd (ro), 1 = ramfs file, 2 = ramfs directory */
};

/* Keep in sync with the userland copy in user/usys.h. */
struct systime {
    uint16_t year;
    uint8_t month, day, hour, minute, second;
};

/* Keep in sync with the userland copy in user/usys.h. */
struct mousestate {
    int32_t x;
    int32_t y;
    uint32_t buttons; /* MOUSE_LEFT | MOUSE_RIGHT | MOUSE_MIDDLE */
};

/* Keep in sync with the userland copy in user/usys.h. */
struct netinfo {
    uint32_t ip;      /* our IPv4 address (host byte order) */
    uint32_t gateway;
    uint32_t netmask;
    uint32_t dns;
};

/* Keep in sync with the userland copy in user/usys.h. */
struct fbinfo {
    uint32_t width;  /* pixels */
    uint32_t height; /* pixels */
    uint32_t pitch;  /* bytes per scanline */
    uint32_t bpp;    /* bits per pixel: 24 or 32 */
};

/* Keep in sync with the userland copy in user/usys.h. */
struct sysinfo {
    uint32_t ticks;        /* PIT ticks since boot (100 Hz) */
    uint32_t free_frames;  /* physical memory */
    uint32_t total_frames;
    uint32_t tasks_alive;  /* live user tasks, caller included */
};

void syscall_handle(struct registers *regs);
