/* spawnstorm.c - userland stress test: hammer spawn/wait and pipes in a
 * loop, then verify the kernel reclaimed every frame. A self-checking
 * leak detector for the process/fd/pipe machinery.
 *
 * It compares the free-frame count from before any spawning to the count
 * after every child has been waited (and thus reaped). Because this is the
 * foreground task and it waits on each child, all of them are fully torn
 * down by the time we re-read sysinfo, so the counts must match exactly. */
#include "ulib.h"

#define ROUNDS 12

/* One round = a plain spawn/wait plus a piped pair, each fully waited
 * (and thus reaped) before returning. */
static void round(void) {
    int pid = sys_spawn("exitcode.elf");
    if (pid >= 0)
        sys_wait(pid);

    int p[2];
    if (sys_pipe(p) == 0) {
        int a = sys_spawn_io("cat.elf notes.txt", -1, p[1]);
        int b = sys_spawn_io("upper.elf", p[0], -1);
        sys_close(p[0]);
        sys_close(p[1]);
        if (a >= 0)
            sys_wait(a);
        if (b >= 0)
            sys_wait(b);
    }
}

void _start(void) {
    struct sysinfo before, after;

    /* Warm up first: the kernel heap only grows (freed blocks are reused
     * but frames never return to the PMM), so the first allocation of a
     * new size — e.g. the pipe struct — grows it once. Doing a full round
     * before the baseline read keeps that one-time growth out of the
     * measurement. */
    round();
    sys_sysinfo(&before);

    for (int i = 0; i < ROUNDS; i++)
        round();

    sys_sysinfo(&after);
    uprintf("spawnstorm: %d rounds done; frames before=%u after=%u\n", ROUNDS,
            before.free_frames, after.free_frames);
    if (before.free_frames == after.free_frames)
        uprintf("spawnstorm: no leak\n");
    else
        uprintf("spawnstorm: LEAK of %d frames\n",
                (int)before.free_frames - (int)after.free_frames);
    sys_exit(0);
}
