/*
 * External interface: reading and writing push routes as text. Unit tested on
 * its own (tests/ext_api_route_test.c).
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ext_api_route.h"

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

double
bp_ext_route_bearing(double lat1, double lon1, double lat2, double lon2)
{
    /* Flat over the short distances of an airport: east scaled by latitude. */
    const double d2r = 3.14159265358979323846 / 180.0;
    double de = (lon2 - lon1) * cos((lat1 + lat2) / 2.0 * d2r);
    double dn = lat2 - lat1;
    double h = atan2(de, dn) / d2r;

    if (h < 0)
        h += 360;
    if (h >= 360)
        h -= 360;
    return (h);
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

int
bp_ext_route_parse(const char *text, bp_ext_pose_t *poses, int max,
    char *reason, size_t reason_len)
{
    char line[256], whole[256];
    const char *p = text;
    int n = 0, lineno = 0, version = 0;
    bool header = false;

    if (text == NULL) {
        set_reason(reason, reason_len, "no route text", 0);
        return (-1);
    }
    while (*p != '\0') {
        size_t len = strcspn(p, "\r\n");
        char *words[6] = { NULL };
        int nwords = 0;

        lineno++;
        if (len >= sizeof (line)) {
            set_reason(reason, reason_len, "line too long", lineno);
            return (-1);
        }
        memcpy(line, p, len);
        line[len] = '\0';
        memcpy(whole, line, len + 1);
        p += len;
        while (*p == '\r' || *p == '\n')
            p++;

        for (char *w = strtok(line, " \t"); w != NULL && nwords < 6;
            w = strtok(NULL, " \t"))
            words[nwords++] = w;
        if (nwords == 0 || words[0][0] == '#')
            continue;

        if (!header) {
            double v;
            if (strcmp(words[0], "BPROUTE") != 0 || nwords != 2 ||
                !read_number(words[1], &v)) {
                set_reason(reason, reason_len,
                    "the route must start with \"BPROUTE 1\" or \"BPROUTE 2\"",
                    lineno);
                return (-1);
            }
            if (v != BP_EXT_ROUTE_FORMAT && v != BP_EXT_ROUTE_FORMAT_HOLDS) {
                set_reason(reason, reason_len,
                    "unsupported route format version", lineno);
                return (-1);
            }
            version = (int)v;
            header = true;
            continue;
        }

        bp_ext_pose_kind_t kind;
        if (strcmp(words[0], "P") == 0) {
            kind = BP_EXT_POSE_POSITION;
            if (nwords < 4 || nwords > 5) {
                set_reason(reason, reason_len,
                    "expected \"P <latitude> <longitude> <heading>\"", lineno);
                return (-1);
            }
        } else if (version >= BP_EXT_ROUTE_FORMAT_HOLDS &&
            strcmp(words[0], "W") == 0) {
            kind = BP_EXT_POSE_WAYPOINT;
            if (nwords != 3) {
                set_reason(reason, reason_len,
                    "expected \"W <latitude> <longitude>\"", lineno);
                return (-1);
            }
        } else if (version >= BP_EXT_ROUTE_FORMAT_HOLDS &&
            strcmp(words[0], "H") == 0) {
            kind = BP_EXT_POSE_HOLD;
            if (nwords < 4) {
                set_reason(reason, reason_len,
                    "expected \"H <latitude> <longitude> <why the tug "
                    "holds>\"", lineno);
                return (-1);
            }
        } else {
            set_reason(reason, reason_len, version >= BP_EXT_ROUTE_FORMAT_HOLDS ?
                "expected a P, W or H line" :
                "expected \"P <latitude> <longitude> <heading>\"", lineno);
            return (-1);
        }
        if (n >= max) {
            set_reason(reason, reason_len, "too many positions", lineno);
            return (-1);
        }
        memset(&poses[n], 0, sizeof (poses[n]));
        poses[n].kind = kind;
        if (!read_number(words[1], &poses[n].lat) ||
            !read_number(words[2], &poses[n].lon) ||
            (kind == BP_EXT_POSE_POSITION &&
            !read_number(words[3], &poses[n].hdg))) {
            set_reason(reason, reason_len, "not a number", lineno);
            return (-1);
        }
        if (fabs(poses[n].lat) > 90 || fabs(poses[n].lon) > 180) {
            set_reason(reason, reason_len,
                "latitude or longitude out of range", lineno);
            return (-1);
        }
        if (kind == BP_EXT_POSE_POSITION) {
            if (poses[n].hdg < 0 || poses[n].hdg > 360) {
                set_reason(reason, reason_len,
                    "heading must be 0 to 360 degrees", lineno);
                return (-1);
            }
            if (poses[n].hdg == 360)
                poses[n].hdg = 0;
        }
        if (kind == BP_EXT_POSE_HOLD)
            rest_of_line(whole, 3, poses[n].hold_text,
                sizeof (poses[n].hold_text));
        n++;
    }
    if (!header) {
        set_reason(reason, reason_len,
            "the route must start with \"BPROUTE 1\" or \"BPROUTE 2\"", 0);
        return (-1);
    }
    if (n == 0) {
        set_reason(reason, reason_len, "the route has no positions", 0);
        return (-1);
    }
    /*
     * Waypoints and hold points face the next position; the last one keeps
     * the heading it arrives with (from the one before it).
     */
    for (int i = 0; i < n; i++) {
        if (poses[i].kind == BP_EXT_POSE_POSITION)
            continue;
        if (i + 1 < n) {
            poses[i].hdg = bp_ext_route_bearing(poses[i].lat, poses[i].lon,
                poses[i + 1].lat, poses[i + 1].lon);
        } else if (i > 0) {
            poses[i].hdg = bp_ext_route_bearing(poses[i - 1].lat,
                poses[i - 1].lon, poses[i].lat, poses[i].lon);
        } else {
            set_reason(reason, reason_len, "a route of one waypoint needs a "
                "P line with a heading", 0);
            return (-1);
        }
    }
    return (n);
}

bool
bp_ext_route_format(const bp_ext_pose_t *poses, int n, char *text,
    size_t text_len)
{
    size_t used;
    int w, version = BP_EXT_ROUTE_FORMAT;

    if (text == NULL || text_len == 0)
        return (false);
    for (int i = 0; i < n; i++) {
        if (poses[i].kind == BP_EXT_POSE_HOLD)
            version = BP_EXT_ROUTE_FORMAT_HOLDS;
    }
    w = snprintf(text, text_len, "BPROUTE %d\n", version);
    if (w < 0 || (size_t)w >= text_len)
        return (false);
    used = (size_t)w;
    for (int i = 0; i < n; i++) {
        if (poses[i].kind == BP_EXT_POSE_HOLD) {
            w = snprintf(text + used, text_len - used, "H %.8f %.8f %s\n",
                poses[i].lat, poses[i].lon, poses[i].hold_text[0] != '\0' ?
                poses[i].hold_text : "hold");
        } else {
            w = snprintf(text + used, text_len - used, "P %.8f %.8f %.2f %s\n",
                poses[i].lat, poses[i].lon, poses[i].hdg,
                poses[i].backward ? "push" : "tow");
        }
        if (w < 0 || (size_t)w >= text_len - used) {
            text[0] = '\0';
            return (false);
        }
        used += (size_t)w;
    }
    return (true);
}

const char *
bp_ext_route_change_refused(const bp_ext_route_gate_t *gate)
{
    if (!gate->ready)
        return ("BetterPushback cannot work with this aircraft");
    if (gate->slave_mode)
        return ("the route comes from the other cockpit (shared cockpit)");
    if (gate->planner_open)
        return ("the planner is open");
    if (gate->manual_push)
        return ("a manual push is in progress");
    if (!gate->started || gate->awaiting_plan || gate->can_replan)
        return (NULL);
    return ("the route cannot change now: the tug is connecting or the push "
        "is under way");
}
