/*
 * External interface, version 15: push-back routes from a scenery's WED
 * project. Unit tested on its own (tests/scenery_route_test.c).
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ext_api_tugpath.h"
#include "scenery_route.h"

static bool
valid_hdg(double hdg)
{
    return (isfinite(hdg) && hdg >= 0 && hdg <= 360);
}

static double
hdg_diff(double a, double b)
{
    double d = fmod(fabs(a - b), 360.0);

    return (d > 180 ? 360 - d : d);
}

/*
 * One "seg" line after its keyword:
 *   type slat slon shdg elat elon ehdg backward user_placed        (straight)
 *   type slat slon shdg elat elon ehdg backward radius right user_placed (turn)
 */
static bool
read_seg(const char *line, double start[3], bp_scenery_seg_end_t *end)
{
    unsigned type, backward, user_placed, right;
    double radius;
    int used = 0;

    if (sscanf(line, "%u %lf %lf %lf %lf %lf %lf %u%n", &type, &start[0],
        &start[1], &start[2], &end->lat, &end->lon, &end->hdg, &backward,
        &used) != 8)
        return (false);
    line += used;
    if (type == 0) {
        if (sscanf(line, "%u", &user_placed) != 1)
            return (false);
    } else if (type == 1) {
        if (sscanf(line, "%lf %u %u", &radius, &right, &user_placed) != 3)
            return (false);
    } else {
        return (false);
    }
    if (fabs(start[0]) > 90 || fabs(start[1]) > 180 || fabs(end->lat) > 90 ||
        fabs(end->lon) > 180 || !valid_hdg(start[2]) || !valid_hdg(end->hdg))
        return (false);
    end->backward = (backward != 0);
    end->user_placed = (user_placed != 0);
    return (true);
}

int
bp_scenery_routes_parse(const char *text, bp_scenery_route_t *routes, int max)
{
    const char *p = text;
    int n = 0;
    bool in_route = false, broken = false;

    if (text == NULL || max <= 0)
        return (0);
    while (*p != '\0') {
        size_t len = strcspn(p, "\r\n");
        char line[512];
        const char *w;

        if (len >= sizeof (line)) {
            broken = true;          /* nothing this file writes */
            len = sizeof (line) - 1;
        }
        memcpy(line, p, len);
        line[len] = '\0';
        p += strcspn(p, "\r\n");
        while (*p == '\r' || *p == '\n')
            p++;

        w = line + strspn(line, " \t");
        if (*w == '\0' || *w == '#')
            continue;
        if (strncmp(w, "route", 5) == 0 && (w[5] == '\0' || w[5] == ' ' ||
            w[5] == '\t')) {
            /* The route before is kept when it is whole. */
            if (in_route && !broken && routes[n].n > 0)
                n++;
            if (n >= max)
                return (n);
            memset(&routes[n], 0, sizeof (routes[n]));
            in_route = true;
            broken = false;
        } else if (strncmp(w, "seg", 3) == 0 && (w[3] == ' ' ||
            w[3] == '\t')) {
            double start[3];
            bp_scenery_seg_end_t end;
            bp_scenery_route_t *r = &routes[n];

            if (!in_route || broken)
                continue;
            if (!read_seg(w + 3, start, &end) ||
                r->n >= BP_SCENERY_ROUTE_MAX_SEGS) {
                broken = true;
                continue;
            }
            if (r->n == 0) {
                r->lat = start[0];
                r->lon = start[1];
                r->hdg = start[2];
            }
            r->ends[r->n++] = end;
        } else {
            broken = true;
        }
    }
    if (in_route && !broken && n < max && routes[n].n > 0)
        n++;
    return (n);
}

bool
bp_scenery_route_matches(bp_scenery_route_t *route, double lat, double lon,
    double hdg)
{
    bp_ext_geo_t a = { route->lat, route->lon }, b = { lat, lon };

    route->distance = bp_ext_geo_distance(a, b);
    return (route->distance <= BP_SCENERY_ROUTE_MAX_DIST_M &&
        hdg_diff(route->hdg, hdg) <= BP_SCENERY_ROUTE_MAX_HDG_DEG);
}

static int
by_distance(const void *a, const void *b)
{
    const bp_scenery_route_t *ra = a, *rb = b;

    return (ra->distance < rb->distance ? -1 :
        ra->distance > rb->distance ? 1 : 0);
}

void
bp_scenery_routes_sort(bp_scenery_route_t *routes, int n)
{
    if (n > 1)
        qsort(routes, (size_t)n, sizeof (*routes), by_distance);
}

int
bp_scenery_route_poses(const bp_scenery_route_t *route, bp_ext_pose_t *poses,
    int max)
{
    bool marked = false;
    int n = 0;

    for (int i = 0; i < route->n; i++)
        marked = marked || route->ends[i].user_placed;
    for (int i = 0; i < route->n && n < max; i++) {
        const bp_scenery_seg_end_t *e = &route->ends[i];
        bool last = (i + 1 == route->n);
        bool turn_about = !last && route->ends[i + 1].backward != e->backward;

        if (!(last || (marked ? e->user_placed : turn_about)))
            continue;
        memset(&poses[n], 0, sizeof (poses[n]));
        poses[n].lat = e->lat;
        poses[n].lon = e->lon;
        poses[n].hdg = (e->hdg >= 360 ? 0 : e->hdg);
        poses[n].backward = e->backward;
        poses[n].kind = BP_EXT_POSE_POSITION;
        n++;
    }
    return (n);
}
