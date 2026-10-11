#include <stdio.h>
#include <string.h>

#include "ext_api_handoff.h"

/* Published blocker numbers (ext_api_state.h): external and chocks. */
#define BLOCKER_EXTERNAL 5
#define BLOCKER_CHOCKS 6

static bp_ext_handoff_t handoff;
static char status[BP_EXT_HANDOFF_TEXT_LEN + 24];

bp_ext_handoff_t *
bp_ext_handoff(void)
{
    return (&handoff);
}

void
bp_ext_handoff_reset(void)
{
    memset(&handoff, 0, sizeof (handoff));
}

bool
bp_ext_handoff_blocks_connect(void)
{
    return (handoff.external_block != 0);
}

int
bp_ext_handoff_start_blocker(bool chocked)
{
    if (handoff.external_block != 0)
        return (BLOCKER_EXTERNAL);
    if (handoff.chocks_external != 0 && chocked)
        return (BLOCKER_CHOCKS);
    return (0);
}

bool
bp_ext_handoff_removes_chocks(void)
{
    return (handoff.chocks_external == 0);
}

bool
bp_ext_handoff_draws_walker(bool would)
{
    return (would && handoff.walkers_external == 0);
}

bool
bp_ext_handoff_holds_connected(void)
{
    return (handoff.hold_connected != 0);
}

const char *
bp_ext_handoff_hold_status(void)
{
    /* The text is written by another plugin: never trusted to be terminated. */
    handoff.hold_text[sizeof (handoff.hold_text) - 1] = '\0';
    if (handoff.hold_text[0] != '\0')
        (void)snprintf(status, sizeof (status), "Holding connected: %s",
            handoff.hold_text);
    else
        (void)snprintf(status, sizeof (status),
            "Holding connected for the ground crew");
    return (status);
}

const char *
bp_ext_handoff_status(void)
{
    /* The text is written by another plugin: never trusted to be terminated. */
    handoff.text[sizeof (handoff.text) - 1] = '\0';
    if (handoff.text[0] != '\0')
        (void)snprintf(status, sizeof (status), "Waiting: %s", handoff.text);
    else
        (void)snprintf(status, sizeof (status), "Waiting for the ground crew");
    return (status);
}
