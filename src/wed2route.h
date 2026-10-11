/*
 * CDDL HEADER START
 *
 * This file and its contents are supplied under the terms of the
 * Common Development and Distribution License ("CDDL"), version 1.0.
 * You may only use this file in accordance with the terms of version
 * 1.0 of the CDDL.
 *
 * A full copy of the text of the CDDL should have accompanied this
 * source.  A copy of the CDDL is also available via the Internet at
 * http://www.illumos.org/license/CDDL.
 *
 * CDDL HEADER END
*/
/*
 * Copyright 2017 Saso Kiselkov. All rights reserved.
 */

#ifndef    _WED2ROUTE_H_
#define    _WED2ROUTE_H_

#include <acfutils/avl.h>
#include <acfutils/types.h>

#include "scenery_route.h"

#ifdef    __cplusplus
extern "C" {
#endif

/*
 * Converts each scenery's earth.wed.xml into BetterPushback_routes.dat beside
 * it, when the project is newer than the file or `force` is set.
 */
void xlate_wedroutes(bool_t force);

/*
 * The converted scenery routes that start where the aircraft stands (within
 * BP_SCENERY_ROUTE_MAX_DIST_M and BP_SCENERY_ROUTE_MAX_HDG_DEG), nearest
 * first. Returns how many were put in `out` (at most `max`).
 */
int scenery_routes_near(double lat, double lon, double hdg,
    bp_scenery_route_t *out, int max);

#ifdef    __cplusplus
}
#endif

#endif    /* _WED2ROUTE_H_ */
