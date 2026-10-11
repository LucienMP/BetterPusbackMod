/*
 * External interface, version 12 (README-EXTERNAL-API.md): the tug's state
 * and position, estimates before anything moves, and the operation's
 * milestones. The computations are in ext_api_tug.c; this publishes them.
 */

#include <math.h>
#include <string.h>

#include <XPLMDataAccess.h>
#include <XPLMGraphics.h>

#include <acfutils/dr.h>
#include <acfutils/log.h>

#include "bp.h"
#include "ext_api_tug.h"
#include "ext_api_tugpath.h"
#include "ext_tug.h"

/* Before the tug is called: the drive-up the tug makes today, and connecting. */
#define CONNECT_TIME_S 45.0
#define DRIVE_UP_TIME_S 30.0
#define REVERSE_PAUSE_S 4.0

static bool_t inited = B_FALSE;
static int tug_state = BP_EXT_TUG_NONE;
static char tug_state_name[16] = "none";
static float tug_eta = -1, tug_lat = 0, tug_lon = 0, tug_hdg = 0;
static char tug_origin[48] = "";
static float eta_if_called = -1, connect_time = (float)CONNECT_TIME_S;
static float route_time = -1;
static float time_off_blocks = -1, time_push_complete = -1, time_tug_clear = -1;
static bp_ext_milestones_t milestones;
static int last_step = PB_STEP_OFF;
static dr_t zulu_dr, lat_dr, lon_dr;
/* Version 14: the tug's parking place and road, and the prepositioned tug. */
static char tug_path_in[BP_EXT_TUGPATH_TEXT_LEN] = "";
static int tug_path_status = BP_EXT_TUGPATH_NONE;
static char tug_path_reason[BP_EXT_TUGPATH_REASON_LEN] = "";
static int prepositioned = 0;
static dr_t tug_path_in_dr, tug_path_status_dr, tug_path_reason_dr,
    prepositioned_dr;
/* An estimate for the drive in along a road, before the tug is chosen. */
#define ROAD_SPEED_MPS 6.0

static dr_t tug_state_dr, tug_state_name_dr, tug_eta_dr, tug_lat_dr, tug_lon_dr,
    tug_hdg_dr, tug_origin_dr, eta_if_called_dr, connect_time_dr, route_time_dr,
    off_blocks_dr, push_complete_dr, tug_clear_dr;

static void
refresh_route_time(void)
{
    bp_ext_leg_t legs[256];
    double fwd, rev, accel;
    int n = bp_route_legs(legs, (int)(sizeof (legs) / sizeof (legs[0])));

    bp_route_speeds(&fwd, &rev, &accel);
    route_time = n > 0 ? (float)bp_ext_route_time(legs, n, fwd, rev, accel,
        REVERSE_PAUSE_S) : -1.0f;
}

const char *
ext_tug_path_text(void)
{
    /* Written by another plugin: never trusted to be terminated. */
    tug_path_in[sizeof (tug_path_in) - 1] = '\0';
    return (tug_path_in);
}

/* Seconds to drive in along the road written now, or the usual drive-up. */
static double
drive_up_time(void)
{
    bp_ext_tugpath_t path;
    bp_ext_geo_t acf;
    char reason[BP_EXT_TUGPATH_REASON_LEN];

    acf.lat = dr_getf(&lat_dr);
    acf.lon = dr_getf(&lon_dr);
    if (!bp_ext_tugpath_parse(ext_tug_path_text(), &path, reason,
        sizeof (reason)) || !bp_ext_tugpath_usable(&path, acf, reason,
        sizeof (reason)))
        return (DRIVE_UP_TIME_S);
    return (bp_ext_tugpath_drive_time(bp_ext_tugpath_length(&path, acf),
        ROAD_SPEED_MPS, DRIVE_UP_TIME_S));
}

bool_t
ext_tug_refresh(void)
{
    int step = (int)bp_current_step();
    int state = bp_ext_tug_state(step);
    bool_t changed = (state != tug_state);
    double lat, lon, hdg, spd, distance;
    double ground_speed = 0;

    if (step != PB_STEP_OFF && last_step == PB_STEP_OFF) {
        bp_ext_milestones_reset(&milestones);   /* a new operation */
        changed = B_TRUE;
    }
    last_step = step;

    /* Version 14: waiting at its parking place, or at the nose. */
    if (state == BP_EXT_TUG_DRIVING_IN && bp_tug_at_parking())
        state = BP_EXT_TUG_AT_PARKING;
    if (bp_prepositioned())
        state = BP_EXT_TUG_AT_STAND;
    changed = changed || (state != tug_state);
    if (bp_tug_path_status() != tug_path_status ||
        (bp_prepositioned() ? 1 : 0) != prepositioned)
        changed = B_TRUE;
    tug_path_status = bp_tug_path_status();
    (void)snprintf(tug_path_reason, sizeof (tug_path_reason), "%s",
        bp_tug_path_reason());
    prepositioned = bp_prepositioned() ? 1 : 0;
    (void)snprintf(tug_origin, sizeof (tug_origin), "%s",
        bp_tug_origin_name());

    tug_state = state;
    (void)snprintf(tug_state_name, sizeof (tug_state_name), "%s",
        bp_ext_tug_state_name(state));
    if (bp_tug_position(&lat, &lon, &hdg, &spd)) {
        tug_lat = (float)lat;
        tug_lon = (float)lon;
        tug_hdg = (float)hdg;
        distance = bp_tug_distance_to_aircraft();
        tug_eta = (state == BP_EXT_TUG_DRIVING_IN && distance >= 0) ?
            (float)bp_ext_eta(distance, fabs(spd)) :
            (state == BP_EXT_TUG_DRIVING_IN ? -1.0f : 0.0f);
    } else {
        tug_lat = tug_lon = tug_hdg = 0;
        tug_eta = -1;
    }

    /* Before the tug is called, how long until it would be connected. */
    eta_if_called = (state == BP_EXT_TUG_NONE) ?
        (float)(drive_up_time() + CONNECT_TIME_S) : -1.0f;
    refresh_route_time();

    {
        double metric_distance;
        bool_t speed_valid, distance_valid;

        bp_get_ground_ops_metrics(&ground_speed, &speed_valid,
            &metric_distance, &distance_valid);
        if (!speed_valid)
            ground_speed = 0;
    }
    if (bp_ext_milestones_update(&milestones, step, ground_speed,
        dr_getf(&zulu_dr)))
        changed = B_TRUE;
    time_off_blocks = (float)milestones.off_blocks;
    time_push_complete = (float)milestones.push_complete;
    time_tug_clear = (float)milestones.tug_clear;
    return (changed);
}

void
ext_tug_init(void)
{
    if (inited)
        return;
    bp_ext_milestones_reset(&milestones);
    fdr_find(&zulu_dr, "sim/time/zulu_time_sec");
    fdr_find(&lat_dr, "sim/flightmodel/position/latitude");
    fdr_find(&lon_dr, "sim/flightmodel/position/longitude");
    dr_create_b(&tug_path_in_dr, tug_path_in, sizeof (tug_path_in), B_TRUE,
        "bp/tug_path_in");
    dr_create_i(&tug_path_status_dr, &tug_path_status, B_FALSE,
        "bp/tug_path_status");
    dr_create_b(&tug_path_reason_dr, tug_path_reason,
        sizeof (tug_path_reason), B_FALSE, "bp/tug_path_reason");
    dr_create_i(&prepositioned_dr, &prepositioned, B_FALSE,
        "bp/prepositioned");
    dr_create_i(&tug_state_dr, &tug_state, B_FALSE, "bp/tug_state");
    dr_create_b(&tug_state_name_dr, tug_state_name, sizeof (tug_state_name),
        B_FALSE, "bp/tug_state_name");
    dr_create_f(&tug_eta_dr, &tug_eta, B_FALSE, "bp/tug_eta_s");
    dr_create_f(&tug_lat_dr, &tug_lat, B_FALSE, "bp/tug_lat");
    dr_create_f(&tug_lon_dr, &tug_lon, B_FALSE, "bp/tug_lon");
    dr_create_f(&tug_hdg_dr, &tug_hdg, B_FALSE, "bp/tug_hdg");
    dr_create_b(&tug_origin_dr, tug_origin, sizeof (tug_origin), B_FALSE,
        "bp/tug_origin");
    dr_create_f(&eta_if_called_dr, &eta_if_called, B_FALSE,
        "bp/tug_eta_if_called_s");
    dr_create_f(&connect_time_dr, &connect_time, B_FALSE, "bp/connect_time_s");
    dr_create_f(&route_time_dr, &route_time, B_FALSE, "bp/route_time_s");
    dr_create_f(&off_blocks_dr, &time_off_blocks, B_FALSE,
        "bp/time_off_blocks");
    dr_create_f(&push_complete_dr, &time_push_complete, B_FALSE,
        "bp/time_push_complete");
    dr_create_f(&tug_clear_dr, &time_tug_clear, B_FALSE, "bp/time_tug_clear");
    inited = B_TRUE;
}

void
ext_tug_fini(void)
{
    if (!inited)
        return;
    dr_delete(&tug_state_dr);
    dr_delete(&tug_state_name_dr);
    dr_delete(&tug_eta_dr);
    dr_delete(&tug_lat_dr);
    dr_delete(&tug_lon_dr);
    dr_delete(&tug_hdg_dr);
    dr_delete(&tug_origin_dr);
    dr_delete(&eta_if_called_dr);
    dr_delete(&connect_time_dr);
    dr_delete(&route_time_dr);
    dr_delete(&off_blocks_dr);
    dr_delete(&push_complete_dr);
    dr_delete(&tug_clear_dr);
    dr_delete(&tug_path_in_dr);
    dr_delete(&tug_path_status_dr);
    dr_delete(&tug_path_reason_dr);
    dr_delete(&prepositioned_dr);
    inited = B_FALSE;
}
