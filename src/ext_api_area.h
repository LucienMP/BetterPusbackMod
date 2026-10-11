/*
 * External interface, version 20 (README-EXTERNAL-API.md): the area the
 * operation will sweep, so a ground-handling plugin can move its vehicles
 * and people out of the way and an ATC plugin can keep its traffic clear.
 *
 * The route's segments (straights and turns, as BetterPushback drives them)
 * are sampled into poses; the aircraft's footprint at each pose makes up the
 * swept area. The tug's road in (version 14) is published beside it.
 *
 * Pure computations, no XPLM dependencies (tests/ext_api_area_test.c).
 */

#ifndef _EXT_API_AREA_H_
#define _EXT_API_AREA_H_

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BP_EXT_AREA_FORMAT 1
#define BP_EXT_AREA_TEXT_LEN 8192
/* Footprints are this far apart at most, metres. */
#define BP_EXT_AREA_STEP_M 5.0
/* Added all round the aircraft's footprint, metres. */
#define BP_EXT_AREA_MARGIN_M 2.0

/* Local metres (x east, y north) and a true heading in degrees. */
typedef struct {
    double x, y, hdg;
} bp_area_pose_t;

/* One driving segment, as BetterPushback plans it. */
typedef struct {
    bool turn;
    double x0, y0, hdg0;     /* start */
    double x1, y1, hdg1;     /* end */
    double r;                /* turn radius */
    bool right;              /* the turn's centre is right of the heading */
} bp_area_seg_t;

/*
 * Poses along the segments, no more than `step` metres apart (at least one
 * per segment end, turns followed along their arc): the first segment's
 * start, then each sample up to the last segment's end. Returns how many
 * were put in `out` (at most `max`; the end is kept when cut short).
 */
int bp_area_sample(const bp_area_seg_t *segs, int n, double step,
    bp_area_pose_t *out, int max);

/* Metres along the segments (turns along their arc). */
double bp_area_length(const bp_area_seg_t *segs, int n);

#ifdef __cplusplus
}
#endif

#endif /* _EXT_API_AREA_H_ */
