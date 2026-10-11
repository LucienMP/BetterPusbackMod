#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ext_api_surface.h"

static void
test_x_planes_runway_condition_is_read(void)
{
    assert(bp_surface_from_friction(0) == BP_SURFACE_DRY);
    assert(bp_surface_from_friction(0.4) == BP_SURFACE_DRY);
    assert(bp_surface_from_friction(1) == BP_SURFACE_WET);
    assert(bp_surface_from_friction(3) == BP_SURFACE_WET);
    assert(bp_surface_from_friction(4) == BP_SURFACE_STANDING_WATER);
    assert(bp_surface_from_friction(6.2) == BP_SURFACE_STANDING_WATER);
    assert(bp_surface_from_friction(7) == BP_SURFACE_SNOW);
    assert(bp_surface_from_friction(10) == BP_SURFACE_ICE);
    assert(bp_surface_from_friction(12) == BP_SURFACE_ICE);
    assert(bp_surface_from_friction(13) == BP_SURFACE_SNOW_ON_ICE);
    assert(bp_surface_from_friction(15) == BP_SURFACE_SNOW_ON_ICE);
    assert(bp_surface_from_friction(99) == BP_SURFACE_SNOW_ON_ICE);
    assert(bp_surface_from_friction(-1) == BP_SURFACE_DRY);
    assert(bp_surface_from_friction(NAN) == BP_SURFACE_DRY);
}

static void
test_another_plugin_knows_the_apron_better(void)
{
    /* The runways are icy, but the apron was de-iced. */
    assert(bp_surface_choose(BP_SURFACE_WET, 11) == BP_SURFACE_WET);
    assert(bp_surface_choose(-1, 11) == BP_SURFACE_ICE);
    assert(bp_surface_choose(6, 0) == BP_SURFACE_DRY);   /* not a surface */
}

static void
test_slower_and_gentler_on_slippery_aprons(void)
{
    assert(bp_surface_speed_factor(BP_SURFACE_DRY) == 1.0);
    assert(bp_surface_accel_factor(BP_SURFACE_DRY) == 1.0);
    for (int s = 1; s < BP_SURFACE_COUNT; s++) {
        assert(bp_surface_speed_factor(s) < 1.0);
        assert(bp_surface_accel_factor(s) <= bp_surface_speed_factor(s));
        assert(bp_surface_speed_factor(s) <=
            bp_surface_speed_factor(s - 1));
    }
    assert(bp_surface_speed_factor(42) == 1.0);
    assert(strcmp(bp_surface_name(BP_SURFACE_STANDING_WATER),
        "standing_water") == 0);
    assert(strcmp(bp_surface_name(-3), "dry") == 0);
}

static void
test_lights_at_night_and_in_fog(void)
{
    assert(!bp_surface_lights_needed(30, 10000));
    assert(bp_surface_lights_needed(2, 10000));      /* dusk */
    assert(bp_surface_lights_needed(-20, 10000));    /* night */
    assert(bp_surface_lights_needed(30, 800));       /* fog */
    assert(!bp_surface_lights_needed(30, -1));       /* unknown visibility */
    assert(!bp_surface_lights_needed(NAN, NAN));
}

int
main(void)
{
    test_x_planes_runway_condition_is_read();
    test_another_plugin_knows_the_apron_better();
    test_slower_and_gentler_on_slippery_aprons();
    test_lights_at_night_and_in_fog();
    printf("ext_api_surface tests passed\n");
    return (0);
}
