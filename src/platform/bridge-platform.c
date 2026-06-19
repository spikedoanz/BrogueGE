#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <pthread.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ucontext.h>

#include "platform.h"
#include "bridge-env.h"
#include "bridge-profile.h"
#include "GlobalsBase.h"
#include "Globals.h"

#define BRH_ABI_VERSION 11
#define BRH_OBS_CELLS (COLS * ROWS)
#define BRH_MAP_CELLS (DCOLS * DROWS)
#define BRH_MAP_LAYER_CELLS (DCOLS * DROWS * NUMBER_TERRAIN_LAYERS)
#define BRH_COLOR_CELLS (COLS * ROWS * 3)
#define BRH_MAP_COLOR_CELLS (DCOLS * DROWS * 3)
#define BRH_BLSTATS_SIZE 21
#define BRH_MESSAGE_SIZE 256
#define BRH_PROGRAM_STATE_SIZE 8
#define BRH_INVENTORY_SIZE 26
#define BRH_INVENTORY_STR_LENGTH 80
#define BRH_INVENTORY_STR_CELLS (BRH_INVENTORY_SIZE * BRH_INVENTORY_STR_LENGTH)
#define BRH_THREAD_STACK_SIZE (16 * 1024 * 1024)
#define BRH_STEP_OK 0
#define BRH_STEP_INVALID_KEY 1
#define BRH_CAPTURE_NONE 0
#define BRH_CAPTURE_FULL 1
#define BRH_CAPTURE_COMPACT 2
#define BRH_UNKNOWN_SHORT INT16_MIN
#define BRH_OBS_CELL_FLAGS (DISCOVERED | VISIBLE | HAS_PLAYER | HAS_MONSTER | HAS_ITEM \
                            | IN_FIELD_OF_VIEW | WAS_VISIBLE | HAS_STAIRS | MAGIC_MAPPED \
                            | ITEM_DETECTED | CLAIRVOYANT_VISIBLE | WAS_CLAIRVOYANT_VISIBLE \
                            | TELEPATHIC_VISIBLE | WAS_TELEPATHIC_VISIBLE | IS_IN_PATH \
                            | KNOWN_TO_BE_TRAP_FREE)
#define BRH_OBS_ITEM_FLAGS (ITEM_IDENTIFIED | ITEM_EQUIPPED | ITEM_PROTECTED \
                            | ITEM_RUNIC_HINTED | ITEM_RUNIC_IDENTIFIED \
                            | ITEM_CAN_BE_IDENTIFIED | ITEM_MAGIC_DETECTED \
                            | ITEM_MAX_CHARGES_KNOWN | ITEM_IS_KEY | ITEM_PLAYER_AVOIDS)

struct brh_observation {
    int16_t glyphs[BRH_OBS_CELLS];
    uint32_t chars[BRH_OBS_CELLS];
    uint8_t colors_fg[BRH_COLOR_CELLS];
    uint8_t colors_bg[BRH_COLOR_CELLS];
    uint8_t specials[BRH_OBS_CELLS];
    uint16_t map_layers[BRH_MAP_LAYER_CELLS];
    uint64_t map_flags[BRH_MAP_CELLS];
    uint16_t map_volume[BRH_MAP_CELLS];
    uint8_t map_machine[BRH_MAP_CELLS];
    int16_t map_light[BRH_MAP_COLOR_CELLS];
    uint8_t map_has_item[BRH_MAP_CELLS];
    uint16_t map_item_category[BRH_MAP_CELLS];
    int16_t map_item_kind[BRH_MAP_CELLS];
    int16_t map_item_quantity[BRH_MAP_CELLS];
    uint64_t map_item_flags[BRH_MAP_CELLS];
    uint8_t map_has_monster[BRH_MAP_CELLS];
    int16_t map_monster_kind[BRH_MAP_CELLS];
    int16_t map_monster_hp[BRH_MAP_CELLS];
    int16_t map_monster_state[BRH_MAP_CELLS];
    uint8_t inventory_present[BRH_INVENTORY_SIZE];
    uint8_t inventory_letters[BRH_INVENTORY_SIZE];
    uint8_t inventory_strs[BRH_INVENTORY_STR_CELLS];
    uint16_t inventory_category[BRH_INVENTORY_SIZE];
    int16_t inventory_kind[BRH_INVENTORY_SIZE];
    int16_t inventory_quantity[BRH_INVENTORY_SIZE];
    uint64_t inventory_flags[BRH_INVENTORY_SIZE];
    int16_t inventory_enchant1[BRH_INVENTORY_SIZE];
    int16_t inventory_enchant2[BRH_INVENTORY_SIZE];
    int16_t inventory_charges[BRH_INVENTORY_SIZE];
    int64_t blstats[BRH_BLSTATS_SIZE];
    uint8_t message[BRH_MESSAGE_SIZE];
    uint64_t program_state[BRH_PROGRAM_STATE_SIZE];
};

struct brh_env {
    brh_observation *observations;
    long *actions;
    uint8_t *controls;
    uint8_t *shifts;
    float *rewards;
    float *terminals;
    int num_agents;
    unsigned int rng;
    boolean running;
};

int brh_reset(uint64_t seed, brh_observation *out);
int brh_reset_compact(uint64_t seed, brh_compact_observation *out);
int brh_step(long key, int control, int shift, brh_observation *out);
int brh_step_compact(long key, int control, int shift, brh_compact_observation *out);
int brh_step_no_observation(long key, int control, int shift);
uint32_t brh_abi_version(void);
size_t brh_observation_size(void);
size_t brh_compact_observation_size(void);
int brh_screen_cols(void);
int brh_screen_rows(void);
int brh_map_cols(void);
int brh_map_rows(void);
int brh_inventory_size(void);
int brh_inventory_str_length(void);
void brh_close(void);
void brh_set_data_dir(const char *path);
const char *brh_last_error(void);
void brh_mark_invalid_key(void);
brh_env *brh_env_create(const brh_env_buffers *buffers);
int brh_env_reset(brh_env *env, uint64_t seed);
int brh_env_step(brh_env *env, long key, int control, int shift);
int brh_env_step_no_observation(brh_env *env, long key, int control, int shift);
int brh_env_step_from_buffers(brh_env *env);
int brh_env_num_agents(const brh_env *env);
void brh_env_close(brh_env *env);

struct brogueConsole currentConsole;
char dataDirectory[BROGUE_FILENAME_MAX] = STRINGIFY(DATADIR);
boolean serverMode = false;
boolean nonInteractivePlayback = false;
boolean hasGraphics = false;
enum graphicsModes graphicsMode = TEXT_GRAPHICS;

static ucontext_t bridgeCallerContext;
static ucontext_t bridgeGameContext;
static void *bridgeGameStack = NULL;
static boolean bridgeThreadStarted = false;
static boolean bridgeWaitingForAction = false;
static boolean bridgeActionReady = false;
static boolean bridgeCloseRequested = false;
static boolean bridgeExited = false;
static boolean bridgeActiveControl = false;
static boolean bridgeActiveShift = false;
static int bridgeExitCode = 0;
static int bridgeLastStepStatus = BRH_STEP_OK;
static int bridgeCaptureMode = BRH_CAPTURE_FULL;
static char bridgeLastError[256] = "";
static rogueEvent bridgePendingEvent = {0};
static brh_observation bridgeLastObservation = {0};
static brh_compact_observation bridgeLastCompactObservation = {0};
static uint8_t bridgeCompactMapChars[BRH_PUBLIC_COMPACT_CELLS] = {0};
static boolean bridgeCompactMapDirty = true;
static int64_t bridgeEndScore = 0;
static boolean bridgeEndWon = false;
static brh_profile bridgeProfile = {0};
static pthread_mutex_t bridgeEnvMutex = PTHREAD_MUTEX_INITIALIZER;
static brh_env *bridgeActiveEnv = NULL;
int brh_profile_enabled_cache = -1;

static int bridge_profile_enabled(void);
static uint64_t bridge_now_ns(void);
static void bridge_coroutine_main(void);
static void bridge_gameLoop(void);
static boolean bridge_pauseForMilliseconds(short milliseconds, PauseBehavior behavior);
static void bridge_nextKeyOrMouseEvent(rogueEvent *returnEvent,
                                       boolean textInput,
                                       boolean colorsDance);
static void bridge_plotChar(enum displayGlyph inputChar,
                            short xLoc,
                            short yLoc,
                            short foreRed,
                            short foreGreen,
                            short foreBlue,
                            short backRed,
                            short backGreen,
                            short backBlue);
static void bridge_remap(const char *input_name, const char *output_name);
static boolean bridge_modifierHeld(int modifier);
static void bridge_notifyEvent(short eventId,
                               int data1,
                               int data2,
                               const char *str1,
                               const char *str2);
static enum graphicsModes bridge_setGraphicsMode(enum graphicsModes mode);
static void bridge_fill_observation(brh_observation *out);
static void bridge_fill_compact_observation(brh_compact_observation *out);
static void bridge_fill_unknown_semantics(brh_observation *out);
static void bridge_fill_program_state(brh_observation *out);
static void bridge_fill_compact_program_state(brh_compact_observation *out);
static void bridge_fill_map_semantics(brh_observation *out);
static void bridge_fill_item_semantics(brh_observation *out);
static void bridge_fill_monster_semantics(brh_observation *out);
static void bridge_fill_inventory_semantics(brh_observation *out);
static void bridge_set_error(const char *message);
static void bridge_set_error_locked(const char *message);
static int bridge_reset_internal(uint64_t seed,
                                 brh_observation *out,
                                 brh_compact_observation *compactOut,
                                 int captureMode);
static int bridge_step_internal(long key,
                                int control,
                                int shift,
                                brh_observation *out,
                                brh_compact_observation *compactOut,
                                int captureMode);
static int bridge_start_coroutine_locked(void);
static int bridge_resume_coroutine_locked(void);
static void bridge_finish_coroutine_locked(int exitCode);
static int bridge_env_validate_active(brh_env *env);
static void bridge_env_update_outputs(brh_env *env);
static void bridge_env_update_outputs_from_observation(brh_env *env, const brh_observation *observation);
static int bridge_check_observation_ready_locked(boolean allowExit);
static void bridge_copy_observation(brh_observation *to, const brh_observation *from);
static void bridge_initialize_launch_state(uint64_t seed);
static int bridge_append_message_line(brh_observation *out, int offset, const char *line);
static int bridge_copy_plain_text(uint8_t *target, int targetLength, const char *source);
static int bridge_map_index(short x, short y);
static boolean bridge_loc_is_in_map(pos loc);
static int bridge_inventory_slot(const brh_observation *out, char letter);
static void bridge_compact_rebuild_map_chars(void);
static void bridge_compact_update_map_cell(short x, short y);
static enum displayGlyph bridge_compact_cell_glyph(short x, short y);
static enum displayGlyph bridge_compact_terrain_glyph(short x, short y);
static int16_t bridge_observed_item_kind(const item *theItem);
static int16_t bridge_observed_remembered_item_kind(uint16_t category, short kind);
static int16_t bridge_observed_enchant(const item *theItem, short enchant);
static int16_t bridge_observed_charges(const item *theItem);
static void bridge_store_map_item(brh_observation *out, int cellIndex, const item *theItem);
static void bridge_store_inventory_item(brh_observation *out, int slot, item *theItem);
static void bridge_store_monster(brh_observation *out, const creature *monst);
static uint8_t bridge_color_component(char component);
static int16_t bridge_glyph_component(enum displayGlyph glyph);
static uint32_t bridge_char_component(enum displayGlyph glyph);

static struct brogueConsole bridgeConsole = {
    bridge_gameLoop,
    bridge_pauseForMilliseconds,
    bridge_nextKeyOrMouseEvent,
    bridge_plotChar,
    bridge_remap,
    bridge_modifierHeld,
    bridge_notifyEvent,
    NULL,
    bridge_setGraphicsMode
};

int brh_reset(uint64_t seed, brh_observation *out) {
    return bridge_reset_internal(seed, out, NULL, BRH_CAPTURE_FULL);
}

int brh_reset_compact(uint64_t seed, brh_compact_observation *out) {
    return bridge_reset_internal(seed, NULL, out, BRH_CAPTURE_COMPACT);
}

static int bridge_reset_internal(uint64_t seed,
                                 brh_observation *out,
                                 brh_compact_observation *compactOut,
                                 int captureMode) {
    int rc;
    int prof;
    uint64_t start = 0;

    if (captureMode == BRH_CAPTURE_FULL && out == NULL) {
        bridge_set_error("observation pointer must not be null");
        return -1;
    }
    if (captureMode == BRH_CAPTURE_COMPACT && compactOut == NULL) {
        bridge_set_error("compact observation pointer must not be null");
        return -1;
    }

    prof = bridge_profile_enabled();
    if (prof) {
        start = bridge_now_ns();
        bridgeProfile.reset_calls++;
    }
    brh_close();

    bridgeLastError[0] = '\0';
    bridgeWaitingForAction = false;
    bridgeActionReady = false;
    bridgeCloseRequested = false;
    bridgeExited = false;
    bridgeExitCode = 0;
    bridgeLastStepStatus = BRH_STEP_OK;
    bridgeCaptureMode = captureMode;
    bridgeActiveControl = false;
    bridgeActiveShift = false;
    memset(&bridgeLastObservation, 0, sizeof(bridgeLastObservation));
    memset(&bridgeLastCompactObservation, 0, sizeof(bridgeLastCompactObservation));
    memset(bridgeCompactMapChars, ' ', sizeof(bridgeCompactMapChars));
    bridgeCompactMapDirty = true;
    memset(&bridgePendingEvent, 0, sizeof(bridgePendingEvent));
    bridgeEndScore = 0;
    bridgeEndWon = false;

    currentConsole = bridgeConsole;
    serverMode = true;
    nonInteractivePlayback = false;
    hasGraphics = false;
    graphicsMode = TEXT_GRAPHICS;
    gameVariant = VARIANT_BROGUE;
    bridge_initialize_launch_state(seed);

    rc = bridge_start_coroutine_locked();
    if (rc != 0) {
        rc = -1;
        goto done;
    }

    rc = bridge_resume_coroutine_locked();
    if (rc == 0) {
        rc = bridge_check_observation_ready_locked(false);
    }
    if (rc == 0) {
        if (captureMode == BRH_CAPTURE_FULL) {
            bridge_copy_observation(out, &bridgeLastObservation);
        } else if (captureMode == BRH_CAPTURE_COMPACT) {
            memcpy(compactOut, &bridgeLastCompactObservation, sizeof(*compactOut));
        }
    }
done:
    if (prof) bridgeProfile.reset_ns += bridge_now_ns() - start;
    return rc;
}

int brh_step(long key, int control, int shift, brh_observation *out) {
    return bridge_step_internal(key, control, shift, out, NULL, BRH_CAPTURE_FULL);
}

int brh_step_compact(long key, int control, int shift, brh_compact_observation *out) {
    return bridge_step_internal(key, control, shift, NULL, out, BRH_CAPTURE_COMPACT);
}

int brh_step_no_observation(long key, int control, int shift) {
    return bridge_step_internal(key, control, shift, NULL, NULL, BRH_CAPTURE_NONE);
}

static int bridge_step_internal(long key,
                                int control,
                                int shift,
                                brh_observation *out,
                                brh_compact_observation *compactOut,
                                int captureMode) {
    int rc;
    int prof;
    uint64_t start = 0;

    if (captureMode == BRH_CAPTURE_FULL && out == NULL) {
        bridge_set_error("observation pointer must not be null");
        return -1;
    }
    if (captureMode == BRH_CAPTURE_COMPACT && compactOut == NULL) {
        bridge_set_error("compact observation pointer must not be null");
        return -1;
    }

    prof = bridge_profile_enabled();
    if (prof) {
        start = bridge_now_ns();
        bridgeProfile.step_calls++;
    }
    if (!bridgeThreadStarted || bridgeExited) {
        bridge_set_error_locked("Brogue bridge is not running");
        rc = -1;
        goto done;
    }
    if (!bridgeWaitingForAction) {
        bridge_set_error_locked("Brogue bridge is not waiting for an action");
        rc = -1;
        goto done;
    }

    bridgePendingEvent.eventType = KEYSTROKE;
    bridgePendingEvent.param1 = key;
    bridgePendingEvent.param2 = 0;
    bridgePendingEvent.controlKey = control ? true : false;
    bridgePendingEvent.shiftKey = shift ? true : false;
    bridgeLastStepStatus = BRH_STEP_OK;
    if (captureMode == BRH_CAPTURE_COMPACT && bridgeCaptureMode != BRH_CAPTURE_COMPACT) {
        bridgeCompactMapDirty = true;
    }
    bridgeCaptureMode = captureMode;
    bridgeActionReady = true;
    bridgeWaitingForAction = false;

    rc = bridge_resume_coroutine_locked();
    if (rc == 0) {
        rc = bridge_check_observation_ready_locked(true);
    }
    if (rc == 0) {
        if (captureMode == BRH_CAPTURE_FULL) {
            bridge_copy_observation(out, &bridgeLastObservation);
        } else if (captureMode == BRH_CAPTURE_COMPACT) {
            memcpy(compactOut, &bridgeLastCompactObservation, sizeof(*compactOut));
        }
        rc = bridgeLastStepStatus;
    }
done:
    if (prof) bridgeProfile.step_ns += bridge_now_ns() - start;
    return rc;
}

uint32_t brh_abi_version(void) {
    return BRH_ABI_VERSION;
}

size_t brh_observation_size(void) {
    return sizeof(brh_observation);
}

size_t brh_compact_observation_size(void) {
    return sizeof(brh_compact_observation);
}

int brh_bridge_should_refresh_sidebar(void) {
    return bridgeCaptureMode == BRH_CAPTURE_FULL;
}

int brh_bridge_should_refresh_dungeon_cell(void) {
    return bridgeCaptureMode == BRH_CAPTURE_FULL;
}

int brh_bridge_allows_compact_simulation_shortcuts(void) {
    const char *value;

    if (bridgeCaptureMode != BRH_CAPTURE_COMPACT) {
        return 0;
    }

    value = getenv("BROGUE_COMPACT_SIMULATION_SHORTCUTS");
    return (value != NULL && value[0] != '\0' && strcmp(value, "0") != 0) ? 1 : 0;
}

void brh_bridge_update_compact_cell(short x, short y) {
    if (bridgeCaptureMode == BRH_CAPTURE_COMPACT) {
        bridge_compact_update_map_cell(x, y);
    }
}

int brh_screen_cols(void) {
    return COLS;
}

int brh_screen_rows(void) {
    return ROWS;
}

int brh_map_cols(void) {
    return DCOLS;
}

int brh_map_rows(void) {
    return DROWS;
}

int brh_inventory_size(void) {
    return BRH_INVENTORY_SIZE;
}

int brh_inventory_str_length(void) {
    return BRH_INVENTORY_STR_LENGTH;
}

void brh_close(void) {
    if (bridgeThreadStarted && !bridgeExited) {
        bridgeCloseRequested = true;
        bridgeActionReady = true;
        while (!bridgeExited && bridgeWaitingForAction) {
            bridgeWaitingForAction = false;
            if (bridge_resume_coroutine_locked() != 0) {
                break;
            }
        }
    }

    bridgeThreadStarted = false;
    bridgeWaitingForAction = false;
    bridgeActionReady = false;
    bridgeCloseRequested = false;
    bridgeExited = false;
    bridgeExitCode = 0;
    free(bridgeGameStack);
    bridgeGameStack = NULL;
}

void brh_set_data_dir(const char *path) {
    if (path == NULL || path[0] == '\0') {
        return;
    }

    strncpy(dataDirectory, path, BROGUE_FILENAME_MAX - 1);
    dataDirectory[BROGUE_FILENAME_MAX - 1] = '\0';
}

const char *brh_last_error(void) {
    return bridgeLastError;
}

void brh_mark_invalid_key(void) {
    if (bridgeThreadStarted && !bridgeExited) {
        bridgeLastStepStatus = BRH_STEP_INVALID_KEY;
    }
}

void brh_profile_reset(void) {
    memset(&bridgeProfile, 0, sizeof(bridgeProfile));
}

void brh_profile_read(brh_profile *out) {
    if (out == NULL) {
        return;
    }
    *out = bridgeProfile;
}

uint64_t brh_profile_zone_start(int zone) {
    if (!bridge_profile_enabled()
        || zone < 0
        || zone >= BRH_PROFILE_ZONE_COUNT) {
        return 0;
    }
    return bridge_now_ns();
}

void brh_profile_zone_end(int zone, uint64_t start_ns) {
    if (start_ns == 0
        || zone < 0
        || zone >= BRH_PROFILE_ZONE_COUNT) {
        return;
    }
    bridgeProfile.zone_calls[zone]++;
    bridgeProfile.zone_ns[zone] += bridge_now_ns() - start_ns;
}

brh_env *brh_env_create(const brh_env_buffers *buffers) {
    brh_env *env;

    if (buffers == NULL) {
        bridge_set_error("BrogueEnv buffers pointer must not be null");
        return NULL;
    }
    if (buffers->observations == NULL) {
        bridge_set_error("BrogueEnv observations buffer must not be null");
        return NULL;
    }
    if (buffers->rewards == NULL) {
        bridge_set_error("BrogueEnv rewards buffer must not be null");
        return NULL;
    }
    if (buffers->terminals == NULL) {
        bridge_set_error("BrogueEnv terminals buffer must not be null");
        return NULL;
    }

    pthread_mutex_lock(&bridgeEnvMutex);
    if (bridgeActiveEnv != NULL) {
        pthread_mutex_unlock(&bridgeEnvMutex);
        bridge_set_error("only one live BrogueEnv is supported until Brogue state is per-env");
        return NULL;
    }

    env = (brh_env *) calloc(1, sizeof(*env));
    if (env == NULL) {
        pthread_mutex_unlock(&bridgeEnvMutex);
        bridge_set_error("failed to allocate BrogueEnv");
        return NULL;
    }

    env->observations = buffers->observations;
    env->actions = buffers->actions;
    env->controls = buffers->controls;
    env->shifts = buffers->shifts;
    env->rewards = buffers->rewards;
    env->terminals = buffers->terminals;
    env->num_agents = 1;
    env->rng = 0;
    env->running = false;
    bridgeActiveEnv = env;
    pthread_mutex_unlock(&bridgeEnvMutex);
    return env;
}

int brh_env_reset(brh_env *env, uint64_t seed) {
    int rc;

    if (bridge_env_validate_active(env) != 0) {
        return -1;
    }

    rc = brh_reset(seed, env->observations);
    if (rc == 0) {
        env->running = true;
        bridge_env_update_outputs(env);
    } else {
        env->running = false;
    }
    return rc;
}

int brh_env_step(brh_env *env, long key, int control, int shift) {
    int rc;

    if (bridge_env_validate_active(env) != 0) {
        return -1;
    }
    if (!env->running) {
        bridge_set_error("BrogueEnv is not running; call brh_env_reset first");
        return -1;
    }

    rc = brh_step(key, control, shift, env->observations);
    if (rc >= 0) {
        bridge_env_update_outputs(env);
        if (env->terminals[0] != 0.0f) {
            env->running = false;
        }
    }
    return rc;
}

int brh_env_step_no_observation(brh_env *env, long key, int control, int shift) {
    int rc;

    if (bridge_env_validate_active(env) != 0) {
        return -1;
    }
    if (!env->running) {
        bridge_set_error("BrogueEnv is not running; call brh_env_reset first");
        return -1;
    }

    rc = brh_step_no_observation(key, control, shift);
    if (rc >= 0) {
        bridge_env_update_outputs_from_observation(env, &bridgeLastObservation);
        if (env->terminals[0] != 0.0f) {
            env->running = false;
        }
    }
    return rc;
}

int brh_env_step_from_buffers(brh_env *env) {
    int control = 0;
    int shift = 0;

    if (bridge_env_validate_active(env) != 0) {
        return -1;
    }
    if (env->actions == NULL) {
        bridge_set_error("BrogueEnv action buffer must not be null for brh_env_step_from_buffers");
        return -1;
    }
    if (env->controls != NULL) {
        control = env->controls[0] ? 1 : 0;
    }
    if (env->shifts != NULL) {
        shift = env->shifts[0] ? 1 : 0;
    }
    return brh_env_step(env, env->actions[0], control, shift);
}

int brh_env_num_agents(const brh_env *env) {
    if (env == NULL) {
        return 0;
    }
    return env->num_agents;
}

void brh_env_close(brh_env *env) {
    boolean shouldClose = false;

    if (env == NULL) {
        return;
    }

    pthread_mutex_lock(&bridgeEnvMutex);
    if (bridgeActiveEnv == env) {
        bridgeActiveEnv = NULL;
        shouldClose = true;
    }
    pthread_mutex_unlock(&bridgeEnvMutex);

    if (shouldClose) {
        brh_close();
        free(env);
    }
}

boolean tryParseUint64(char *str, uint64_t *num) {
    unsigned long long n;
    char buf[100];

    if (strlen(str)
        && sscanf(str, "%llu", &n)
        && sprintf(buf, "%llu", n)
        && !strcmp(buf, str)) {
        *num = (uint64_t) n;
        return true;
    }
    return false;
}

static void bridge_coroutine_main(void) {
    int exitCode;

    exitCode = rogueMain();
    bridge_finish_coroutine_locked(exitCode);
}

int brh_profile_enabled_from_env(void) {
    const char *value = getenv("BROGUE_PROFILE");
    return (value != NULL && value[0] != '\0' && strcmp(value, "0") != 0) ? 1 : 0;
}

static int bridge_profile_enabled(void) {
    if (brh_profile_enabled_cache < 0) {
        brh_profile_enabled_cache = brh_profile_enabled_from_env();
    }
    return brh_profile_enabled_cache;
}

static uint64_t bridge_now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t) ts.tv_sec * 1000000000ull + (uint64_t) ts.tv_nsec;
}

static void bridge_gameLoop(void) {
    (void) rogueMain();
}

static boolean bridge_pauseForMilliseconds(short milliseconds, PauseBehavior behavior) {
    if (bridge_profile_enabled()) bridgeProfile.pause_calls++;
    (void) milliseconds;
    (void) behavior;
    return false;
}

static void bridge_nextKeyOrMouseEvent(rogueEvent *returnEvent,
                                       boolean textInput,
                                       boolean colorsDance) {
    (void) textInput;
    (void) colorsDance;

    if (bridgeCloseRequested) {
        rogue.quit = true;
        rogue.gameHasEnded = true;
        returnEvent->eventType = KEYSTROKE;
        returnEvent->param1 = ESCAPE_KEY;
        returnEvent->param2 = 0;
        returnEvent->controlKey = false;
        returnEvent->shiftKey = false;
        bridgeActionReady = false;
        bridgeWaitingForAction = false;
        return;
    }

    if (bridge_profile_enabled()) {
        bridgeProfile.input_yields++;
    }
    if (bridgeCaptureMode == BRH_CAPTURE_FULL) {
        if (bridge_profile_enabled()) {
            uint64_t obsStart = bridge_now_ns();
            bridgeProfile.observation_fills++;
            bridge_fill_observation(&bridgeLastObservation);
            bridgeProfile.observation_fill_ns += bridge_now_ns() - obsStart;
        } else {
            bridge_fill_observation(&bridgeLastObservation);
        }
    } else if (bridgeCaptureMode == BRH_CAPTURE_COMPACT) {
        if (bridge_profile_enabled()) {
            uint64_t obsStart = bridge_now_ns();
            bridgeProfile.compact_observation_fills++;
            bridge_fill_compact_observation(&bridgeLastCompactObservation);
            bridgeProfile.compact_observation_fill_ns += bridge_now_ns() - obsStart;
        } else {
            bridge_fill_compact_observation(&bridgeLastCompactObservation);
        }
    } else {
        bridge_fill_program_state(&bridgeLastObservation);
    }
    bridgeWaitingForAction = true;
    bridgeActionReady = false;

    if (swapcontext(&bridgeGameContext, &bridgeCallerContext) != 0) {
        bridge_set_error_locked("failed to yield from Brogue coroutine");
        rogue.quit = true;
        rogue.gameHasEnded = true;
        returnEvent->eventType = KEYSTROKE;
        returnEvent->param1 = ESCAPE_KEY;
        returnEvent->param2 = 0;
        returnEvent->controlKey = false;
        returnEvent->shiftKey = false;
        return;
    }

    if (bridgeCloseRequested) {
        rogue.quit = true;
        rogue.gameHasEnded = true;
        returnEvent->eventType = KEYSTROKE;
        returnEvent->param1 = ACKNOWLEDGE_KEY;
        returnEvent->param2 = 0;
        returnEvent->controlKey = false;
        returnEvent->shiftKey = false;
        bridgeActionReady = false;
        bridgeWaitingForAction = false;
        return;
    }

    *returnEvent = bridgePendingEvent;
    bridgeActiveControl = bridgePendingEvent.controlKey;
    bridgeActiveShift = bridgePendingEvent.shiftKey;
    bridgeActionReady = false;
}

static void bridge_plotChar(enum displayGlyph inputChar,
                            short xLoc,
                            short yLoc,
                            short foreRed,
                            short foreGreen,
                            short foreBlue,
                            short backRed,
                            short backGreen,
                            short backBlue) {
    if (bridge_profile_enabled()) bridgeProfile.plot_calls++;
    (void) inputChar;
    (void) xLoc;
    (void) yLoc;
    (void) foreRed;
    (void) foreGreen;
    (void) foreBlue;
    (void) backRed;
    (void) backGreen;
    (void) backBlue;
}

static void bridge_remap(const char *input_name, const char *output_name) {
    (void) input_name;
    (void) output_name;
}

static boolean bridge_modifierHeld(int modifier) {
    if (bridge_profile_enabled()) bridgeProfile.modifier_calls++;
    if (modifier == 0) {
        return bridgeActiveShift;
    }
    if (modifier == 1) {
        return bridgeActiveControl;
    }
    return false;
}

static void bridge_notifyEvent(short eventId,
                               int data1,
                               int data2,
                               const char *str1,
                               const char *str2) {
    if (bridge_profile_enabled()) bridgeProfile.notify_calls++;
    (void) data2;
    (void) str1;
    (void) str2;
    if (eventId == GAMEOVER_VICTORY || eventId == GAMEOVER_SUPERVICTORY) {
        bridgeEndWon = true;
        bridgeEndScore = (int64_t) data1;
    } else if (eventId == GAMEOVER_DEATH || eventId == GAMEOVER_QUIT) {
        bridgeEndWon = false;
        bridgeEndScore = (int64_t) data1;
    } else {
        (void) eventId;
        (void) data1;
    }
}

static enum graphicsModes bridge_setGraphicsMode(enum graphicsModes mode) {
    return mode;
}

static void bridge_fill_observation(brh_observation *out) {
    int x;
    int y;
    int messageOffset = 0;

    memset(out, 0, sizeof(*out));
    bridge_fill_unknown_semantics(out);

    for (y = 0; y < ROWS; y++) {
        for (x = 0; x < COLS; x++) {
            int cellIndex = y * COLS + x;
            int colorIndex = cellIndex * 3;
            cellDisplayBuffer *cell = &displayBuffer.cells[x][y];

            out->glyphs[cellIndex] = bridge_glyph_component(cell->character);
            out->chars[cellIndex] = bridge_char_component(cell->character);
            out->colors_fg[colorIndex] = bridge_color_component(cell->foreColorComponents[0]);
            out->colors_fg[colorIndex + 1] = bridge_color_component(cell->foreColorComponents[1]);
            out->colors_fg[colorIndex + 2] = bridge_color_component(cell->foreColorComponents[2]);
            out->colors_bg[colorIndex] = bridge_color_component(cell->backColorComponents[0]);
            out->colors_bg[colorIndex + 1] = bridge_color_component(cell->backColorComponents[1]);
            out->colors_bg[colorIndex + 2] = bridge_color_component(cell->backColorComponents[2]);
        }
    }

    for (y = 0; y < MESSAGE_LINES && messageOffset < BRH_MESSAGE_SIZE - 1; y++) {
        messageOffset = bridge_append_message_line(out, messageOffset, displayedMessage[y]);
        if (messageOffset < BRH_MESSAGE_SIZE - 1) {
            out->message[messageOffset++] = (uint8_t) '\n';
        }
    }

    out->blstats[0] = player.loc.x;
    out->blstats[1] = player.loc.y;
    out->blstats[2] = rogue.strength;
    out->blstats[3] = player.currentHP;
    out->blstats[4] = player.info.maxHP;
    out->blstats[5] = rogue.depthLevel;
    out->blstats[6] = (int64_t) rogue.gold;
    out->blstats[7] = (int64_t) rogue.playerTurnNumber;
    out->blstats[8] = (int64_t) rogue.absoluteTurnNumber;
    out->blstats[9] = rogue.stealthRange;
    out->blstats[10] = player.status[STATUS_NUTRITION];

    bridge_fill_program_state(out);

    bridge_fill_map_semantics(out);
    bridge_fill_item_semantics(out);
    bridge_fill_monster_semantics(out);
    bridge_fill_inventory_semantics(out);
}

static void bridge_fill_compact_observation(brh_compact_observation *out) {
    if (bridgeCompactMapDirty) {
        bridge_compact_rebuild_map_chars();
    }
    memcpy(out->chars, bridgeCompactMapChars, sizeof(out->chars));
    memset(out->blstats, 0, sizeof(out->blstats));
    memset(out->program_state, 0, sizeof(out->program_state));

    out->blstats[0] = player.loc.x;
    out->blstats[1] = player.loc.y;
    out->blstats[2] = rogue.strength;
    out->blstats[3] = player.currentHP;
    out->blstats[4] = player.info.maxHP;
    out->blstats[5] = rogue.depthLevel;
    out->blstats[6] = (int32_t) rogue.gold;
    out->blstats[7] = (int32_t) rogue.playerTurnNumber;
    out->blstats[8] = (int32_t) rogue.absoluteTurnNumber;
    out->blstats[9] = rogue.stealthRange;
    out->blstats[10] = player.status[STATUS_NUTRITION];

    bridge_fill_compact_program_state(out);
}

static void bridge_fill_program_state(brh_observation *out) {
    int prof = bridge_profile_enabled();
    uint64_t start = 0;
    if (prof) {
        start = bridge_now_ns();
        bridgeProfile.program_fills++;
    }
    out->program_state[0] = rogue.playerTurnNumber;
    out->program_state[1] = rogue.gameHasEnded ? 1 : 0;
    out->program_state[2] = 0;       /* won: always 0 mid-game; patched to 1 at GAMEOVER_VICTORY */
    out->program_state[3] = rogue.depthLevel;
    out->program_state[4] = rogue.seed;
    out->program_state[5] = rogue.gold;
    out->program_state[6] = rogue.gold; /* score proxy: rogue.gold mid-game; patched to final score at game end */
    out->program_state[7] = (rogue.gameInProgress ? 1 : 0)
                          | (rogue.quit ? 2 : 0)
                          | (rogue.disturbed ? 4 : 0);
    if (prof) bridgeProfile.program_fill_ns += bridge_now_ns() - start;
}

static void bridge_fill_compact_program_state(brh_compact_observation *out) {
    int prof = bridge_profile_enabled();
    uint64_t start = 0;
    if (prof) {
        start = bridge_now_ns();
        bridgeProfile.program_fills++;
    }
    out->program_state[0] = (int32_t) rogue.playerTurnNumber;
    out->program_state[1] = rogue.gameHasEnded ? 1 : 0;
    out->program_state[2] = 0;
    out->program_state[3] = rogue.depthLevel;
    out->program_state[4] = (int32_t) rogue.seed;
    out->program_state[5] = (int32_t) rogue.gold;
    out->program_state[6] = (int32_t) rogue.gold;
    out->program_state[7] = (rogue.gameInProgress ? 1 : 0)
                          | (rogue.quit ? 2 : 0)
                          | (rogue.disturbed ? 4 : 0);
    if (prof) bridgeProfile.program_fill_ns += bridge_now_ns() - start;
}

static void bridge_fill_unknown_semantics(brh_observation *out) {
    int i;

    for (i = 0; i < BRH_MAP_CELLS; i++) {
        out->map_item_kind[i] = BRH_UNKNOWN_SHORT;
        out->map_item_quantity[i] = BRH_UNKNOWN_SHORT;
        out->map_monster_kind[i] = BRH_UNKNOWN_SHORT;
        out->map_monster_hp[i] = BRH_UNKNOWN_SHORT;
        out->map_monster_state[i] = BRH_UNKNOWN_SHORT;
    }
    for (i = 0; i < BRH_INVENTORY_SIZE; i++) {
        out->inventory_kind[i] = BRH_UNKNOWN_SHORT;
        out->inventory_quantity[i] = BRH_UNKNOWN_SHORT;
        out->inventory_enchant1[i] = BRH_UNKNOWN_SHORT;
        out->inventory_enchant2[i] = BRH_UNKNOWN_SHORT;
        out->inventory_charges[i] = BRH_UNKNOWN_SHORT;
    }
}

static void bridge_fill_map_semantics(brh_observation *out) {
    short x;
    short y;
    int layer;

    for (y = 0; y < DROWS; y++) {
        for (x = 0; x < DCOLS; x++) {
            int cellIndex = bridge_map_index(x, y);
            int layerIndex = cellIndex * NUMBER_TERRAIN_LAYERS;
            int colorIndex = cellIndex * 3;
            pcell *cell = &pmap[x][y];
            boolean visible = (cell->flags & ANY_KIND_OF_VISIBLE) ? true : false;
            boolean known = visible || (cell->flags & (DISCOVERED | MAGIC_MAPPED));

            if (visible) {
                for (layer = 0; layer < NUMBER_TERRAIN_LAYERS; layer++) {
                    out->map_layers[layerIndex + layer] = (uint16_t) cell->layers[layer];
                }
                out->map_volume[cellIndex] = cell->volume;
                out->map_machine[cellIndex] = cell->machineNumber;
                out->map_light[colorIndex] = tmap[x][y].light[0];
                out->map_light[colorIndex + 1] = tmap[x][y].light[1];
                out->map_light[colorIndex + 2] = tmap[x][y].light[2];
                out->map_flags[cellIndex] = (uint64_t) (cell->flags & BRH_OBS_CELL_FLAGS);
            } else if (known) {
                out->map_layers[layerIndex + DUNGEON] = (uint16_t) cell->rememberedTerrain;
                out->map_flags[cellIndex] = (uint64_t) (cell->rememberedCellFlags & BRH_OBS_CELL_FLAGS);
                if (cell->rememberedItemCategory) {
                    out->map_has_item[cellIndex] = 1;
                    out->map_item_category[cellIndex] = (uint16_t) cell->rememberedItemCategory;
                    out->map_item_kind[cellIndex] = bridge_observed_remembered_item_kind(
                        out->map_item_category[cellIndex],
                        cell->rememberedItemKind);
                    out->map_item_quantity[cellIndex] = cell->rememberedItemQuantity;
                }
            }
        }
    }
}

static void bridge_fill_item_semantics(brh_observation *out) {
    item *theItem;

    if (floorItems == NULL) {
        return;
    }
    for (theItem = floorItems->nextItem; theItem != NULL; theItem = theItem->nextItem) {
        if (bridge_loc_is_in_map(theItem->loc)
            && (pmapAt(theItem->loc)->flags & (ANY_KIND_OF_VISIBLE | ITEM_DETECTED))) {
            bridge_store_map_item(out, bridge_map_index(theItem->loc.x, theItem->loc.y), theItem);
        }
    }
}

static void bridge_fill_monster_semantics(brh_observation *out) {
    if (bridge_loc_is_in_map(player.loc)) {
        bridge_store_monster(out, &player);
    }
    if (monsters == NULL) {
        return;
    }
    for (creatureIterator it = iterateCreatures(monsters); hasNextCreature(it);) {
        creature *monst = nextCreature(&it);
        if (bridge_loc_is_in_map(monst->loc)
            && (pmapAt(monst->loc)->flags & ANY_KIND_OF_VISIBLE)) {
            bridge_store_monster(out, monst);
        }
    }
}

static void bridge_fill_inventory_semantics(brh_observation *out) {
    item *theItem;

    if (packItems == NULL) {
        return;
    }
    for (theItem = packItems->nextItem; theItem != NULL; theItem = theItem->nextItem) {
        int slot = bridge_inventory_slot(out, theItem->inventoryLetter);
        if (slot >= 0) {
            bridge_store_inventory_item(out, slot, theItem);
        }
    }
}

static void bridge_set_error(const char *message) {
    bridge_set_error_locked(message);
}

static void bridge_set_error_locked(const char *message) {
    strncpy(bridgeLastError, message, sizeof(bridgeLastError) - 1);
    bridgeLastError[sizeof(bridgeLastError) - 1] = '\0';
}

static int bridge_start_coroutine_locked(void) {
    if (bridgeGameStack != NULL) {
        free(bridgeGameStack);
        bridgeGameStack = NULL;
    }

    bridgeGameStack = calloc(1, BRH_THREAD_STACK_SIZE);
    if (bridgeGameStack == NULL) {
        bridgeThreadStarted = false;
        bridge_set_error_locked("failed to allocate Brogue coroutine stack");
        return -1;
    }

    if (getcontext(&bridgeGameContext) != 0) {
        free(bridgeGameStack);
        bridgeGameStack = NULL;
        bridgeThreadStarted = false;
        bridge_set_error_locked("failed to initialize Brogue coroutine context");
        return -1;
    }

    bridgeGameContext.uc_stack.ss_sp = bridgeGameStack;
    bridgeGameContext.uc_stack.ss_size = BRH_THREAD_STACK_SIZE;
    bridgeGameContext.uc_stack.ss_flags = 0;
    bridgeGameContext.uc_link = &bridgeCallerContext;
    makecontext(&bridgeGameContext, bridge_coroutine_main, 0);

    bridgeThreadStarted = true;
    return 0;
}

static int bridge_resume_coroutine_locked(void) {
    int rc = 0;
    int prof = bridge_profile_enabled();
    uint64_t start = 0;

    if (prof) {
        start = bridge_now_ns();
        bridgeProfile.resume_calls++;
    }
    if (!bridgeThreadStarted || bridgeExited) {
        goto done;
    }
    if (swapcontext(&bridgeCallerContext, &bridgeGameContext) != 0) {
        snprintf(bridgeLastError,
                 sizeof(bridgeLastError),
                 "failed to resume Brogue coroutine: %s",
                 strerror(errno));
        rc = -1;
    }
done:
    if (prof) bridgeProfile.resume_ns += bridge_now_ns() - start;
    return rc;
}

static void bridge_finish_coroutine_locked(int exitCode) {
    bridgeExitCode = exitCode;
    bridgeExited = true;
    bridgeWaitingForAction = false;
    /*
     * rogueMain() frees level-owned lists before returning. Keep the last
     * observation captured at an input boundary instead of walking freed
     * terrain/item/monster globals during shutdown.
     */
    bridgeLastObservation.program_state[1] = 1;
    bridgeLastObservation.program_state[2] = bridgeEndWon ? 1 : 0;
    bridgeLastObservation.program_state[6] = (uint64_t) bridgeEndScore;
    bridgeLastCompactObservation.program_state[1] = 1;
    bridgeLastCompactObservation.program_state[2] = bridgeEndWon ? 1 : 0;
    bridgeLastCompactObservation.program_state[6] = (int32_t) bridgeEndScore;
}

static int bridge_env_validate_active(brh_env *env) {
    boolean valid;

    if (env == NULL) {
        bridge_set_error("BrogueEnv handle must not be null");
        return -1;
    }

    pthread_mutex_lock(&bridgeEnvMutex);
    valid = (bridgeActiveEnv == env);
    pthread_mutex_unlock(&bridgeEnvMutex);
    if (!valid) {
        bridge_set_error("BrogueEnv handle is not active");
        return -1;
    }
    return 0;
}

static void bridge_env_update_outputs(brh_env *env) {
    bridge_env_update_outputs_from_observation(env, env->observations);
}

static void bridge_env_update_outputs_from_observation(brh_env *env, const brh_observation *observation) {
    env->rewards[0] = 0.0f;
    env->terminals[0] = observation->program_state[1] ? 1.0f : 0.0f;
}

static int bridge_check_observation_ready_locked(boolean allowExit) {
    if (bridgeExited && !bridgeWaitingForAction) {
        if (allowExit) {
            return 0;
        }
        snprintf(bridgeLastError,
                 sizeof(bridgeLastError),
                 "Brogue bridge exited with status %d",
                 bridgeExitCode);
        return -1;
    }
    if (!bridgeWaitingForAction) {
        bridge_set_error_locked("Brogue bridge did not reach an input boundary");
        return -1;
    }
    return 0;
}

static void bridge_copy_observation(brh_observation *to, const brh_observation *from) {
    memcpy(to, from, sizeof(*to));
}

static void bridge_initialize_launch_state(uint64_t seed) {
    memset((void *) &rogue, 0, sizeof(playerCharacter));
    rogue.nextGame = seed == 0 ? NG_NEW_GAME : NG_NEW_GAME_WITH_SEED;
    rogue.nextGameSeed = seed;
    rogue.mode = GAME_MODE_NORMAL;
    rogue.displayStealthRangeMode = false;
    rogue.trueColorMode = false;
}

static int bridge_append_message_line(brh_observation *out, int offset, const char *line) {
    if (offset >= BRH_MESSAGE_SIZE - 1) {
        return BRH_MESSAGE_SIZE - 1;
    }
    offset += bridge_copy_plain_text(&out->message[offset], BRH_MESSAGE_SIZE - offset, line);
    return offset;
}

static int bridge_copy_plain_text(uint8_t *target, int targetLength, const char *source) {
    int inputOffset = 0;
    int outputOffset = 0;

    if (targetLength <= 0) {
        return 0;
    }
    memset(target, 0, targetLength);
    while (source[inputOffset] != '\0' && outputOffset < targetLength - 1) {
        if ((uint8_t) source[inputOffset] == COLOR_ESCAPE) {
            int skip;

            inputOffset++;
            for (skip = 0; skip < 3 && source[inputOffset] != '\0'; skip++) {
                inputOffset++;
            }
            continue;
        }
        target[outputOffset++] = (uint8_t) source[inputOffset++];
    }
    return outputOffset;
}

static int bridge_map_index(short x, short y) {
    return y * DCOLS + x;
}

static boolean bridge_loc_is_in_map(pos loc) {
    return loc.x >= 0 && loc.x < DCOLS && loc.y >= 0 && loc.y < DROWS;
}

static int bridge_inventory_slot(const brh_observation *out, char letter) {
    int i;

    if (letter >= 'a' && letter <= 'z') {
        return letter - 'a';
    }
    for (i = 0; i < BRH_INVENTORY_SIZE; i++) {
        if (out->inventory_letters[i] == 0) {
            return i;
        }
    }
    return -1;
}

static void bridge_compact_rebuild_map_chars(void) {
    short x;
    short y;

    for (y = 0; y < DROWS; y++) {
        for (x = 0; x < DCOLS; x++) {
            bridge_compact_update_map_cell(x, y);
        }
    }
    bridgeCompactMapDirty = false;
}

static void bridge_compact_update_map_cell(short x, short y) {
    int cellIndex;
    uint32_t ch;

    if (x < 0 || x >= DCOLS || y < 0 || y >= DROWS) {
        return;
    }

    cellIndex = y * DCOLS + x;
    ch = bridge_char_component(bridge_compact_cell_glyph(x, y));
    bridgeCompactMapChars[cellIndex] = (ch > 0 && ch <= UINT8_MAX) ? (uint8_t) ch : (uint8_t) '?';
}

static enum displayGlyph bridge_compact_cell_glyph(short x, short y) {
    pos loc = { x, y };
    pcell *cell = &pmap[x][y];
    creature *monst = NULL;

    if (cell->flags & HAS_PLAYER) {
        return player.info.displayChar;
    }

    if (cell->flags & HAS_MONSTER) {
        monst = monsterAtLoc(loc);
    } else if (cell->flags & HAS_DORMANT_MONSTER) {
        monst = dormantMonsterAtLoc(loc);
    }

    if ((cell->flags & HAS_MONSTER)
        && monst != NULL
        && (playerCanSeeOrSense(x, y)
            || ((monst->info.flags & MONST_IMMOBILE) && (cell->flags & DISCOVERED)))
        && (!monsterIsHidden(monst, &player) || rogue.playbackOmniscience)) {
        return monst->info.displayChar;
    }

    if (monst != NULL && monsterRevealed(monst) && !canSeeMonster(monst)) {
        return monst->info.isLarge ? 'X' : 'x';
    }

    if ((cell->flags & HAS_ITEM)
        && !cellHasTerrainFlag(loc, T_OBSTRUCTS_ITEMS)
        && (playerCanSeeOrSense(x, y)
            || ((cell->flags & DISCOVERED) && !cellHasTerrainFlag(loc, T_MOVES_ITEMS)))) {
        item *theItem = itemAtLoc(loc);
        if (theItem != NULL) {
            return theItem->displayChar;
        }
    }

    return bridge_compact_terrain_glyph(x, y);
}

static enum displayGlyph bridge_compact_terrain_glyph(short x, short y) {
    pcell *cell = &pmap[x][y];
    enum tileType tile = NOTHING;

    if (!playerCanSeeOrSense(x, y)
        && !(cell->flags & (ITEM_DETECTED | HAS_PLAYER))
        && (cell->flags & (DISCOVERED | MAGIC_MAPPED))
        && (cell->flags & STABLE_MEMORY)
        && cell->rememberedAppearance.character) {
        return cell->rememberedAppearance.character;
    }

    if (!(cell->flags & DISCOVERED) && !rogue.playbackOmniscience) {
        if (!(cell->flags & MAGIC_MAPPED)) {
            return ' ';
        }
        tile = cell->layers[LIQUID] ? cell->layers[LIQUID] : cell->layers[DUNGEON];
    } else if (cell->layers[SURFACE]) {
        tile = cell->layers[SURFACE];
    } else if (cell->layers[LIQUID]) {
        tile = cell->layers[LIQUID];
    } else {
        tile = cell->layers[DUNGEON];
    }

    return (tile && tileCatalog[tile].displayChar) ? tileCatalog[tile].displayChar : ' ';
}

static int16_t bridge_observed_item_kind(const item *theItem) {
    if (theItem->category & (FOOD | WEAPON | ARMOR | CHARM | GOLD | AMULET | GEM | KEY)) {
        return theItem->kind;
    }
    if (theItem->flags & ITEM_IDENTIFIED) {
        return theItem->kind;
    }
    return BRH_UNKNOWN_SHORT;
}

static int16_t bridge_observed_remembered_item_kind(uint16_t category, short kind) {
    if (category & (FOOD | WEAPON | ARMOR | CHARM | GOLD | AMULET | GEM | KEY)) {
        return kind;
    }
    return BRH_UNKNOWN_SHORT;
}

static int16_t bridge_observed_enchant(const item *theItem, short enchant) {
    if (theItem->flags & ITEM_IDENTIFIED) {
        return enchant;
    }
    return BRH_UNKNOWN_SHORT;
}

static int16_t bridge_observed_charges(const item *theItem) {
    if (theItem->flags & (ITEM_IDENTIFIED | ITEM_MAX_CHARGES_KNOWN)) {
        return theItem->charges;
    }
    return BRH_UNKNOWN_SHORT;
}

static void bridge_store_map_item(brh_observation *out, int cellIndex, const item *theItem) {
    out->map_has_item[cellIndex] = 1;
    out->map_item_category[cellIndex] = (uint16_t) theItem->category;
    out->map_item_kind[cellIndex] = bridge_observed_item_kind(theItem);
    out->map_item_quantity[cellIndex] = theItem->quantity;
    out->map_item_flags[cellIndex] = (uint64_t) (theItem->flags & BRH_OBS_ITEM_FLAGS);
}

static void bridge_store_inventory_item(brh_observation *out, int slot, item *theItem) {
    char name[COLS * 3] = "";

    out->inventory_present[slot] = 1;
    out->inventory_letters[slot] = (uint8_t) theItem->inventoryLetter;
    itemName(theItem, name, true, true, NULL);
    bridge_copy_plain_text(&out->inventory_strs[slot * BRH_INVENTORY_STR_LENGTH],
                           BRH_INVENTORY_STR_LENGTH,
                           name);
    out->inventory_category[slot] = (uint16_t) theItem->category;
    out->inventory_kind[slot] = bridge_observed_item_kind(theItem);
    out->inventory_quantity[slot] = theItem->quantity;
    out->inventory_flags[slot] = (uint64_t) (theItem->flags & BRH_OBS_ITEM_FLAGS);
    out->inventory_enchant1[slot] = bridge_observed_enchant(theItem, theItem->enchant1);
    out->inventory_enchant2[slot] = bridge_observed_enchant(theItem, theItem->enchant2);
    out->inventory_charges[slot] = bridge_observed_charges(theItem);
}

static void bridge_store_monster(brh_observation *out, const creature *monst) {
    int cellIndex = bridge_map_index(monst->loc.x, monst->loc.y);

    out->map_has_monster[cellIndex] = 1;
    out->map_monster_kind[cellIndex] = monst->info.monsterID;
    out->map_monster_hp[cellIndex] = monst->currentHP;
    out->map_monster_state[cellIndex] = monst->creatureState;
}

static uint8_t bridge_color_component(char component) {
    int value = (int) component;

    if (value <= 0) {
        return 0;
    }
    if (value >= 100) {
        return UINT8_MAX;
    }
    return (uint8_t) ((value * UINT8_MAX + 50) / 100);
}

static int16_t bridge_glyph_component(enum displayGlyph glyph) {
    if (glyph < 0 || glyph > INT16_MAX) {
        return 0;
    }
    return (int16_t) glyph;
}

static uint32_t bridge_char_component(enum displayGlyph glyph) {
    if (glyph == 0) {
        return 0;
    }
    return glyphToUnicode(glyph);
}
