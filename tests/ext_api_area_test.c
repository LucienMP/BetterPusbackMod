#include <assert.h>
#include <math.h>
#include <stdio.h>

#include "ext_api_area.h"

static bool
near(double a, double b, double tol)
{
    return (fabs(a - b) <= tol);
}

static void
test_a_straight_push_is_sampled_every_few_metres(void)
{
    /* Pushed 20 m south, nose north. */
    bp_area_seg_t s = { false, 0, 0, 0, 0, -20, 0, 0, false };
    bp_area_pose_t p[16];
    int n = bp_area_sample(&s, 1, 5, p, 16);

    assert(n == 5);
    assert(near(p[0].y, 0, 1e-9) && near(p[4].y, -20, 1e-9));
    assert(near(p[2].y, -10, 1e-9) && near(p[2].hdg, 0, 1e-9));
    assert(near(bp_area_length(&s, 1), 20, 1e-9));
}

static void
test_a_turn_follows_its_arc(void)
{
    /*
     * Heading north at the origin, turning right on a 10 m radius: the centre
     * is 10 m east; a quarter turn ends 10 m east and 10 m north, heading east.
     */
    bp_area_seg_t s = { true, 0, 0, 0, 10, 10, 90, 10, true };
    bp_area_pose_t p[32];
    int n = bp_area_sample(&s, 1, 2, p, 32);

    assert(near(bp_area_length(&s, 1), 10 * 3.14159265358979 / 2, 1e-6));
    assert(n == 1 + (int)ceil(bp_area_length(&s, 1) / 2));
    assert(near(p[n - 1].x, 10, 1e-6) && near(p[n - 1].y, 10, 1e-6));
    assert(near(p[n - 1].hdg, 90, 1e-6));
    /* Every sample on the arc. */
    for (int i = 0; i < n; i++)
        assert(near(hypot(p[i].x - 10, p[i].y), 10, 1e-6));

    /* The same to the left: centre 10 m west. */
    s = (bp_area_seg_t){ true, 0, 0, 0, -10, 10, 270, 10, false };
    n = bp_area_sample(&s, 1, 2, p, 32);
    assert(near(p[n - 1].x, -10, 1e-6) && near(p[n - 1].y, 10, 1e-6));
    assert(near(p[n - 1].hdg, 270, 1e-6));
    for (int i = 0; i < n; i++)
        assert(near(hypot(p[i].x + 10, p[i].y), 10, 1e-6));

    /* The side noted the other way round: the end decides. */
    s = (bp_area_seg_t){ true, 0, 0, 0, -10, 10, 270, 10, true };
    n = bp_area_sample(&s, 1, 2, p, 32);
    assert(near(p[n - 1].x, -10, 1e-6) && near(p[n - 1].y, 10, 1e-6));
    assert(near(p[n / 2].x, -10 + 10 * cos(45 * 3.14159265358979 / 180),
        0.6));
}

static void
test_segments_join_and_the_end_is_kept(void)
{
    bp_area_seg_t s[2] = {
        { false, 0, 0, 0, 0, -10, 0, 0, false },
        { false, 0, -10, 0, 0, -30, 0, 0, false }
    };
    bp_area_pose_t p[4];
    int n;

    assert(bp_area_sample(s, 2, 5, p, 0) == 0);
    assert(bp_area_sample(s, 0, 5, p, 4) == 0);
    /* Room for four of seven: the last is the route's end. */
    n = bp_area_sample(s, 2, 5, p, 4);
    assert(n == 4 && near(p[3].y, -30, 1e-9));
    /* A step of nothing is taken as the usual one. */
    {
        bp_area_pose_t q[16];

        assert(bp_area_sample(s, 2, 0, q, 16) == 7);
    }
}

int
main(void)
{
    test_a_straight_push_is_sampled_every_few_metres();
    test_a_turn_follows_its_arc();
    test_segments_join_and_the_end_is_kept();
    printf("ext_api_area tests passed\n");
    return (0);
}
