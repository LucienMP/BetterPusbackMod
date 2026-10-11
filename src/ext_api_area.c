/*
 * External interface, version 20: the area the operation will sweep. Unit
 * tested on its own (tests/ext_api_area_test.c).
 */

#include <math.h>

#include "ext_api_area.h"

#define D2R (3.14159265358979323846 / 180.0)

/* Signed degrees from a to b, -180..180. */
static double
rel_hdg(double a, double b)
{
    double d = fmod(b - a, 360.0);

    if (d > 180)
        d -= 360;
    if (d < -180)
        d += 360;
    return (d);
}

static double
norm_hdg(double h)
{
    h = fmod(h, 360.0);
    return (h < 0 ? h + 360 : h);
}

static double
seg_length(const bp_area_seg_t *s)
{
    if (s->turn)
        return (fabs(rel_hdg(s->hdg0, s->hdg1)) * D2R * s->r);
    return (hypot(s->x1 - s->x0, s->y1 - s->y0));
}

double
bp_area_length(const bp_area_seg_t *segs, int n)
{
    double len = 0;

    for (int i = 0; i < n; i++)
        len += seg_length(&segs[i]);
    return (len);
}

/*
 * On a turn: the centre lies off the start, to one side of the heading, and
 * the vehicle stays at the radius from it, square to its heading.
 */
static bp_area_pose_t
arc_at(const bp_area_seg_t *s, double side, double f)
{
    bp_area_pose_t p;
    double cx = s->x0 + side * cos(s->hdg0 * D2R) * s->r;
    double cy = s->y0 - side * sin(s->hdg0 * D2R) * s->r;
    double h = s->hdg0 + rel_hdg(s->hdg0, s->hdg1) * f;

    p.x = cx - side * cos(h * D2R) * s->r;
    p.y = cy + side * sin(h * D2R) * s->r;
    p.hdg = norm_hdg(h);
    return (p);
}

/* The pose `f` (0..1) of the way along the segment. */
static bp_area_pose_t
seg_at(const bp_area_seg_t *s, double f)
{
    bp_area_pose_t p, end_r, end_l;
    double side;

    if (!s->turn) {
        p.x = s->x0 + (s->x1 - s->x0) * f;
        p.y = s->y0 + (s->y1 - s->y0) * f;
        p.hdg = s->hdg0;
        return (p);
    }
    /*
     * The side the planner noted, unless the other one is the one that
     * reaches the segment's end (backing round a turn, the nose heading
     * and the side are easily read the other way).
     */
    side = s->right ? 1 : -1;
    end_r = arc_at(s, side, 1);
    end_l = arc_at(s, -side, 1);
    if (hypot(end_l.x - s->x1, end_l.y - s->y1) + 1e-6 <
        hypot(end_r.x - s->x1, end_r.y - s->y1))
        side = -side;
    return (arc_at(s, side, f));
}

int
bp_area_sample(const bp_area_seg_t *segs, int n, double step,
    bp_area_pose_t *out, int max)
{
    int count = 0;

    if (n <= 0 || max <= 0)
        return (0);
    if (step <= 0)
        step = BP_EXT_AREA_STEP_M;
    out[count++] = seg_at(&segs[0], 0);
    for (int i = 0; i < n; i++) {
        int parts = (int)ceil(seg_length(&segs[i]) / step);

        if (parts < 1)
            parts = 1;
        for (int k = 1; k <= parts; k++) {
            if (count >= max) {
                /* Cut short: the route's end matters most. */
                out[max - 1] = seg_at(&segs[n - 1], 1);
                return (max);
            }
            out[count++] = seg_at(&segs[i], (double)k / parts);
        }
    }
    return (count);
}
