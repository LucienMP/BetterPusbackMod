/*
 * External interface, version 12: the tug's state and position, estimates
 * and milestones (README-EXTERNAL-API.md). See ext_api_tug.h.
 */

#ifndef _EXT_TUG_H_
#define _EXT_TUG_H_

#include <acfutils/types.h>

#ifdef __cplusplus
extern "C" {
#endif

void ext_tug_init(void);
void ext_tug_fini(void);
/* Refreshes the published values; true when bp/state_seq should move. */
bool_t ext_tug_refresh(void);

#ifdef __cplusplus
}
#endif

#endif /* _EXT_TUG_H_ */
