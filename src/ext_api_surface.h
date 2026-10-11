/*
 * External interface, version 17 (README-EXTERNAL-API.md): winter and night.
 * Pushes on a wet, snowy or icy apron are slower and gentler, and the tug's
 * lights are on at night and in low visibility.
 *
 * The surface comes from X-Plane's runway condition
 * (sim/weather/region/runway_friction), unless another plugin that knows the
 * apron better (cleared, de-iced, sanded) sets it in bp/surface_override.
 *
 * Pure computations, no XPLM dependencies (tests/ext_api_surface_test.c).
 */

#ifndef _EXT_API_SURFACE_H_
#define _EXT_API_SURFACE_H_

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Published numbers (bp/surface): fixed forever, new ones at the end. */
typedef enum {
    BP_SURFACE_DRY = 0,
    BP_SURFACE_WET = 1,
    BP_SURFACE_STANDING_WATER = 2,
    BP_SURFACE_SNOW = 3,
    BP_SURFACE_ICE = 4,
    BP_SURFACE_SNOW_ON_ICE = 5
} bp_surface_t;
#define BP_SURFACE_COUNT 6

/* Below this the tug's lights are on: the sun's elevation, degrees... */
#define BP_SURFACE_LIGHTS_SUN_DEG 5.0
/* ...or the visibility, metres. */
#define BP_SURFACE_LIGHTS_VIS_M 1500.0

/*
 * The surface from X-Plane's runway friction enumeration: 0 dry, 1-3 wet,
 * 4-6 puddly, 7-9 snowy, 10-12 icy, 13-15 snowy and icy.
 */
int bp_surface_from_friction(double friction);

/* The surface in use: the override when it is 0-5, else X-Plane's. */
int bp_surface_choose(int override, double friction);

const char *bp_surface_name(int surface);       /* "snow"; "dry" if unknown */

/* What the push's top speeds and its acceleration are multiplied by. */
double bp_surface_speed_factor(int surface);
double bp_surface_accel_factor(int surface);

/* The tug's lights are on: at night, at dusk and dawn, or in low visibility. */
bool bp_surface_lights_needed(double sun_pitch_deg, double visibility_m);

#ifdef __cplusplus
}
#endif

#endif /* _EXT_API_SURFACE_H_ */
