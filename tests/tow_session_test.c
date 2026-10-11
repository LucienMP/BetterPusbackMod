#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "pushback_step.h"
#include "tow_session.h"

static void
test_kinds_and_states_have_fixed_names(void)
{
    assert(strcmp(tow_kind_name(TOW_KIND_RETURN_TO_STAND), "return_to_stand") == 0);
    assert(strcmp(tow_kind_name(TOW_KIND_REPOSITION), "reposition") == 0);
    assert(strcmp(tow_kind_name(TOW_KIND_HANGAR), "hangar") == 0);
    assert(strcmp(tow_kind_name(TOW_KIND_ISOLATED_POSITION), "isolated_position") == 0);
    assert(strcmp(tow_kind_name(TOW_KIND_REMOTE_STAND), "remote_stand") == 0);
    assert(strcmp(tow_kind_name(TOW_KIND_RUNUP_AREA), "runup_area") == 0);
    assert(strcmp(tow_kind_name(TOW_KIND_RECOVERY), "recovery") == 0);
    assert(strcmp(tow_kind_name(99), "none") == 0);
    assert(TOW_STATE_IDLE == 0 && TOW_STATE_CONNECTING == 1 && TOW_STATE_TOWING == 2 &&
        TOW_STATE_HOLDING == 3 && TOW_STATE_DONE == 4 && TOW_STATE_FAILED == 5);
    assert(strcmp(tow_state_name(TOW_STATE_HOLDING), "holding") == 0);
    assert(strcmp(tow_state_name(-1), "idle") == 0);
}

static void
test_a_tow_is_refused_with_a_reason(void)
{
    tow_session_reset();
    assert(tow_session_refused(TOW_KIND_REPOSITION, false, false, false) == NULL);
    assert(strstr(tow_session_refused(0, false, false, false), "bp/tow_kind"));
    assert(strstr(tow_session_refused(8, false, false, false), "bp/tow_kind"));
    assert(strstr(tow_session_refused(2, false, true, false), "shared cockpit"));
    assert(strstr(tow_session_refused(2, false, false, true), "already"));
    assert(strstr(tow_session_refused(2, true, false, false), "already under way"));
    assert(tow_session_begin(TOW_KIND_HANGAR));
    assert(strstr(tow_session_refused(2, false, false, false), "already"));
    assert(!tow_session_begin(TOW_KIND_HANGAR));   /* one at a time */
    tow_session_finish(true);
}

static void
test_the_state_follows_the_controller(void)
{
    tow_session_reset();
    assert(tow_session_state(PB_STEP_OFF, false) == TOW_STATE_IDLE);
    assert(tow_session_begin(TOW_KIND_REPOSITION));
    assert(tow_session_is_active() && tow_session_kind() == TOW_KIND_REPOSITION);
    assert(tow_session_state(PB_STEP_TUG_LOAD, false) == TOW_STATE_CONNECTING);
    assert(tow_session_state(PB_STEP_CONNECTED, false) == TOW_STATE_CONNECTING);
    assert(tow_session_state(PB_STEP_PUSHING, false) == TOW_STATE_TOWING);
    assert(tow_session_state(PB_STEP_PUSHING, true) == TOW_STATE_HOLDING);
    assert(tow_session_state(PB_STEP_MOVING_AWAY, false) == TOW_STATE_TOWING);
    tow_session_finish(true);
    assert(!tow_session_is_active());
    assert(tow_session_state(PB_STEP_OFF, false) == TOW_STATE_DONE);
    /* Ended early: failed, until the next tow. */
    assert(tow_session_begin(TOW_KIND_RECOVERY));
    tow_session_finish(false);
    assert(tow_session_state(PB_STEP_OFF, false) == TOW_STATE_FAILED);
    assert(tow_session_begin(TOW_KIND_REPOSITION));
    assert(tow_session_state(PB_STEP_START, false) == TOW_STATE_CONNECTING);
    tow_session_reset();
    assert(tow_session_state(PB_STEP_OFF, false) == TOW_STATE_IDLE);
}

static void
test_the_speed_limit_caps_the_tug(void)
{
    assert(tow_session_speed_cap(4.0, 0.0) == 4.0);     /* no limit */
    assert(tow_session_speed_cap(4.0, -1.0) == 4.0);
    assert(tow_session_speed_cap(4.0, 2.0) == 2.0);
    assert(tow_session_speed_cap(4.0, 6.0) == 4.0);     /* never faster than the tug */
    *tow_session_speed_limit() = 2.0f;
    tow_session_reset();
    assert(*tow_session_speed_limit() == 2.0f);         /* the setting outlives sessions */
    *tow_session_speed_limit() = 0.0f;
}

static void
test_tows_are_not_saved_and_have_no_wing_walker(void)
{
    tow_session_reset();
    assert(tow_session_allows_persistent_routes() && tow_session_allows_wing_walker());
    assert(tow_session_begin(TOW_KIND_HANGAR));
    assert(!tow_session_allows_persistent_routes() && !tow_session_allows_wing_walker());
    tow_session_finish(true);
    assert(tow_session_allows_persistent_routes());
}

static void
test_recovery_rules(void)
{
    assert(tow_session_recovery_refused(true, true) == NULL);
    assert(strstr(tow_session_recovery_refused(false, true), "nose gear"));
    assert(strstr(tow_session_recovery_refused(true, false), "pavement"));
    /* Slow: recovery's own cap, or a lower one another plugin set. */
    assert(tow_session_limit(TOW_KIND_RECOVERY, 0.0) == TOW_RECOVERY_SPEED_LIMIT);
    assert(tow_session_limit(TOW_KIND_RECOVERY, 5.0) == TOW_RECOVERY_SPEED_LIMIT);
    assert(tow_session_limit(TOW_KIND_RECOVERY, 0.5) == 0.5);
    assert(tow_session_limit(TOW_KIND_HANGAR, 0.0) == 0.0);
    assert(tow_session_limit(TOW_KIND_HANGAR, 3.0) == 3.0);
    /* Chocks hold the aircraft for the tug only on a recovery. */
    assert(tow_session_aircraft_held(TOW_KIND_RECOVERY, false, true));
    assert(!tow_session_aircraft_held(TOW_KIND_HANGAR, false, true));
    assert(tow_session_aircraft_held(TOW_KIND_HANGAR, true, false));
    assert(!tow_session_aircraft_held(TOW_KIND_RECOVERY, false, false));
}

int
main(void)
{
    test_kinds_and_states_have_fixed_names();
    test_a_tow_is_refused_with_a_reason();
    test_the_state_follows_the_controller();
    test_the_speed_limit_caps_the_tug();
    test_tows_are_not_saved_and_have_no_wing_walker();
    test_recovery_rules();
    printf("tow session tests passed\n");
    return (0);
}
