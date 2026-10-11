#include <stddef.h>

#include "pushback_step.h"
#include "tow_session.h"

static const char *const kind_names[TOW_KIND_COUNT] = {
    "none", "return_to_stand", "reposition", "hangar", "isolated_position",
    "remote_stand", "runup_area", "recovery"
};

static const char *const state_names[] = {
    "idle", "connecting", "towing", "holding", "done", "failed"
};

static struct {
    bool active;
    int kind;
    tow_state_t ended;        /* IDLE, DONE or FAILED once a session ended */
    float speed_limit;
} session;

const char *
tow_kind_name(int kind)
{
    return (kind >= 0 && kind < TOW_KIND_COUNT ? kind_names[kind] : "none");
}

const char *
tow_state_name(int state)
{
    return (state >= TOW_STATE_IDLE && state <= TOW_STATE_FAILED ?
        state_names[state] : "idle");
}

const char *
tow_session_refused(int kind, bool started, bool slave_mode,
    bool emergency_tow_active)
{
    if (kind <= TOW_KIND_NONE || kind >= TOW_KIND_COUNT)
        return ("unknown tow kind: set bp/tow_kind to 1-7");
    if (slave_mode)
        return ("the tow is called from the other cockpit (shared cockpit)");
    if (emergency_tow_active || session.active)
        return ("a tow is already under way");
    if (started)
        return ("a push or tow is already under way");
    return (NULL);
}

void
tow_session_reset(void)
{
    float limit = session.speed_limit;

    session.active = false;
    session.kind = TOW_KIND_NONE;
    session.ended = TOW_STATE_IDLE;
    session.speed_limit = limit;   /* another plugin's setting outlives sessions */
}

bool
tow_session_begin(int kind)
{
    if (session.active || kind <= TOW_KIND_NONE || kind >= TOW_KIND_COUNT)
        return (false);
    session.active = true;
    session.kind = kind;
    session.ended = TOW_STATE_IDLE;
    return (true);
}

void
tow_session_finish(bool completed)
{
    if (!session.active)
        return;
    session.active = false;
    session.ended = completed ? TOW_STATE_DONE : TOW_STATE_FAILED;
}

bool
tow_session_is_active(void)
{
    return (session.active);
}

int
tow_session_kind(void)
{
    return (session.kind);
}

tow_state_t
tow_session_state(int step, bool holding)
{
    if (!session.active)
        return (session.ended);
    if (step < PB_STEP_PUSHING)
        return (TOW_STATE_CONNECTING);
    if (step == PB_STEP_PUSHING && holding)
        return (TOW_STATE_HOLDING);
    return (TOW_STATE_TOWING);
}

double
tow_session_speed_cap(double tug_max, double limit)
{
    if (limit <= 0 || limit >= tug_max)
        return (tug_max);
    return (limit);
}

float *
tow_session_speed_limit(void)
{
    return (&session.speed_limit);
}

bool
tow_session_allows_persistent_routes(void)
{
    return (!session.active);
}

bool
tow_session_allows_wing_walker(void)
{
    return (!session.active);
}

const char *
tow_session_recovery_refused(bool nose_gear_on_wheels, bool on_pavement)
{
    if (!nose_gear_on_wheels)
        return ("the nose gear is not on its wheels: the aircraft must be "
            "lifted before it can be towed");
    if (!on_pavement)
        return ("the aircraft is off the pavement: it must be moved back onto "
            "it before it can be towed");
    return (NULL);
}

double
tow_session_limit(int kind, double limit)
{
    if (kind == TOW_KIND_RECOVERY &&
        (limit <= 0 || limit > TOW_RECOVERY_SPEED_LIMIT))
        return (TOW_RECOVERY_SPEED_LIMIT);
    return (limit);
}

bool
tow_session_aircraft_held(int kind, bool parking_brake, bool chocked)
{
    return (parking_brake || (kind == TOW_KIND_RECOVERY && chocked));
}
