/*
 * External interface, version 15 (README-EXTERNAL-API.md): push-back routes
 * drawn in a scenery's WorldEditor project. BetterPushback/recreate_scenery_routes
 * converts each scenery's earth.wed.xml into BetterPushback_routes.dat beside
 * it (wed2route.c); this reads those files, finds the routes that start where
 * the aircraft stands, and turns one into positions for the route loader.
 *
 * Pure computations, no XPLM dependencies (tests/scenery_route_test.c).
 */

#ifndef _SCENERY_ROUTE_H_
#define _SCENERY_ROUTE_H_

#include <stdbool.h>
#include <stddef.h>

#include "ext_api_route.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BP_SCENERY_ROUTE_MAX_SEGS 128
/* How near the aircraft a route must start to be offered. */
#define BP_SCENERY_ROUTE_MAX_DIST_M 20.0
#define BP_SCENERY_ROUTE_MAX_HDG_DEG 30.0

/* One segment's end, as the route file stores it. */
typedef struct {
    double lat, lon, hdg;
    bool backward;
    bool user_placed;     /* a node of the WED route ends here */
} bp_scenery_seg_end_t;

typedef struct {
    double lat, lon, hdg;                 /* where the route starts */
    int n;
    bp_scenery_seg_end_t ends[BP_SCENERY_ROUTE_MAX_SEGS];
    double distance;                      /* from the aircraft, when matched */
} bp_scenery_route_t;

/*
 * Reads a route file's text into `routes` (up to `max`) and returns how many.
 * A route the file got wrong is left out; the rest are read.
 */
int bp_scenery_routes_parse(const char *text, bp_scenery_route_t *routes,
    int max);

/*
 * True when the route starts within BP_SCENERY_ROUTE_MAX_DIST_M of the
 * aircraft and within BP_SCENERY_ROUTE_MAX_HDG_DEG of its heading; its
 * distance is noted in the route.
 */
bool bp_scenery_route_matches(bp_scenery_route_t *route, double lat,
    double lon, double hdg);

/* Sorts matched routes nearest first. */
void bp_scenery_routes_sort(bp_scenery_route_t *routes, int n);

/*
 * The positions the route passes, for the route loader: the end of each WED
 * node, or, in a file converted before nodes were marked, each change of
 * direction; always the route's end. Returns how many (at most `max`).
 */
int bp_scenery_route_poses(const bp_scenery_route_t *route,
    bp_ext_pose_t *poses, int max);

#ifdef __cplusplus
}
#endif

#endif /* _SCENERY_ROUTE_H_ */
