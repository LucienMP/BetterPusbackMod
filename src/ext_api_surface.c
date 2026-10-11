/*
 * External interface, version 17: winter and night. Unit tested on its own
 * (tests/ext_api_surface_test.c).
 */

#include <math.h>

#include "ext_api_surface.h"

static const char *const names[BP_SURFACE_COUNT] = {
    "dry", "wet", "standing_water", "snow", "ice", "snow_on_ice"
};

/*
 * Ground handlers push at a walking pace on snow and slower on ice, and start
 * and stop gently so the tug's wheels keep their grip.
 */
static const double speed_factors[BP_SURFACE_COUNT] = {
    1.0, 0.9, 0.8, 0.6, 0.4, 0.4
};
static const double accel_factors[BP_SURFACE_COUNT] = {
    1.0, 0.8, 0.7, 0.5, 0.35, 0.35
};

int
bp_surface_from_friction(double friction)
{
    int f;

    if (!isfinite(friction) || friction < 0.5)
        return (BP_SURFACE_DRY);
    f = (int)floor(friction + 0.5);
    if (f <= 3)
        return (BP_SURFACE_WET);
    if (f <= 6)
        return (BP_SURFACE_STANDING_WATER);
    if (f <= 9)
        return (BP_SURFACE_SNOW);
    if (f <= 12)
        return (BP_SURFACE_ICE);
    return (BP_SURFACE_SNOW_ON_ICE);
}

int
bp_surface_choose(int override, double friction)
{
    if (override >= 0 && override < BP_SURFACE_COUNT)
        return (override);
    return (bp_surface_from_friction(friction));
}

const char *
bp_surface_name(int surface)
{
    return (surface >= 0 && surface < BP_SURFACE_COUNT ? names[surface] :
        names[BP_SURFACE_DRY]);
}

double
bp_surface_speed_factor(int surface)
{
    return (surface >= 0 && surface < BP_SURFACE_COUNT ?
        speed_factors[surface] : 1.0);
}

double
bp_surface_accel_factor(int surface)
{
    return (surface >= 0 && surface < BP_SURFACE_COUNT ?
        accel_factors[surface] : 1.0);
}

bool
bp_surface_lights_needed(double sun_pitch_deg, double visibility_m)
{
    if (isfinite(sun_pitch_deg) && sun_pitch_deg < BP_SURFACE_LIGHTS_SUN_DEG)
        return (true);
    return (isfinite(visibility_m) && visibility_m >= 0 &&
        visibility_m < BP_SURFACE_LIGHTS_VIS_M);
}
