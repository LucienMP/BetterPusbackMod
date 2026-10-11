/*
 * External interface, version 19 (README-EXTERNAL-API.md): every command
 * answers at once, accepted or refused with the reason, in one place. The
 * route commands also keep answering through bp/route_seq, route_status and
 * route_reason (version 5).
 *
 * The storage the datarefs point at; no XPLM dependencies
 * (tests/ext_api_answer_test.c).
 */

#ifndef _EXT_API_ANSWER_H_
#define _EXT_API_ANSWER_H_

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BP_EXT_ANSWER_COMMAND_LEN 64
#define BP_EXT_ANSWER_REASON_LEN 128

/* Published numbers (bp/answer_status). */
enum {
    BP_EXT_ANSWER_NONE = 0,
    BP_EXT_ANSWER_ACCEPTED = 1,
    BP_EXT_ANSWER_REFUSED = 2
};

typedef struct {
    int seq;                                       /* bp/answer_seq */
    int status;                                    /* bp/answer_status */
    char command[BP_EXT_ANSWER_COMMAND_LEN];       /* bp/answer_command */
    char reason[BP_EXT_ANSWER_REASON_LEN];         /* bp/answer_reason */
} bp_ext_answer_t;

bp_ext_answer_t *bp_ext_answer_store(void);
void bp_ext_answer_reset(void);

/* Records the answer to `command`: seq moves by one, every time. */
void bp_ext_answer(const char *command, bool accepted, const char *reason);

/*
 * Commands may call others (start calls connect_first in some setups): only
 * the outermost one answers. Enter before running a command, leave after;
 * enter returns true when this one is the outermost.
 */
bool bp_ext_answer_enter(void);
void bp_ext_answer_leave(void);
/* Answers only when the current command is the outermost one. */
void bp_ext_answer_outer(const char *command, bool accepted,
    const char *reason);

#ifdef __cplusplus
}
#endif

#endif /* _EXT_API_ANSWER_H_ */
