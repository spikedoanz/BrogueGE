#ifndef BRH_BRIDGE_PROFILE_H
#define BRH_BRIDGE_PROFILE_H

#include <stdint.h>

#include "bridge-env.h"

#ifdef BROGUE_BRIDGE
extern int brh_profile_enabled_cache;
int brh_profile_enabled_from_env(void);
uint64_t brh_profile_zone_start(int zone);
void brh_profile_zone_end(int zone, uint64_t start_ns);
int brh_bridge_should_refresh_sidebar(void);
int brh_bridge_should_refresh_dungeon_cell(void);
int brh_bridge_allows_compact_simulation_shortcuts(void);
void brh_bridge_update_compact_cell(short x, short y);

static inline int brh_profile_enabled_inline(void) {
    if (brh_profile_enabled_cache < 0) {
        brh_profile_enabled_cache = brh_profile_enabled_from_env();
    }
    return brh_profile_enabled_cache;
}
#else
static inline int brh_profile_enabled_inline(void) {
    return 0;
}

static inline uint64_t brh_profile_zone_start(int zone) {
    (void) zone;
    return 0;
}

static inline void brh_profile_zone_end(int zone, uint64_t start_ns) {
    (void) zone;
    (void) start_ns;
}

static inline int brh_bridge_should_refresh_sidebar(void) {
    return 1;
}

static inline int brh_bridge_should_refresh_dungeon_cell(void) {
    return 1;
}

static inline int brh_bridge_allows_compact_simulation_shortcuts(void) {
    return 0;
}

static inline void brh_bridge_update_compact_cell(short x, short y) {
    (void) x;
    (void) y;
}
#endif

#define BRH_PROFILE_START(name, zone) \
    uint64_t name = brh_profile_enabled_inline() ? brh_profile_zone_start(zone) : 0
#define BRH_PROFILE_END(zone, name) \
    do { \
        if ((name) != 0) { \
            brh_profile_zone_end(zone, name); \
        } \
    } while (0)

#endif
