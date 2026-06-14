/* env.h - a small global environment (KEY=VALUE), shared by all tasks and
 * inherited implicitly across spawns. Set/queried via syscalls. */
#pragma once

#define ENV_MAX_VARS 16
#define ENV_KEY_MAX 32
#define ENV_VAL_MAX 96

/* Set (or update, or with an empty value clear) a variable. Returns 0 on
 * success, -1 if the name is bad or the table is full. */
int env_set(const char *name, const char *val);

/* Copy the value of `name` into out (NUL-terminated, up to max). Returns the
 * value length, or -1 if unset. */
int env_get(const char *name, char *out, int max);
