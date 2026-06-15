/* diskapp.c - a tiny program that is NOT baked into the initrd. CI copies it
 * onto the FAT disk from the host and runs it, proving the OS can load and
 * execute an app straight from the disk (the "installed/downloaded app" path).
 */
#include "ulib.h"

void _start(void) {
    uprintf("diskapp: hello from the FAT disk\n");
    sys_exit(0);
}
