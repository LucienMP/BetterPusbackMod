/*
 * Tow sessions (README-EXTERNAL-API.md, version 9): another plugin calls the
 * tug to tow the aircraft along a route it supplies, at any time the aircraft
 * is stopped on the ground, not only after a push and not only at a stand.
 * A tow has a kind (reposition, hangar, isolated position, ...), may have its
 * speed capped, and is never saved as a stand's route.
 *
 * This module only keeps the session's facts and rules; the controller in
 * bp.c drives the tow as it drives any route. No XPLM dependencies, so it is
 * unit tested on its own (tests/tow_session_test.c).
 */

#ifndef _TOW_SESSION_H_
#define _TOW_SESSION_H_

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Published numbers (bp/tow_kind): fixed forever, new ones at the end. */
typedef enum {
    TOW_KIND_NONE = 0,
    TOW_KIND_RETURN_TO_STAND = 1,
    TOW_KIND_REPOSITION = 2,
    TOW_KIND_HANGAR = 3,
    TOW_KIND_ISOLATED_POSITION = 4,
    TOW_KIND_REMOTE_STAND = 5,
    TOW_KIND_RUNUP_AREA = 6,
    TOW_KIND_RECOVERY = 7
} tow_kind_t;
#define TOW_KIND_COUNT 8

/* Published numbers (bp/tow_state). */
typedef enum {
    TOW_STATE_IDLE = 0,       /* no tow, none since the last reset */
    TOW_STATE_CONNECTING = 1, /* the tug drives up and connects */
    TOW_STATE_TOWING = 2,     /* moving, or stopping and disconnecting at the end */
    TOW_STATE_HOLDING = 3,    /* stopped at a hold point, tug connected */
    TOW_STATE_DONE = 4,       /* the tow ended and the tug drove away */
    TOW_STATE_FAILED = 5      /* the tow ended early (stopped, or could not start) */
} tow_state_t;

const char *tow_kind_name(int kind);       /* "reposition"; "none" if unknown */
const char *tow_state_name(int state);     /* "towing"; "idle" if unknown */

/*
 * Why a tow of `kind` cannot be called now, or NULL when it can. The
 * controller's own checks (the aircraft stopped on the ground, the route
 * fitting) come after this.
 */
const char *tow_session_refused(int kind, bool started, bool slave_mode,
    bool emergency_tow_active);

void tow_session_reset(void);
bool tow_session_begin(int kind);
/* The session ends: `completed` when the tug disconnected and drove away. */
void tow_session_finish(bool completed);
bool tow_session_is_active(void);
int tow_session_kind(void);

/*
 * The published state, from the controller's step (pushback_step_t as an
 * int) and whether the tug is held at a hold point.
 */
tow_state_t tow_session_state(int step, bool holding);

/*
 * The tow speed limit another plugin may set (bp/tow_speed_limit, m/s; 0 or
 * less: none) applied to the tug's own speed.
 */
double tow_session_speed_cap(double tug_max, double limit);
/* Storage for bp/tow_speed_limit (the dataref writes it directly). */
float *tow_session_speed_limit(void);

/*
 * Recovery of a disabled aircraft (kind 7, version 11): refused when the nose
 * gear is not down on its wheels or the aircraft is off the pavement (it is
 * then lifted or moved by other means); the tug connects with only chocks
 * holding the aircraft when its brakes have failed; slow.
 */
#define TOW_RECOVERY_SPEED_LIMIT 2.0   /* m/s, deflated tyres and failed brakes */
const char *tow_session_recovery_refused(bool nose_gear_on_wheels,
    bool on_pavement);
/* The speed limit in force (0: none): another plugin's, and recovery's own. */
double tow_session_limit(int kind, double limit);
/* The aircraft is held for the tug to connect: the parking brake, or on a
 * recovery the chocks alone. */
bool tow_session_aircraft_held(int kind, bool parking_brake, bool chocked);

/* A tow route is never saved as a stand's route; no wing walker on tows. */
bool tow_session_allows_persistent_routes(void);
bool tow_session_allows_wing_walker(void);

#ifdef __cplusplus
}
#endif

#endif /* _TOW_SESSION_H_ */
