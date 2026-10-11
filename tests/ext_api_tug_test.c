#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ext_api_tug.h"
#include "pushback_step.h"

static void
test_the_tug_state_follows_every_step(void)
{
    assert(bp_ext_tug_state(PB_STEP_OFF) == BP_EXT_TUG_NONE);
    assert(bp_ext_tug_state(PB_STEP_DRIVING_UP_CLOSE) == BP_EXT_TUG_DRIVING_IN);
    assert(bp_ext_tug_state(PB_STEP_WAITING_FOR_DOORS) == BP_EXT_TUG_AT_STAND);
    assert(bp_ext_tug_state(PB_STEP_LIFTING) == BP_EXT_TUG_CONNECTING);
    assert(bp_ext_tug_state(PB_STEP_CONNECTED) == BP_EXT_TUG_CONNECTED);
    assert(bp_ext_tug_state(PB_STEP_PUSHING) == BP_EXT_TUG_MOVING);
    assert(bp_ext_tug_state(PB_STEP_MOVING_AWAY) == BP_EXT_TUG_DISCONNECTING);
    assert(bp_ext_tug_state(PB_STEP_DRIVING_AWAY) == BP_EXT_TUG_RETURNING);
    /* Every step has a state; the states never go back during an operation. */
    int last = 0;
    for (int s = PB_STEP_TUG_LOAD; s < PB_STEP_COUNT; s++) {
        int t = bp_ext_tug_state(s);
        assert(t >= BP_EXT_TUG_DRIVING_IN && t >= last);
        last = t;
    }
    assert(strcmp(bp_ext_tug_state_name(BP_EXT_TUG_DRIVING_IN), "driving_in") == 0);
    assert(strcmp(bp_ext_tug_state_name(BP_EXT_TUG_RETURNING), "returning") == 0);
    assert(strcmp(bp_ext_tug_state_name(42), "none") == 0);
}

static void
test_the_route_time(void)
{
    /* 40 m back at 1 m/s with 0.25 m/s^2: 4 m to speed up and slow down. */
    bp_ext_leg_t back[] = { { 40, true } };
    double t = bp_ext_route_time(back, 1, 4, 1, 0.25, 4);
    assert(fabs(t - (36.0 / 1.0 + 8.0)) < 1e-9);
    /* Back 20 m, then forward 100 m: a stop and the driver's pause between. */
    bp_ext_leg_t push_pull[] = { { 20, true }, { 60, false }, { 40, false } };
    double t2 = bp_ext_route_time(push_pull, 3, 4, 1, 0.25, 4);
    double back20 = (20.0 - 4.0) / 1.0 + 8.0;
    double fwd100 = (100.0 - 64.0) / 4.0 + 32.0;
    assert(fabs(t2 - (back20 + 4 + fwd100)) < 1e-9);
    /* Too short to reach full speed. */
    bp_ext_leg_t inch[] = { { 1, false } };
    assert(fabs(bp_ext_route_time(inch, 1, 4, 1, 0.25, 4) - 2 * sqrt(4.0)) < 1e-9);
    assert(bp_ext_route_time(NULL, 0, 4, 1, 0.25, 4) == 0);
    assert(bp_ext_eta(60, 4) == 15);
    assert(fabs(bp_ext_eta(14, 0) - 10) < 1e-9);   /* never slower than walking */
    assert(bp_ext_eta(-1, 4) == 0);
}

static void
test_the_milestones_are_set_once(void)
{
    bp_ext_milestones_t m;

    bp_ext_milestones_reset(&m);
    assert(m.off_blocks == -1 && m.push_complete == -1 && m.tug_clear == -1);
    assert(!bp_ext_milestones_update(&m, PB_STEP_CONNECTED, 0, 100));
    assert(!bp_ext_milestones_update(&m, PB_STEP_PUSHING, 0.05, 110));  /* not moving yet */
    assert(bp_ext_milestones_update(&m, PB_STEP_PUSHING, -0.4, 120));
    assert(m.off_blocks == 120);
    assert(!bp_ext_milestones_update(&m, PB_STEP_PUSHING, -1.0, 130));
    assert(bp_ext_milestones_update(&m, PB_STEP_STOPPED, 0, 300));
    assert(m.push_complete == 300);
    assert(bp_ext_milestones_update(&m, PB_STEP_DRIVING_AWAY, 0, 400));
    assert(m.tug_clear == 400);
    assert(!bp_ext_milestones_update(&m, PB_STEP_OFF, 0, 500));          /* kept */
    assert(m.off_blocks == 120);
}

static void
test_a_busy_hub_makes_the_tug_wait(void)
{
    char text[96];

    assert(bp_ext_tug_wait_remaining(0, 10) == 0);
    assert(bp_ext_tug_wait_remaining(-5, 0) == 0);
    assert(bp_ext_tug_wait_remaining(360, 0) == 360);
    assert(bp_ext_tug_wait_remaining(360, 60) == 300);
    assert(bp_ext_tug_wait_remaining(360, 400) == 0);
    assert(bp_ext_tug_wait_remaining(360, -3) == 360);
    bp_ext_tug_wait_text(300, text, sizeof (text));
    assert(strcmp(text, "All tugs are busy: the tug comes in about 5 "
        "minutes") == 0);
    bp_ext_tug_wait_text(301, text, sizeof (text));
    assert(strstr(text, "about 6 minutes") != NULL);
    bp_ext_tug_wait_text(10, text, sizeof (text));
    assert(strstr(text, "about 1 minute") != NULL &&
        strstr(text, "minutes") == NULL);
}

int
main(void)
{
    test_a_busy_hub_makes_the_tug_wait();
    test_the_tug_state_follows_every_step();
    test_the_route_time();
    test_the_milestones_are_set_once();
    printf("ext_api_tug tests passed\n");
    return (0);
}
