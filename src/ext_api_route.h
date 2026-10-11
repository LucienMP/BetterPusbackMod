/*
 * External interface: push routes exchanged with other plugins as text
 * (README-EXTERNAL-API.md, "External routes"). Pure parsing and formatting,
 * no XPLM dependencies.
 *
 * A route is the list of positions and true headings the aircraft should
 * reach, one leg after another, starting from where it stands. The positions
 * are the aircraft's reference point (the latitude and longitude X-Plane
 * reports for the aircraft); BetterPushback fits each leg to the aircraft's
 * turning limits, pushing to a target behind it and towing to one ahead.
 *
 *     BPROUTE 1
 *     P 47.7931234 12.9975123 180.0
 *     P 47.7924000 12.9969000 270.0 push
 *
 * Lines starting with '#' and blank lines are ignored. A fifth word on a "P"
 * line (push or tow, written in published routes) is ignored when reading.
 *
 * Version 2 (interface version 8) adds two kinds of line, for tows along a
 * taxi route:
 *
 *     BPROUTE 2
 *     W 47.7930200 12.9990000
 *     H 47.7933000 13.0012000 hold short of runway 15/33
 *     P 47.7939870 13.0067630 340.8
 *
 * "W lat lon" is a waypoint: the aircraft passes over it facing the next
 * position (the last one keeps the heading it arrived with). "H lat lon text"
 * is a hold point: a waypoint where the tug stops and waits until it is told
 * to go on; the text says why. Version 1 routes read as before.
 */

#ifndef _EXT_API_ROUTE_H_
#define _EXT_API_ROUTE_H_

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BP_EXT_ROUTE_FORMAT 1          /* the plain route: P lines only */
#define BP_EXT_ROUTE_FORMAT_HOLDS 2    /* with W and H lines */
#define BP_EXT_ROUTE_MAX_POSES 64
#define BP_EXT_ROUTE_TEXT_LEN 4096
#define BP_EXT_ROUTE_REASON_LEN 128
#define BP_EXT_ROUTE_HOLD_TEXT_LEN 96

typedef enum {
    BP_EXT_POSE_POSITION = 0,          /* P: a position and heading */
    BP_EXT_POSE_WAYPOINT = 1,          /* W: passed over, facing the next */
    BP_EXT_POSE_HOLD = 2               /* H: a waypoint to stop at */
} bp_ext_pose_kind_t;

typedef struct {
    double lat;
    double lon;
    double hdg;                        /* true heading, 0 <= hdg < 360 */
    bool backward;                     /* written routes only: a push leg */
    bp_ext_pose_kind_t kind;
    char hold_text[BP_EXT_ROUTE_HOLD_TEXT_LEN];   /* H only */
} bp_ext_pose_t;

/*
 * Reads a route. Returns the number of poses (at least 1), or -1 with the
 * reason in `reason`.
 */
int bp_ext_route_parse(const char *text, bp_ext_pose_t *poses, int max,
    char *reason, size_t reason_len);

/*
 * True heading from one position to another (degrees, 0 <= h < 360): the
 * heading a waypoint takes from the leg after it.
 */
double bp_ext_route_bearing(double lat1, double lon1, double lat2, double lon2);

/*
 * Writes a route (with push/tow words); returns false if it does not fit.
 * A route with a hold point is written as version 2, with its H lines.
 */
bool bp_ext_route_format(const bp_ext_pose_t *poses, int n, char *text,
    size_t text_len);

/* What decides whether another plugin may change the route now. */
typedef struct {
    bool ready;             /* BetterPushback works with this aircraft */
    bool slave_mode;        /* shared cockpit: the other cockpit plans */
    bool planner_open;
    bool manual_push;
    bool started;           /* an operation is running */
    bool awaiting_plan;     /* the connected tug waits for a plan */
    bool can_replan;        /* the connected hold, parking brake set */
} bp_ext_route_gate_t;

/*
 * NULL when the route may be changed (loaded, cleared, a slot loaded): before
 * an operation, while the connected tug waits for a plan, and during the
 * connected hold. Otherwise why not, in words.
 */
const char *bp_ext_route_change_refused(const bp_ext_route_gate_t *gate);

#ifdef __cplusplus
}
#endif

#endif /* _EXT_API_ROUTE_H_ */
