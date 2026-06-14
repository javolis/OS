/* env.c - a tiny global environment table. All access is from syscall
 * context (non-preemptible), so no locking is needed. The environment is
 * global rather than per-process: a simplification that still lets a
 * spawned program read what its parent set. */
#include <stddef.h>

#include "env.h"

static struct {
    char key[ENV_KEY_MAX];
    char val[ENV_VAL_MAX];
    int used;
} vars[ENV_MAX_VARS];

static int streq(const char *a, const char *b) {
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

static int copy(char *dst, const char *src, int max) {
    int i = 0;
    while (src[i] && i < max - 1) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
    return i;
}

int env_set(const char *name, const char *val) {
    if (!name[0])
        return -1;
    int len = 0;
    while (name[len])
        len++;
    if (len >= ENV_KEY_MAX)
        return -1;

    /* Empty value clears the variable. */
    int clear = (val[0] == '\0');

    for (int i = 0; i < ENV_MAX_VARS; i++)
        if (vars[i].used && streq(vars[i].key, name)) {
            if (clear)
                vars[i].used = 0;
            else
                copy(vars[i].val, val, ENV_VAL_MAX);
            return 0;
        }
    if (clear)
        return 0; /* clearing a missing var is a no-op */
    for (int i = 0; i < ENV_MAX_VARS; i++)
        if (!vars[i].used) {
            vars[i].used = 1;
            copy(vars[i].key, name, ENV_KEY_MAX);
            copy(vars[i].val, val, ENV_VAL_MAX);
            return 0;
        }
    return -1; /* table full */
}

int env_get(const char *name, char *out, int max) {
    for (int i = 0; i < ENV_MAX_VARS; i++)
        if (vars[i].used && streq(vars[i].key, name))
            return copy(out, vars[i].val, max);
    return -1;
}
