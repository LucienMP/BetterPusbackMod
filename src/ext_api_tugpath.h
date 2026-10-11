/*
 * External interface, version 14 (README-EXTERNAL-API.md): where the tug
 * comes from. Another plugin that knows the airport's service roads writes
 * the tug's parking place and the road to the stand into bp/tug_path_in:
 *
 *     BPTUG 1
 *     O <latitude> <longitude> <heading> [name]
 *     W <latitude> <longitude>
 *     ...
 *
 * O is the tug's parking place (once); each W a point on the road to the
 * stand, in driving order. On the way back the tug drives the same road the
 * other way and parks at O. Without a path, the tug appears and leaves as it
 * always has.
 *
 * Pure computations, no XPLM dependencies (tests/ext_api_tugpath_test.c).
 */

#ifndef _EXT_API_TUGPATH_H_
#define _EXT_API_TUGPATH_H_

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BP_EXT_TUGPATH_FORMAT 1
#define BP_EXT_TUGPATH_TEXT_LEN 2048
#define BP_EXT_TUGPATH_MAX_POINTS 32
#define BP_EXT_TUGPATH_NAME_LEN 48
#define BP_EXT_TUGPATH_REASON_LEN 128
/* Farther than this from the aircraft, a point is taken as a mistake. */
#define BP_EXT_TUGPATH_MAX_DIST_M 3000.0

typedef struct {
    double lat, lon;
} bp_ext_geo_t;

typedef struct {
    bp_ext_geo_t origin;
    double origin_hdg;
    char name[BP_EXT_TUGPATH_NAME_LEN];
    int n;                                        /* road points */
    bp_ext_geo_t way[BP_EXT_TUGPATH_MAX_POINTS];
} bp_ext_tugpath_t;

/* Published numbers (bp/tug_path_status): fixed forever. */
typedef enum {
    BP_EXT_TUGPATH_NONE = 0,       /* no path written: BetterPushback's own */
    BP_EXT_TUGPATH_USED = 1,       /* the tug came along the path */
    BP_EXT_TUGPATH_REFUSED = 2     /* not used; bp/tug_path_reason says why */
} bp_ext_tugpath_status_t;

/*
 * Reads the text. False with the reason when it is not a path; an empty text
 * (nothing but blanks and # comments) is false with an empty reason.
 */
bool bp_ext_tugpath_parse(const char *text, bp_ext_tugpath_t *path,
    char *reason, size_t reason_len);

/* Metres between two points (great circle). */
double bp_ext_geo_distance(bp_ext_geo_t a, bp_ext_geo_t b);

/* Bearing from a to b, degrees true. */
double bp_ext_geo_bearing(bp_ext_geo_t a, bp_ext_geo_t b);

/*
 * True when the path can be used for an aircraft at `acf`: every point
 * within BP_EXT_TUGPATH_MAX_DIST_M of it. False with the reason otherwise.
 */
bool bp_ext_tugpath_usable(const bp_ext_tugpath_t *path, bp_ext_geo_t acf,
    char *reason, size_t reason_len);

/* Metres from the origin along the road to `to` (the aircraft's nose). */
double bp_ext_tugpath_length(const bp_ext_tugpath_t *path, bp_ext_geo_t to);

/*
 * Seconds a drive of `length` metres takes at `speed` m/s, with `slack`
 * seconds for the turns at the end; used to wait for the tug driving back.
 */
double bp_ext_tugpath_drive_time(double length, double speed, double slack);

#ifdef __cplusplus
}
#endif

#endif /* _EXT_API_TUGPATH_H_ */
