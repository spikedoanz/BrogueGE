#ifndef BRH_BRIDGE_ENV_H
#define BRH_BRIDGE_ENV_H

#include <stddef.h>
#include <stdint.h>

typedef struct brh_observation brh_observation;
typedef struct brh_env brh_env;

#define BRH_PUBLIC_SCREEN_COLS 100
#define BRH_PUBLIC_SCREEN_ROWS 34
#define BRH_PUBLIC_SCREEN_CELLS (BRH_PUBLIC_SCREEN_COLS * BRH_PUBLIC_SCREEN_ROWS)
#define BRH_PUBLIC_COMPACT_COLS 79
#define BRH_PUBLIC_COMPACT_ROWS 29
#define BRH_PUBLIC_COMPACT_CELLS (BRH_PUBLIC_COMPACT_COLS * BRH_PUBLIC_COMPACT_ROWS)
#define BRH_PUBLIC_BLSTATS_SIZE 21
#define BRH_PUBLIC_PROGRAM_STATE_SIZE 8
#define BRH_PROFILE_ZONE_COUNT 37

enum brh_profile_zone_id {
    BRH_ZONE_EXECUTE_EVENT = 0,
    BRH_ZONE_EXECUTE_KEYSTROKE,
    BRH_ZONE_DIRECTION_ACTION,
    BRH_ZONE_PLAYER_TURN_ENDED,
    BRH_ZONE_AUTO_REST,
    BRH_ZONE_MANUAL_SEARCH,
    BRH_ZONE_UPDATE_ENVIRONMENT,
    BRH_ZONE_MONSTERS_TURN,
    BRH_ZONE_UPDATE_VISION,
    BRH_ZONE_REFRESH_DUNGEON_CELL,
    BRH_ZONE_REFRESH_SIDEBAR,
    BRH_ZONE_COMMIT_DRAWS,
    BRH_ZONE_REFRESH_SCREEN,
    BRH_ZONE_DISPLAY_LEVEL,
    BRH_ZONE_MOVE_CURSOR,
    BRH_ZONE_INPUT_LOOP_PREP,
    BRH_ZONE_VISION_DEMOTE_RESET,
    BRH_ZONE_VISION_FOV,
    BRH_ZONE_VISION_SENSES,
    BRH_ZONE_VISION_LIGHTING,
    BRH_ZONE_VISION_DISPLAY,
    BRH_ZONE_LIGHTING_RESET,
    BRH_ZONE_LIGHTING_TILE_GLOW,
    BRH_ZONE_LIGHTING_CREATURES,
    BRH_ZONE_LIGHTING_DISPLAY_DETAIL,
    BRH_ZONE_LIGHTING_MINER,
    BRH_ZONE_ENV_FALL,
    BRH_ZONE_ENV_RESET_FIRE,
    BRH_ZONE_ENV_GAS,
    BRH_ZONE_ENV_PROMOTIONS,
    BRH_ZONE_ENV_BOOKKEEPING,
    BRH_ZONE_ENV_FIRE,
    BRH_ZONE_ENV_FLOOR_ITEMS,
    BRH_ZONE_TURN_UPDATE_SCENT,
    BRH_ZONE_TURN_SCHEDULE_LOOP,
    BRH_ZONE_TURN_POST_VISION,
    BRH_ZONE_TURN_END_BOOKKEEPING,
};

typedef struct brh_compact_observation {
    uint8_t chars[BRH_PUBLIC_COMPACT_CELLS];
    int32_t blstats[BRH_PUBLIC_BLSTATS_SIZE];
    int32_t program_state[BRH_PUBLIC_PROGRAM_STATE_SIZE];
} brh_compact_observation;

typedef struct brh_env_buffers {
    brh_observation *observations;
    long *actions;
    uint8_t *controls;
    uint8_t *shifts;
    float *rewards;
    float *terminals;
} brh_env_buffers;

typedef struct brh_profile {
    uint64_t reset_calls;
    uint64_t step_calls;
    uint64_t resume_calls;
    uint64_t input_yields;
    uint64_t observation_fills;
    uint64_t compact_observation_fills;
    uint64_t program_fills;
    uint64_t plot_calls;
    uint64_t pause_calls;
    uint64_t modifier_calls;
    uint64_t notify_calls;
    uint64_t reset_ns;
    uint64_t step_ns;
    uint64_t resume_ns;
    uint64_t observation_fill_ns;
    uint64_t compact_observation_fill_ns;
    uint64_t program_fill_ns;
    uint64_t zone_calls[BRH_PROFILE_ZONE_COUNT];
    uint64_t zone_ns[BRH_PROFILE_ZONE_COUNT];
} brh_profile;

uint32_t brh_abi_version(void);
size_t brh_observation_size(void);
size_t brh_compact_observation_size(void);
int brh_screen_cols(void);
int brh_screen_rows(void);
int brh_map_cols(void);
int brh_map_rows(void);
int brh_inventory_size(void);
int brh_inventory_str_length(void);
void brh_set_data_dir(const char *path);
const char *brh_last_error(void);

int brh_reset(uint64_t seed, brh_observation *out);
int brh_reset_compact(uint64_t seed, brh_compact_observation *out);
int brh_step(long key, int control, int shift, brh_observation *out);
int brh_step_compact(long key, int control, int shift, brh_compact_observation *out);
int brh_step_no_observation(long key, int control, int shift);
void brh_close(void);
void brh_mark_invalid_key(void);

brh_env *brh_env_create(const brh_env_buffers *buffers);
int brh_env_reset(brh_env *env, uint64_t seed);
int brh_env_step(brh_env *env, long key, int control, int shift);
int brh_env_step_no_observation(brh_env *env, long key, int control, int shift);
int brh_env_step_from_buffers(brh_env *env);
int brh_env_num_agents(const brh_env *env);
void brh_env_close(brh_env *env);
void brh_profile_reset(void);
void brh_profile_read(brh_profile *out);

#endif
