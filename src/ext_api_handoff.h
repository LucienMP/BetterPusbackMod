/*
 * External interface, version 13 (README-EXTERNAL-API.md): parts of the
 * operation another plugin may take over, so the two never do the same job.
 *
 * - An external blocker: a ground-handling plugin knows of things that must
 *   finish before the tug connects or the push starts (a belt loader still at
 *   the aft hold, the catering truck lowering). While it is set the tug does
 *   neither, and the blocker is published with that plugin's words.
 * - The chocks: removing them becomes the other plugin's crew step after the
 *   tug connects; a chocked aircraft then blocks the push start.
 * - The wing walker: the other plugin draws its own walkers, so
 *   BetterPushback draws none and says when one is needed.
 *
 * The storage the writable datarefs point at, and the decisions; no XPLM
 * dependencies (tests/ext_api_handoff_test.c).
 */

#ifndef _EXT_API_HANDOFF_H_
#define _EXT_API_HANDOFF_H_

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BP_EXT_HANDOFF_TEXT_LEN 96
#define BP_EXT_HANDOFF_OWNER_LEN 32

typedef struct {
    int external_block;                    /* bp/external_block */
    char text[BP_EXT_HANDOFF_TEXT_LEN];    /* bp/external_block_text */
    char owner[BP_EXT_HANDOFF_OWNER_LEN];  /* bp/external_block_owner */
    int chocks_external;                   /* bp/chocks_external */
    int walkers_external;                  /* bp/walkers_external */
} bp_ext_handoff_t;

bp_ext_handoff_t *bp_ext_handoff(void);
void bp_ext_handoff_reset(void);

/* The tug may not connect: the other plugin's blocker is set. */
bool bp_ext_handoff_blocks_connect(void);
/* What blocks the push from starting: 0 nothing, 5 external, 6 chocks. */
int bp_ext_handoff_start_blocker(bool chocked);
/* BetterPushback removes the chocks itself as the push starts. */
bool bp_ext_handoff_removes_chocks(void);
/* BetterPushback draws its wing walker (when it otherwise would). */
bool bp_ext_handoff_draws_walker(bool would);
/* The status line while the other plugin blocks ("Waiting: belt loader at
 * the aft hold"); its text, or a plain one. */
const char *bp_ext_handoff_status(void);

#ifdef __cplusplus
}
#endif

#endif /* _EXT_API_HANDOFF_H_ */
