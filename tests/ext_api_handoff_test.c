#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "ext_api_handoff.h"

static void
test_nothing_changes_by_default(void)
{
    bp_ext_handoff_reset();
    assert(!bp_ext_handoff_blocks_connect());
    assert(bp_ext_handoff_start_blocker(true) == 0);   /* chocks: BP removes them */
    assert(bp_ext_handoff_removes_chocks());
    assert(bp_ext_handoff_draws_walker(true));
    assert(!bp_ext_handoff_draws_walker(false));
}

static void
test_an_external_blocker_holds_the_connection_and_the_start(void)
{
    bp_ext_handoff_t *h;

    bp_ext_handoff_reset();
    h = bp_ext_handoff();
    h->external_block = 1;
    (void)snprintf(h->text, sizeof (h->text), "belt loader at the aft hold");
    assert(bp_ext_handoff_blocks_connect());
    assert(bp_ext_handoff_start_blocker(false) == 5);
    assert(strcmp(bp_ext_handoff_status(), "Waiting: belt loader at the aft hold") == 0);
    h->text[0] = '\0';
    assert(strcmp(bp_ext_handoff_status(), "Waiting for the ground crew") == 0);
    /* A text not terminated by its writer is cut, never read past. */
    memset(h->text, 'x', sizeof (h->text));
    assert(strlen(bp_ext_handoff_status()) < sizeof (h->text) + 16);
    h->external_block = 0;
    assert(!bp_ext_handoff_blocks_connect());
}

static void
test_chocks_and_walkers_belong_to_the_other_plugin(void)
{
    bp_ext_handoff_t *h;

    bp_ext_handoff_reset();
    h = bp_ext_handoff();
    h->chocks_external = 1;
    assert(!bp_ext_handoff_removes_chocks());
    assert(bp_ext_handoff_start_blocker(true) == 6);
    assert(bp_ext_handoff_start_blocker(false) == 0);
    h->external_block = 1;
    assert(bp_ext_handoff_start_blocker(true) == 5);   /* the plugin's own blocker first */
    h->walkers_external = 1;
    assert(!bp_ext_handoff_draws_walker(true));
}

int
main(void)
{
    test_nothing_changes_by_default();
    test_an_external_blocker_holds_the_connection_and_the_start();
    test_chocks_and_walkers_belong_to_the_other_plugin();
    printf("ext_api_handoff tests passed\n");
    return (0);
}
