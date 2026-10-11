#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ext_api_tugpath.h"

static bool
near(double a, double b, double tol)
{
    return (fabs(a - b) <= tol);
}

static void
test_a_path_is_read(void)
{
    bp_ext_tugpath_t path;
    char reason[BP_EXT_TUGPATH_REASON_LEN];

    assert(bp_ext_tugpath_parse("# from the service roads\n"
        "BPTUG 1\r\n"
        "O 47.0 15.0 90 Tug parking North  \n"
        "W 47.001 15.001\n"
        "\n"
        "W 47.002 15.001\n", &path, reason, sizeof (reason)));
    assert(reason[0] == '\0');
    assert(near(path.origin.lat, 47.0, 1e-12));
    assert(near(path.origin_hdg, 90, 1e-12));
    assert(strcmp(path.name, "Tug parking North") == 0);
    assert(path.n == 2);
    assert(near(path.way[1].lat, 47.002, 1e-12));

    /* No road points: the tug drives from its parking place straight in. */
    assert(bp_ext_tugpath_parse("BPTUG 1\nO 47 15 360\n", &path, reason,
        sizeof (reason)));
    assert(path.n == 0 && path.name[0] == '\0' && path.origin_hdg == 0);
}

static void
test_nothing_written_is_no_path_and_no_complaint(void)
{
    bp_ext_tugpath_t path;
    char reason[BP_EXT_TUGPATH_REASON_LEN] = "x";

    assert(!bp_ext_tugpath_parse("", &path, reason, sizeof (reason)));
    assert(reason[0] == '\0');
    assert(!bp_ext_tugpath_parse("  \n# nothing\n", &path, reason,
        sizeof (reason)));
    assert(reason[0] == '\0');
    assert(!bp_ext_tugpath_parse(NULL, &path, reason, sizeof (reason)));
}

static void
test_mistakes_are_named(void)
{
    bp_ext_tugpath_t path;
    char reason[BP_EXT_TUGPATH_REASON_LEN];
    char many[BP_EXT_TUGPATH_TEXT_LEN] = "BPTUG 1\nO 47 15 0\n";

    assert(!bp_ext_tugpath_parse("BPROUTE 1\n", &path, reason,
        sizeof (reason)));
    assert(strstr(reason, "line 1") != NULL);
    assert(!bp_ext_tugpath_parse("BPTUG 2\nO 47 15 0\n", &path, reason,
        sizeof (reason)));
    assert(strstr(reason, "version") != NULL);
    assert(!bp_ext_tugpath_parse("BPTUG 1\nW 47 15\n", &path, reason,
        sizeof (reason)));
    assert(strstr(reason, "no origin") != NULL);
    assert(!bp_ext_tugpath_parse("BPTUG 1\nO 47 15 0\nO 47 15 0\n", &path,
        reason, sizeof (reason)));
    assert(strstr(reason, "line 3: a second origin") != NULL);
    assert(!bp_ext_tugpath_parse("BPTUG 1\nO 47 15\n", &path, reason,
        sizeof (reason)));
    assert(!bp_ext_tugpath_parse("BPTUG 1\nO 91 15 0\n", &path, reason,
        sizeof (reason)));
    assert(!bp_ext_tugpath_parse("BPTUG 1\nO 47 15 400\n", &path, reason,
        sizeof (reason)));
    assert(!bp_ext_tugpath_parse("BPTUG 1\nO 47 15 0\nW 47 15 3\n", &path,
        reason, sizeof (reason)));
    assert(!bp_ext_tugpath_parse("BPTUG 1\nO 47 15 0\nW 47 east\n", &path,
        reason, sizeof (reason)));
    assert(!bp_ext_tugpath_parse("BPTUG 1\nO 47 15 0\nP 47 15 0\n", &path,
        reason, sizeof (reason)));
    assert(strstr(reason, "unknown line") != NULL);

    for (int i = 0; i <= BP_EXT_TUGPATH_MAX_POINTS; i++)
        strcat(many, "W 47.001 15.001\n");
    assert(!bp_ext_tugpath_parse(many, &path, reason, sizeof (reason)));
    assert(strstr(reason, "too many") != NULL);
}

static void
test_distances_and_bearings(void)
{
    bp_ext_geo_t a = { 47.0, 15.0 }, north = { 47.001, 15.0 },
        east = { 47.0, 15.001 };

    /* A thousandth of a degree of latitude is about 111 m. */
    assert(near(bp_ext_geo_distance(a, north), 111.2, 0.5));
    assert(near(bp_ext_geo_distance(a, east), 111.2 * cos(47.0 *
        3.14159265358979323846 / 180), 0.5));
    assert(near(bp_ext_geo_bearing(a, north), 0, 0.01));
    assert(near(bp_ext_geo_bearing(a, east), 90, 0.01));
    assert(near(bp_ext_geo_bearing(north, a), 180, 0.01));
}

static void
test_a_path_far_from_the_aircraft_is_refused(void)
{
    bp_ext_tugpath_t path;
    char reason[BP_EXT_TUGPATH_REASON_LEN];
    bp_ext_geo_t acf = { 47.002, 15.0 }, elsewhere = { 48.0, 15.0 };

    assert(bp_ext_tugpath_parse("BPTUG 1\nO 47.0 15.0 0 Parking\n"
        "W 47.001 15.0\n", &path, reason, sizeof (reason)));
    assert(bp_ext_tugpath_usable(&path, acf, reason, sizeof (reason)));
    assert(!bp_ext_tugpath_usable(&path, elsewhere, reason, sizeof (reason)));
    assert(strstr(reason, "origin") != NULL);

    path.way[0].lat = 47.05;      /* a road point 5 km away */
    assert(!bp_ext_tugpath_usable(&path, acf, reason, sizeof (reason)));
    assert(strstr(reason, "road point 1") != NULL);
}

static void
test_length_and_drive_time(void)
{
    bp_ext_tugpath_t path;
    char reason[BP_EXT_TUGPATH_REASON_LEN];
    bp_ext_geo_t nose = { 47.002, 15.0 };

    assert(bp_ext_tugpath_parse("BPTUG 1\nO 47.0 15.0 0\nW 47.001 15.0\n",
        &path, reason, sizeof (reason)));
    assert(near(bp_ext_tugpath_length(&path, nose), 222.4, 1.0));
    path.n = 0;
    assert(near(bp_ext_tugpath_length(&path, nose), 222.4, 1.0));

    assert(near(bp_ext_tugpath_drive_time(200, 5, 30), 30 + 80, 1e-9));
    /* A standing tug's speed never divides by nothing. */
    assert(near(bp_ext_tugpath_drive_time(10, 0, 0), 20, 1e-9));
    assert(near(bp_ext_tugpath_drive_time(-5, 5, 30), 30, 1e-9));
}

int
main(void)
{
    test_a_path_is_read();
    test_nothing_written_is_no_path_and_no_complaint();
    test_mistakes_are_named();
    test_distances_and_bearings();
    test_a_path_far_from_the_aircraft_is_refused();
    test_length_and_drive_time();
    printf("ext_api_tugpath tests passed\n");
    return (0);
}
