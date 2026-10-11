/*
 * External interface, version 12 (README-EXTERNAL-API.md): the tug as another
 * plugin needs to see it, and the estimates and times around an operation.
 *
 * - Where the tug is and what it is doing (a ground-handling plugin draws it
 *   among its own vehicles; an ATC plugin sees it on the apron).
 * - How long things will take before anything moves (when to call the tug so
 *   it is there in time; "tug in four minutes").
 * - The operation's milestones in zulu time (off-blocks is the airline's OUT
 *   time).
 *
 * Pure computations, no XPLM dependencies (tests/ext_api_tug_test.c).
 */

#ifndef _EXT_API_TUG_H_
#define _EXT_API_TUG_H_

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Published numbers (bp/tug_state): fixed forever, new ones at the end. */
typedef enum {
    BP_EXT_TUG_NONE = 0,          /* no tug */
    BP_EXT_TUG_AT_PARKING = 1,    /* waiting at its parking (later versions) */
    BP_EXT_TUG_DRIVING_IN = 2,    /* on its way to the aircraft */
    BP_EXT_TUG_AT_STAND = 3,      /* at the aircraft, waiting to connect */
    BP_EXT_TUG_CONNECTING = 4,    /* driving up to the nose wheel, lifting */
    BP_EXT_TUG_CONNECTED = 5,     /* connected, not moving yet */
    BP_EXT_TUG_MOVING = 6,        /* pushing or towing */
    BP_EXT_TUG_DISCONNECTING = 7, /* lowering, disconnecting, moving clear */
    BP_EXT_TUG_RETURNING = 8      /* driving away */
} bp_ext_tug_state_t;

/* The tug's state from the controller's step (pushback_step_t as an int). */
int bp_ext_tug_state(int step);
const char *bp_ext_tug_state_name(int state);   /* "driving_in" */

/* One leg of the planned route, for the time estimate. */
typedef struct {
    double len;          /* metres along the leg */
    bool backward;       /* a push leg (reverse speed) */
} bp_ext_leg_t;

/*
 * Seconds to drive the route: each leg at the forward or reverse speed, with
 * accelerating and slowing down at each change of direction and at the end,
 * and the pause the driver takes to change direction.
 */
double bp_ext_route_time(const bp_ext_leg_t *legs, int n, double fwd_spd,
    double rev_spd, double accel, double reverse_pause);

/* Seconds for the tug to cover `distance` at `speed` (at least walking pace). */
double bp_ext_eta(double distance, double speed);

/* Zulu seconds of an operation's milestones; -1 until each happens. */
typedef struct {
    double off_blocks;      /* the aircraft first moved under the tug */
    double push_complete;   /* stopped at the end of the route */
    double tug_clear;       /* the tug disconnected and drives away */
} bp_ext_milestones_t;

void bp_ext_milestones_reset(bp_ext_milestones_t *m);
/*
 * Notes the milestones reached at this step (pushback_step_t as an int), with
 * the aircraft's ground speed (m/s) and zulu time. True when one was set.
 * A step of 0 (off) after an operation keeps the times until the next one
 * starts; call reset then.
 */
bool bp_ext_milestones_update(bp_ext_milestones_t *m, int step,
    double ground_speed, double zulu);

#ifdef __cplusplus
}
#endif

#endif /* _EXT_API_TUG_H_ */
