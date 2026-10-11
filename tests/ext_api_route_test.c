#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ext_api_route.h"

static bp_ext_pose_t poses[BP_EXT_ROUTE_MAX_POSES];
static char reason[BP_EXT_ROUTE_REASON_LEN];

static int
parse(const char *text)
{
    reason[0] = '\0';
    return (bp_ext_route_parse(text, poses, BP_EXT_ROUTE_MAX_POSES, reason,
        sizeof (reason)));
}

static void
test_a_route_is_read(void)
{
    int n = parse("BPROUTE 1\n"
        "# push back to face west\n"
        "\n"
        "P 47.7931234 12.9975123 180\r\n"
        "  P\t47.7924 12.9969 270.5 push\n");

    assert(n == 2);
    assert(fabs(poses[0].lat - 47.7931234) < 1e-9);
    assert(fabs(poses[0].lon - 12.9975123) < 1e-9);
    assert(poses[0].hdg == 180);
    assert(poses[1].hdg == 270.5);
    assert(!poses[1].backward);           /* the push/tow word is ignored */
    assert(parse("BPROUTE 1\nP 0 0 360\n") == 1 && poses[0].hdg == 0);
}

static void
test_bad_routes_are_refused_with_a_reason(void)
{
    assert(parse("") == -1 && strstr(reason, "BPROUTE") != NULL);
    assert(parse("P 47 12 180\n") == -1 && strstr(reason, "BPROUTE") != NULL);
    assert(parse("BPROUTE 3\nP 47 12 180\n") == -1 &&
        strstr(reason, "version") != NULL);
    /* W and H lines only in version 2. */
    assert(parse("BPROUTE 1\nW 47 12\n") == -1 && strstr(reason, "line 2"));
    assert(parse("BPROUTE 1\n") == -1 && strstr(reason, "no positions"));
    assert(parse("BPROUTE 1\nP 47 12\n") == -1 &&
        strstr(reason, "line 2") != NULL);
    assert(parse("BPROUTE 1\nP 47 east 180\n") == -1 &&
        strstr(reason, "not a number") != NULL);
    assert(parse("BPROUTE 1\nP 95 12 180\n") == -1 &&
        strstr(reason, "range") != NULL);
    assert(parse("BPROUTE 1\nP 47 12 361\n") == -1 &&
        strstr(reason, "heading") != NULL);
    assert(parse("BPROUTE 1\nP 47 12 nan\n") == -1);
    assert(parse("BPROUTE 1\nX 47 12 180\n") == -1);
    assert(bp_ext_route_parse(NULL, poses, 4, reason, sizeof (reason)) == -1);
}

static void
test_too_many_positions_are_refused(void)
{
    char text[BP_EXT_ROUTE_TEXT_LEN];
    size_t used = (size_t)snprintf(text, sizeof (text), "BPROUTE 1\n");

    for (int i = 0; i <= BP_EXT_ROUTE_MAX_POSES; i++)
        used += (size_t)snprintf(text + used, sizeof (text) - used,
            "P 47.%d 12 180\n", i);
    assert(parse(text) == -1 && strstr(reason, "too many") != NULL);
}

static void
test_version_2_waypoints_and_hold_points(void)
{
    int n = parse("BPROUTE 2\n"
        "# tow to Hangar 7 across runway 15/33\n"
        "P 47.79260200 12.99802200 101.6\n"
        "W 47.79302000 12.99900000\n"
        "H 47.79330000 13.00120000   hold short of runway 15/33  \n"
        "W 47.79380000 13.00450000\n"
        "P 47.79398700 13.00676300 340.8\n");

    assert(n == 5);
    assert(poses[0].kind == BP_EXT_POSE_POSITION && poses[0].hdg == 101.6);
    assert(poses[1].kind == BP_EXT_POSE_WAYPOINT);
    assert(poses[2].kind == BP_EXT_POSE_HOLD);
    assert(strcmp(poses[2].hold_text, "hold short of runway 15/33") == 0);
    /* A waypoint faces the next position: here roughly east-north-east. */
    assert(fabs(poses[1].hdg - bp_ext_route_bearing(poses[1].lat,
        poses[1].lon, poses[2].lat, poses[2].lon)) < 1e-9);
    assert(poses[1].hdg > 60 && poses[1].hdg < 80);
    assert(poses[4].hdg == 340.8);
    /* The last waypoint keeps the heading it arrives with. */
    assert(parse("BPROUTE 2\nP 47 12 90\nW 47 12.001\n") == 2 &&
        fabs(poses[1].hdg - 90) < 0.01);
    assert(parse("BPROUTE 2\nW 47 12\n") == -1 && strstr(reason, "one waypoint"));
    assert(parse("BPROUTE 2\nH 47 12\n") == -1 && strstr(reason, "holds"));
    assert(parse("BPROUTE 2\nW 47 12 90\n") == -1 && strstr(reason, "W <"));
    assert(fabs(bp_ext_route_bearing(47, 12, 47.001, 12) - 0) < 1e-9);
    assert(fabs(bp_ext_route_bearing(47, 12, 47, 12.001) - 90) < 1e-9);
    assert(fabs(bp_ext_route_bearing(47, 12, 46.999, 12) - 180) < 1e-9);
    assert(fabs(bp_ext_route_bearing(47, 12, 47, 11.999) - 270) < 1e-9);
}

static void
test_sixty_four_positions_fit(void)
{
    char text[BP_EXT_ROUTE_TEXT_LEN];
    size_t used = (size_t)snprintf(text, sizeof (text), "BPROUTE 2\n");

    for (int i = 0; i < BP_EXT_ROUTE_MAX_POSES; i++)
        used += (size_t)snprintf(text + used, sizeof (text) - used,
            "W 47.%04d 12\n", i);
    assert(BP_EXT_ROUTE_MAX_POSES == 64);
    assert(parse(text) == 64);
    /* Written back, 64 positions with hold texts fit the published text. */
    for (int i = 0; i < 64; i++) {
        poses[i].kind = BP_EXT_POSE_HOLD;
        (void)snprintf(poses[i].hold_text, sizeof (poses[i].hold_text),
            "hold short of taxiway %d", i);
    }
    assert(bp_ext_route_format(poses, 64, text, sizeof (text)));
    assert(strncmp(text, "BPROUTE 2\n", 10) == 0);
}

static void
test_a_written_route_reads_back(void)
{
    bp_ext_pose_t out[3] = {
        { 47.79312345, 12.99751234, 180.0, true, BP_EXT_POSE_POSITION, "" },
        { 47.79240001, 12.99690002, 270.25, false, BP_EXT_POSE_POSITION, "" },
        { 47.79200000, 12.99600000, 270.0, false, BP_EXT_POSE_HOLD,
          "hold short of B" }
    };
    char text[BP_EXT_ROUTE_TEXT_LEN];

    assert(bp_ext_route_format(out, 2, text, sizeof (text)));
    assert(strncmp(text, "BPROUTE 1\n", 10) == 0);   /* no hold: version 1 */
    assert(strstr(text, " push\n") != NULL && strstr(text, " tow\n") != NULL);
    assert(parse(text) == 2);
    assert(fabs(poses[0].lat - out[0].lat) < 1e-8);
    assert(fabs(poses[1].lon - out[1].lon) < 1e-8);
    assert(fabs(poses[1].hdg - out[1].hdg) < 1e-2);
    /* A buffer too small is refused, never cut short. */
    assert(!bp_ext_route_format(out, 2, text, 30));
    /* With a hold point: version 2, the H line with its text. */
    assert(bp_ext_route_format(out, 3, text, sizeof (text)));
    assert(strncmp(text, "BPROUTE 2\n", 10) == 0);
    assert(strstr(text, "H 47.79200000 12.99600000 hold short of B\n") != NULL);
    assert(parse(text) == 3 && poses[2].kind == BP_EXT_POSE_HOLD);
}

static bp_ext_route_gate_t
idle_gate(void)
{
    bp_ext_route_gate_t gate;

    memset(&gate, 0, sizeof (gate));
    gate.ready = true;
    return (gate);
}

static void
test_a_route_may_change_only_when_safe(void)
{
    bp_ext_route_gate_t gate = idle_gate();

    /* Before calling the tug (the classic plan-first flow). */
    assert(bp_ext_route_change_refused(&gate) == NULL);
    /* Tug connecting or pushing: refused. */
    gate.started = true;
    assert(strstr(bp_ext_route_change_refused(&gate), "cannot change now"));
    /* The connected tug waits for a plan. */
    gate.awaiting_plan = true;
    assert(bp_ext_route_change_refused(&gate) == NULL);
    /* The connected hold with the parking brake set (Change plan). */
    gate.awaiting_plan = false;
    gate.can_replan = true;
    assert(bp_ext_route_change_refused(&gate) == NULL);
}

static void
test_the_pilot_and_the_aircraft_come_first(void)
{
    bp_ext_route_gate_t gate = idle_gate();

    gate.planner_open = true;
    assert(strstr(bp_ext_route_change_refused(&gate), "planner"));
    gate = idle_gate();
    gate.manual_push = true;
    assert(strstr(bp_ext_route_change_refused(&gate), "manual push"));
    gate = idle_gate();
    gate.slave_mode = true;
    assert(strstr(bp_ext_route_change_refused(&gate), "shared cockpit"));
    gate = idle_gate();
    gate.ready = false;
    assert(strstr(bp_ext_route_change_refused(&gate), "aircraft"));
    /* Even while waiting for a plan, an open planner wins. */
    gate = idle_gate();
    gate.started = gate.awaiting_plan = gate.planner_open = true;
    assert(bp_ext_route_change_refused(&gate) != NULL);
}

int
main(void)
{
    test_a_route_is_read();
    test_bad_routes_are_refused_with_a_reason();
    test_too_many_positions_are_refused();
    test_version_2_waypoints_and_hold_points();
    test_sixty_four_positions_fit();
    test_a_written_route_reads_back();
    test_a_route_may_change_only_when_safe();
    test_the_pilot_and_the_aircraft_come_first();
    printf("ext_api_route tests passed\n");
    return (0);
}
