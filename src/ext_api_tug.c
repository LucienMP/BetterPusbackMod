#include <math.h>
#include <stdio.h>

#include "ext_api_tug.h"
#include "pushback_step.h"

static const char *const tug_names[] = {
    "none", "at_parking", "driving_in", "at_stand", "connecting", "connected",
    "moving", "disconnecting", "returning"
};

int
bp_ext_tug_state(int step)
{
    switch (step) {
    case PB_STEP_TUG_LOAD:
    case PB_STEP_START:
    case PB_STEP_DRIVING_UP_CLOSE:
        return (BP_EXT_TUG_DRIVING_IN);
    case PB_STEP_WAITING_FOR_DOORS:
    case PB_STEP_OPENING_CRADLE:
    case PB_STEP_WAITING_FOR_PBRAKE:
        return (BP_EXT_TUG_AT_STAND);
    case PB_STEP_DRIVING_UP_CONNECT:
    case PB_STEP_GRABBING:
    case PB_STEP_LIFTING:
        return (BP_EXT_TUG_CONNECTING);
    case PB_STEP_CONNECTED:
    case PB_STEP_STARTING:
        return (BP_EXT_TUG_CONNECTED);
    case PB_STEP_PUSHING:
    case PB_STEP_STOPPING:
    case PB_STEP_STOPPED:
        return (BP_EXT_TUG_MOVING);
    case PB_STEP_LOWERING:
    case PB_STEP_UNGRABBING:
    case PB_STEP_WAITING4OK2DISCO:
    case PB_STEP_MOVING_AWAY:
    case PB_STEP_CLOSING_CRADLE:
    case PB_STEP_STARTING2CLEAR:
    case PB_STEP_MOVING2CLEAR:
    case PB_STEP_CLEAR_SIGNAL:
        return (BP_EXT_TUG_DISCONNECTING);
    case PB_STEP_DRIVING_AWAY:
        return (BP_EXT_TUG_RETURNING);
    default:
        return (BP_EXT_TUG_NONE);
    }
}

const char *
bp_ext_tug_state_name(int state)
{
    if (state < 0 || state > BP_EXT_TUG_RETURNING)
        return ("none");
    return (tug_names[state]);
}

/* Time to cover `len` from and to a stop at `spd` with acceleration `accel`. */
static double
run_time(double len, double spd, double accel)
{
    double ramp;

    if (len <= 0 || spd <= 0)
        return (0);
    if (accel <= 0)
        return (len / spd);
    ramp = spd * spd / accel;          /* metres to speed up and slow down */
    if (len >= ramp)
        return ((len - ramp) / spd + 2 * spd / accel);
    return (2 * sqrt(len / accel));    /* never reaches full speed */
}

double
bp_ext_route_time(const bp_ext_leg_t *legs, int n, double fwd_spd,
    double rev_spd, double accel, double reverse_pause)
{
    double total = 0, run = 0;
    bool backward = false;

    for (int i = 0; i < n; i++) {
        if (i > 0 && legs[i].backward != backward) {
            /* A change of direction: the run so far ends with a stop. */
            total += run_time(run, backward ? rev_spd : fwd_spd, accel) +
                reverse_pause;
            run = 0;
        }
        backward = legs[i].backward;
        run += legs[i].len;
    }
    total += run_time(run, backward ? rev_spd : fwd_spd, accel);
    return (total);
}

double
bp_ext_eta(double distance, double speed)
{
    const double walking = 1.4;

    if (distance <= 0)
        return (0);
    return (distance / (speed > walking ? speed : walking));
}

void
bp_ext_milestones_reset(bp_ext_milestones_t *m)
{
    m->off_blocks = m->push_complete = m->tug_clear = -1;
}

bool
bp_ext_milestones_update(bp_ext_milestones_t *m, int step,
    double ground_speed, double zulu)
{
    bool changed = false;

    if (m->off_blocks < 0 && step == PB_STEP_PUSHING &&
        fabs(ground_speed) > 0.1) {
        m->off_blocks = zulu;
        changed = true;
    }
    if (m->push_complete < 0 && m->off_blocks >= 0 &&
        step >= PB_STEP_STOPPED) {
        m->push_complete = zulu;
        changed = true;
    }
    if (m->tug_clear < 0 && step == PB_STEP_DRIVING_AWAY) {
        m->tug_clear = zulu;
        changed = true;
    }
    return (changed);
}

double
bp_ext_tug_wait_remaining(double wait, double elapsed)
{
    if (!isfinite(wait) || wait <= 0 || !isfinite(elapsed))
        return (0);
    return (elapsed >= wait ? 0 : wait - (elapsed > 0 ? elapsed : 0));
}

void
bp_ext_tug_wait_text(double remaining, char *text, size_t len)
{
    int minutes = (int)ceil(remaining / 60.0);

    if (minutes < 1)
        minutes = 1;
    (void)snprintf(text, len, "All tugs are busy: the tug comes in about %d "
        "minute%s", minutes, minutes == 1 ? "" : "s");
}
