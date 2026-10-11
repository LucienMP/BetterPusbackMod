/*
 * External interface, version 14: the tug's parking place and road from
 * another plugin. Unit tested on its own (tests/ext_api_tugpath_test.c).
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ext_api_tugpath.h"

#define EARTH_RADIUS_M 6371008.8
#define D2R (3.14159265358979323846 / 180.0)

static void
set_reason(char *reason, size_t len, const char *what, int line)
{
    if (reason == NULL || len == 0)
        return;
    if (line > 0)
        (void)snprintf(reason, len, "line %d: %s", line, what);
    else
        (void)snprintf(reason, len, "%s", what);
}

/* Reads one number; true when the whole word is a finite number. */
static bool
read_number(const char *word, double *out)
{
    char *end;
    double v;

    if (word == NULL)
        return (false);
    v = strtod(word, &end);
    if (end == word || *end != '\0' || !isfinite(v))
        return (false);
    *out = v;
    return (true);
}

static bool
read_geo(const char *lat_word, const char *lon_word, bp_ext_geo_t *geo)
{
    double lat, lon;

    if (!read_number(lat_word, &lat) || !read_number(lon_word, &lon) ||
        lat < -90 || lat > 90 || lon < -180 || lon > 180)
        return (false);
    geo->lat = lat;
    geo->lon = lon;
    return (true);
}

/* The rest of `line` after its first `skip` words, trimmed. */
static void
rest_of_line(const char *line, int skip, char *out, size_t len)
{
    const char *p = line;

    for (int i = 0; i < skip; i++) {
        p += strspn(p, " \t");
        p += strcspn(p, " \t");
    }
    p += strspn(p, " \t");
    (void)snprintf(out, len, "%s", p);
    for (size_t n = strlen(out); n > 0 && (out[n - 1] == ' ' ||
        out[n - 1] == '\t'); n--)
        out[n - 1] = '\0';
}

bool
bp_ext_tugpath_parse(const char *text, bp_ext_tugpath_t *path, char *reason,
    size_t reason_len)
{
    char line[256], whole[256];
    const char *p = text;
    int lineno = 0;
    bool header = false, origin = false;

    set_reason(reason, reason_len, "", 0);
    memset(path, 0, sizeof (*path));
    if (text == NULL)
        return (false);
    while (*p != '\0') {
        size_t len = strcspn(p, "\r\n");
        char *words[5] = { NULL };
        int nwords = 0;
        char *save = line;

        lineno++;
        if (len >= sizeof (line)) {
            set_reason(reason, reason_len, "line too long", lineno);
            return (false);
        }
        memcpy(line, p, len);
        line[len] = '\0';
        memcpy(whole, line, len + 1);
        p += len;
        while (*p == '\r' || *p == '\n')
            p++;

        /* Split on blanks, at most five words counted. */
        while (*save != '\0') {
            save += strspn(save, " \t");
            if (*save == '\0')
                break;
            if (nwords < 5)
                words[nwords] = save;
            nwords++;
            save += strcspn(save, " \t");
            if (*save != '\0')
                *save++ = '\0';
        }
        if (nwords == 0 || words[0][0] == '#')
            continue;

        if (!header) {
            double v;

            if (strcmp(words[0], "BPTUG") != 0 || nwords != 2 ||
                !read_number(words[1], &v)) {
                set_reason(reason, reason_len,
                    "the path must start with \"BPTUG 1\"", lineno);
                return (false);
            }
            if (v != BP_EXT_TUGPATH_FORMAT) {
                set_reason(reason, reason_len,
                    "unsupported tug path format version", lineno);
                return (false);
            }
            header = true;
            continue;
        }
        if (strcmp(words[0], "O") == 0) {
            double hdg;

            if (origin) {
                set_reason(reason, reason_len, "a second origin", lineno);
                return (false);
            }
            if (nwords < 4 || !read_geo(words[1], words[2], &path->origin) ||
                !read_number(words[3], &hdg) || hdg < 0 || hdg > 360) {
                set_reason(reason, reason_len, "expected \"O <latitude> "
                    "<longitude> <heading> [name]\"", lineno);
                return (false);
            }
            path->origin_hdg = (hdg >= 360 ? 0 : hdg);
            rest_of_line(whole, 4, path->name, sizeof (path->name));
            origin = true;
        } else if (strcmp(words[0], "W") == 0) {
            if (nwords != 3) {
                set_reason(reason, reason_len,
                    "expected \"W <latitude> <longitude>\"", lineno);
                return (false);
            }
            if (path->n >= BP_EXT_TUGPATH_MAX_POINTS) {
                set_reason(reason, reason_len, "too many road points",
                    lineno);
                return (false);
            }
            if (!read_geo(words[1], words[2], &path->way[path->n])) {
                set_reason(reason, reason_len,
                    "expected \"W <latitude> <longitude>\"", lineno);
                return (false);
            }
            path->n++;
        } else {
            set_reason(reason, reason_len, "unknown line; expected O or W",
                lineno);
            return (false);
        }
    }
    if (!header)
        return (false);                     /* nothing written: no reason */
    if (!origin) {
        set_reason(reason, reason_len, "the path has no origin (O line)", 0);
        return (false);
    }
    return (true);
}

double
bp_ext_geo_distance(bp_ext_geo_t a, bp_ext_geo_t b)
{
    double dlat = (b.lat - a.lat) * D2R, dlon = (b.lon - a.lon) * D2R;
    double h = sin(dlat / 2) * sin(dlat / 2) + cos(a.lat * D2R) *
        cos(b.lat * D2R) * sin(dlon / 2) * sin(dlon / 2);

    return (2 * EARTH_RADIUS_M * atan2(sqrt(h), sqrt(1 - h)));
}

double
bp_ext_geo_bearing(bp_ext_geo_t a, bp_ext_geo_t b)
{
    double dlon = (b.lon - a.lon) * D2R;
    double y = sin(dlon) * cos(b.lat * D2R);
    double x = cos(a.lat * D2R) * sin(b.lat * D2R) -
        sin(a.lat * D2R) * cos(b.lat * D2R) * cos(dlon);
    double h = atan2(y, x) / D2R;

    if (h < 0)
        h += 360;
    if (h >= 360)
        h -= 360;
    return (h);
}

bool
bp_ext_tugpath_usable(const bp_ext_tugpath_t *path, bp_ext_geo_t acf,
    char *reason, size_t reason_len)
{
    char what[96];

    if (bp_ext_geo_distance(path->origin, acf) > BP_EXT_TUGPATH_MAX_DIST_M) {
        (void)snprintf(what, sizeof (what), "the origin is more than %.0f m "
            "from the aircraft", BP_EXT_TUGPATH_MAX_DIST_M);
        set_reason(reason, reason_len, what, 0);
        return (false);
    }
    for (int i = 0; i < path->n; i++) {
        if (bp_ext_geo_distance(path->way[i], acf) >
            BP_EXT_TUGPATH_MAX_DIST_M) {
            (void)snprintf(what, sizeof (what), "road point %d is more than "
                "%.0f m from the aircraft", i + 1, BP_EXT_TUGPATH_MAX_DIST_M);
            set_reason(reason, reason_len, what, 0);
            return (false);
        }
    }
    set_reason(reason, reason_len, "", 0);
    return (true);
}

double
bp_ext_tugpath_length(const bp_ext_tugpath_t *path, bp_ext_geo_t to)
{
    bp_ext_geo_t at = path->origin;
    double len = 0;

    for (int i = 0; i < path->n; i++) {
        len += bp_ext_geo_distance(at, path->way[i]);
        at = path->way[i];
    }
    return (len + bp_ext_geo_distance(at, to));
}

double
bp_ext_tugpath_drive_time(double length, double speed, double slack)
{
    /* Corners and slowing down: the tug averages half its top speed. */
    if (speed < 1.0)
        speed = 1.0;
    if (length < 0)
        length = 0;
    return (slack + 2.0 * length / speed);
}
