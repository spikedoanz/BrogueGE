#ifndef BRH_BRIDGE_ENV_H
#define BRH_BRIDGE_ENV_H

#include <stddef.h>
#include <stdint.h>

typedef struct brh_observation brh_observation;
typedef struct brh_env brh_env;

typedef struct brh_env_buffers {
    brh_observation *observations;
    long *actions;
    uint8_t *controls;
    uint8_t *shifts;
    float *rewards;
    float *terminals;
} brh_env_buffers;

uint32_t brh_abi_version(void);
size_t brh_observation_size(void);
int brh_screen_cols(void);
int brh_screen_rows(void);
int brh_map_cols(void);
int brh_map_rows(void);
int brh_inventory_size(void);
int brh_inventory_str_length(void);
void brh_set_data_dir(const char *path);
const char *brh_last_error(void);

int brh_reset(uint64_t seed, brh_observation *out);
int brh_step(long key, int control, int shift, brh_observation *out);
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

#endif
