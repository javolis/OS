/* dirtest.c - userland: exercise ramfs directories. Make a directory, put a
 * file inside it (a slash-separated path), round-trip the contents, and
 * confirm the directory shows up via readdir as kind 2. */
#include "ulib.h"

void _start(void) {
    if (sys_mkdir("docs") != 0) {
        uprintf("dirtest: FAIL mkdir\n");
        sys_exit(1);
    }

    /* Write a file inside the directory. */
    int fd = sys_create("docs/note.txt");
    if (fd < 0) {
        uprintf("dirtest: FAIL create in dir\n");
        sys_exit(1);
    }
    const char *msg = "in a subdir";
    sys_writefd(fd, msg, 11);
    sys_close(fd);

    /* Read it back. */
    fd = sys_open("docs/note.txt");
    char buf[32];
    int n = (fd >= 0) ? sys_read(fd, buf, sizeof(buf)) : -1;
    if (fd >= 0)
        sys_close(fd);
    if (n != 11 || ustrncmp(buf, msg, 11) != 0) {
        uprintf("dirtest: FAIL round-trip n=%d\n", n);
        sys_exit(1);
    }

    /* The directory must appear in readdir as kind 2. */
    struct dirent e;
    int saw_dir = 0;
    for (int i = 0; sys_readdir(i, &e) == 0; i++)
        if (e.kind == 2 && ustreq(e.name, "docs"))
            saw_dir = 1;
    if (!saw_dir) {
        uprintf("dirtest: FAIL dir not listed\n");
        sys_exit(1);
    }

    uprintf("dirtest: ok\n");
    sys_exit(0);
}
