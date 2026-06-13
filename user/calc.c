/* calc.c - userland: a reverse-Polish calculator. Reads whitespace-
 * separated tokens from stdin (numbers and + - * /), evaluates with a
 * heap-allocated stack, and prints the top of stack. e.g. "3 4 + 5 *" -> 35.
 * A real little app built on umalloc + stdio. */
#include "ulib.h"

#define STACK_MAX 64

void _start(void) {
    char in[256];
    int total = 0, n;
    while (total < (int)sizeof(in) - 1 &&
           (n = sys_read(0, in + total, (int)sizeof(in) - 1 - total)) > 0)
        total += n;
    in[total] = '\0';

    int *st = umalloc(STACK_MAX * sizeof(int));
    if (!st) {
        uputs("calc: out of memory");
        sys_exit(1);
    }
    int sp = 0;

    int i = 0;
    while (in[i]) {
        while (in[i] == ' ' || in[i] == '\n' || in[i] == '\t')
            i++;
        if (!in[i])
            break;
        int start = i;
        while (in[i] && in[i] != ' ' && in[i] != '\n' && in[i] != '\t')
            i++;
        char save = in[i];
        in[i] = '\0';
        char *tok = &in[start];

        if (tok[1] == '\0' && (tok[0] == '+' || tok[0] == '-' ||
                               tok[0] == '*' || tok[0] == '/')) {
            if (sp >= 2) {
                int b = st[--sp], a = st[--sp], r = 0;
                if (tok[0] == '+')
                    r = a + b;
                else if (tok[0] == '-')
                    r = a - b;
                else if (tok[0] == '*')
                    r = a * b;
                else
                    r = b ? a / b : 0;
                st[sp++] = r;
            }
        } else if (sp < STACK_MAX) {
            st[sp++] = uatoi(tok); /* handles negative literals like -5 */
        }
        in[i] = save;
    }

    uprintf("%d\n", sp > 0 ? st[sp - 1] : 0);
    ufree(st);
    sys_exit(0);
}
