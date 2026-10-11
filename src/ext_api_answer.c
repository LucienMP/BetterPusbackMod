/*
 * External interface, version 19: every command answers. Unit tested on its
 * own (tests/ext_api_answer_test.c).
 */

#include <stdio.h>
#include <string.h>

#include "ext_api_answer.h"

static bp_ext_answer_t store;
static int depth = 0;

bp_ext_answer_t *
bp_ext_answer_store(void)
{
    return (&store);
}

void
bp_ext_answer_reset(void)
{
    memset(&store, 0, sizeof (store));
    depth = 0;
}

void
bp_ext_answer(const char *command, bool accepted, const char *reason)
{
    store.seq++;
    store.status = accepted ? BP_EXT_ANSWER_ACCEPTED : BP_EXT_ANSWER_REFUSED;
    (void)snprintf(store.command, sizeof (store.command), "%s",
        command != NULL ? command : "");
    (void)snprintf(store.reason, sizeof (store.reason), "%s",
        (!accepted && reason != NULL) ? reason : "");
}

bool
bp_ext_answer_enter(void)
{
    return (++depth == 1);
}

void
bp_ext_answer_leave(void)
{
    if (depth > 0)
        depth--;
}

void
bp_ext_answer_outer(const char *command, bool accepted, const char *reason)
{
    if (depth <= 1)
        bp_ext_answer(command, accepted, reason);
}
