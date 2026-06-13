/* kv.c - userland: an in-memory key/value store built on the heap. Reads
 * commands from stdin: "set <key> <value>", "get <key>", "del <key>".
 * 'get' prints the value (or "(nil)"); set/del are silent. A real little
 * app exercising umalloc/ufree with a linked list of dup'd strings. */
#include "ulib.h"

struct kv {
    char *key;
    char *val;
    struct kv *next;
};

static struct kv *head;

static char *dup(const char *s) {
    uint32_t n = ustrlen(s) + 1;
    char *p = umalloc(n);
    if (p)
        umemcpy(p, s, n);
    return p;
}

static void kv_set(const char *k, const char *v) {
    for (struct kv *e = head; e; e = e->next) {
        if (ustreq(e->key, k)) {
            ufree(e->val);
            e->val = dup(v);
            return;
        }
    }
    struct kv *e = umalloc(sizeof(struct kv));
    if (!e)
        return;
    e->key = dup(k);
    e->val = dup(v);
    e->next = head;
    head = e;
}

static const char *kv_get(const char *k) {
    for (struct kv *e = head; e; e = e->next)
        if (ustreq(e->key, k))
            return e->val;
    return 0;
}

static void kv_del(const char *k) {
    struct kv **pp = &head;
    while (*pp) {
        if (ustreq((*pp)->key, k)) {
            struct kv *dead = *pp;
            *pp = dead->next;
            ufree(dead->key);
            ufree(dead->val);
            ufree(dead);
            return;
        }
        pp = &(*pp)->next;
    }
}

/* Split " a b c..." into cmd, key, and value (value = rest of line). */
static void run_line(char *line) {
    while (*line == ' ')
        line++;
    if (*line == '\0')
        return;

    char *cmd = line;
    while (*line && *line != ' ')
        line++;
    if (*line)
        *line++ = '\0';
    while (*line == ' ')
        line++;

    char *key = line;
    while (*line && *line != ' ')
        line++;
    if (*line)
        *line++ = '\0';
    while (*line == ' ')
        line++;
    char *val = line; /* remainder, may contain spaces */

    if (ustreq(cmd, "set") && *key)
        kv_set(key, val);
    else if (ustreq(cmd, "get") && *key) {
        const char *v = kv_get(key);
        uputs(v ? v : "(nil)");
    } else if (ustreq(cmd, "del") && *key)
        kv_del(key);
}

void _start(void) {
    char in[512];
    int total = 0, n;
    while (total < (int)sizeof(in) - 1 &&
           (n = sys_read(0, in + total, (int)sizeof(in) - 1 - total)) > 0)
        total += n;
    in[total] = '\0';

    int i = 0;
    while (i < total) {
        int s = i;
        while (i < total && in[i] != '\n')
            i++;
        in[i] = '\0';
        run_line(&in[s]);
        i++;
    }
    sys_exit(0);
}
