#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "ext_api_answer.h"

static void
test_every_answer_moves_the_sequence(void)
{
    bp_ext_answer_t *a;

    bp_ext_answer_reset();
    a = bp_ext_answer_store();
    assert(a->seq == 0 && a->status == BP_EXT_ANSWER_NONE);
    bp_ext_answer("BetterPushback/start", false,
        "the preferences window is open");
    assert(a->seq == 1 && a->status == BP_EXT_ANSWER_REFUSED);
    assert(strcmp(a->command, "BetterPushback/start") == 0);
    assert(strcmp(a->reason, "the preferences window is open") == 0);
    /* The same answer again still moves it: the caller sees it was asked. */
    bp_ext_answer("BetterPushback/start", false,
        "the preferences window is open");
    assert(a->seq == 2);
    /* Accepted: no reason, whatever was passed. */
    bp_ext_answer("BetterPushback/stop", true, "ignored");
    assert(a->status == BP_EXT_ANSWER_ACCEPTED && a->reason[0] == '\0');
    bp_ext_answer(NULL, false, NULL);
    assert(a->command[0] == '\0' && a->reason[0] == '\0');
}

static void
test_long_words_are_cut(void)
{
    char long_text[400];
    bp_ext_answer_t *a;

    bp_ext_answer_reset();
    a = bp_ext_answer_store();
    memset(long_text, 'x', sizeof (long_text) - 1);
    long_text[sizeof (long_text) - 1] = '\0';
    bp_ext_answer(long_text, false, long_text);
    assert(strlen(a->command) == sizeof (a->command) - 1);
    assert(strlen(a->reason) == sizeof (a->reason) - 1);
}

static void
test_only_the_outermost_command_answers(void)
{
    bp_ext_answer_t *a;

    bp_ext_answer_reset();
    a = bp_ext_answer_store();
    /* start runs connect_first inside it: one answer, start's. */
    assert(bp_ext_answer_enter());
    assert(!bp_ext_answer_enter());
    bp_ext_answer_outer("BetterPushback/connect_first", true, "");
    bp_ext_answer_leave();
    assert(a->seq == 0);
    bp_ext_answer_outer("BetterPushback/start", true, "");
    bp_ext_answer_leave();
    assert(a->seq == 1 && strcmp(a->command, "BetterPushback/start") == 0);
    /* Leaving more than entered does no harm. */
    bp_ext_answer_leave();
    assert(bp_ext_answer_enter());
    bp_ext_answer_leave();
}

int
main(void)
{
    test_every_answer_moves_the_sequence();
    test_long_words_are_cut();
    test_only_the_outermost_command_answers();
    printf("ext_api_answer tests passed\n");
    return (0);
}
