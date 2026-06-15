/* persist.c - prove FAT-disk persistence across reboots.
 *   persist.elf w   write a marker file to the disk
 *   persist.elf r   read it back and print it
 * CI writes in one boot and reads in a second boot of the same disk image. */
#include "ulib.h"

#define MARK "AVOLIS_PERSIST_OK"

void _start(int argc, char **argv) {
    if (argc >= 2 && argv[1][0] == 'w') {
        int n = sys_disk_write("PERSIST.TXT", MARK, (int)ustrlen(MARK));
        uprintf(n == (int)ustrlen(MARK) ? "persist: wrote marker\n"
                                        : "persist: write failed\n");
    } else {
        char buf[64];
        int n = sys_disk_read("PERSIST.TXT", buf, 63);
        if (n > 0) {
            buf[n] = '\0';
            uprintf("persist: read %s\n", buf);
        } else {
            uprintf("persist: no marker\n");
        }
    }
    sys_exit(0);
}
