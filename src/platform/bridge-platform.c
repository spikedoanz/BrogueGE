#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "platform.h"
#include "GlobalsBase.h"

#define BRH_ABI_VERSION 8
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
#define BRH_OBSERVATION_MASK_SCREEN ((uint64_t) 1U << 0)
#define BRH_OBSERVATION_MASK_SEMANTIC_VISIBLE ((uint64_t) 1U << 1)
#define BRH_OBSERVATION_MASK_INVENTORY ((uint64_t) 1U << 2)
#define BRH_OBSERVATION_MASK_FULL (BRH_OBSERVATION_MASK_SCREEN \
                                   | BRH_OBSERVATION_MASK_SEMANTIC_VISIBLE \
                                   | BRH_OBSERVATION_MASK_INVENTORY)
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

typedef struct brh_observation {
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
} brh_observation;

typedef struct brh_observation_buffers {
    int16_t *glyphs;
    uint32_t *chars;
    uint8_t *colors_fg;
    uint8_t *colors_bg;
    uint8_t *specials;
    uint16_t *map_layers;
    uint64_t *map_flags;
    uint16_t *map_volume;
    uint8_t *map_machine;
    int16_t *map_light;
    uint8_t *map_has_item;
    uint16_t *map_item_category;
    int16_t *map_item_kind;
    int16_t *map_item_quantity;
    uint64_t *map_item_flags;
    uint8_t *map_has_monster;
    int16_t *map_monster_kind;
    int16_t *map_monster_hp;
    int16_t *map_monster_state;
    uint8_t *inventory_present;
    uint8_t *inventory_letters;
    uint8_t *inventory_strs;
    uint16_t *inventory_category;
    int16_t *inventory_kind;
    int16_t *inventory_quantity;
    uint64_t *inventory_flags;
    int16_t *inventory_enchant1;
    int16_t *inventory_enchant2;
    int16_t *inventory_charges;
    int64_t *blstats;
    uint8_t *message;
    uint64_t *program_state;
} brh_observation_buffers;

typedef struct brh_observation_view {
    int16_t *glyphs;
    uint32_t *chars;
    uint8_t *colors_fg;
    uint8_t *colors_bg;
    uint8_t *specials;
    uint16_t *map_layers;
    uint64_t *map_flags;
    uint16_t *map_volume;
    uint8_t *map_machine;
    int16_t *map_light;
    uint8_t *map_has_item;
    uint16_t *map_item_category;
    int16_t *map_item_kind;
    int16_t *map_item_quantity;
    uint64_t *map_item_flags;
    uint8_t *map_has_monster;
    int16_t *map_monster_kind;
    int16_t *map_monster_hp;
    int16_t *map_monster_state;
    uint8_t *inventory_present;
    uint8_t *inventory_letters;
    uint8_t *inventory_strs;
    uint16_t *inventory_category;
    int16_t *inventory_kind;
    int16_t *inventory_quantity;
    uint64_t *inventory_flags;
    int16_t *inventory_enchant1;
    int16_t *inventory_enchant2;
    int16_t *inventory_charges;
    int64_t *blstats;
    uint8_t *message;
    uint64_t *program_state;
    uint64_t observationMask;
} brh_observation_view;

int brh_reset(uint64_t seed, brh_observation *out);
int brh_step(long key, int control, int shift, brh_observation *out);
int brh_reset_registered(uint64_t seed);
int brh_step_registered(long key, int control, int shift);
uint64_t brh_supported_observation_mask(void);
int brh_register_observation_buffers(const brh_observation_buffers *buffers, uint64_t fieldMask);
void brh_clear_observation_buffers(void);
uint32_t brh_abi_version(void);
size_t brh_observation_size(void);
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

struct brogueConsole currentConsole;
char dataDirectory[BROGUE_FILENAME_MAX] = STRINGIFY(DATADIR);
boolean serverMode = false;
boolean nonInteractivePlayback = false;
boolean hasGraphics = false;
enum graphicsModes graphicsMode = TEXT_GRAPHICS;

static pthread_mutex_t bridgeMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t bridgeCond = PTHREAD_COND_INITIALIZER;
static pthread_t bridgeThread;
static boolean bridgeThreadStarted = false;
static boolean bridgeWaitingForAction = false;
static boolean bridgeActionReady = false;
static boolean bridgeCloseRequested = false;
static boolean bridgeExited = false;
static boolean bridgeActiveControl = false;
static boolean bridgeActiveShift = false;
static int bridgeExitCode = 0;
static int bridgeLastStepStatus = BRH_STEP_OK;
static char bridgeLastError[256] = "";
static rogueEvent bridgePendingEvent = {0};
static brh_observation bridgeLastObservation = {0};
static brh_observation_buffers bridgeRegisteredBuffers = {0};
static uint64_t bridgeRegisteredObservationMask = 0;
static boolean bridgeRegisteredBuffersConfigured = false;
static boolean bridgeUseRegisteredObservation = false;
static int64_t bridgeEndScore = 0;
static boolean bridgeEndWon = false;

static int bridge_reset_common(uint64_t seed, brh_observation *out, boolean useRegistered);
static int bridge_step_common(long key,
                              int control,
                              int shift,
                              brh_observation *out,
                              boolean useRegistered);
static void *bridge_thread_main(void *unused);
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
static void bridge_fill_observation(brh_observation_view *out);
static void bridge_clear_enabled_observation(brh_observation_view *out);
static void bridge_fill_unknown_semantics(brh_observation_view *out);
static void bridge_fill_map_semantics(brh_observation_view *out);
static void bridge_fill_item_semantics(brh_observation_view *out);
static void bridge_fill_monster_semantics(brh_observation_view *out);
static void bridge_fill_inventory_semantics(brh_observation_view *out);
static void bridge_set_error(const char *message);
static void bridge_set_error_locked(const char *message);
static int bridge_wait_for_observation_locked(boolean allowExit);
static void bridge_copy_observation(brh_observation *to, const brh_observation *from);
static void bridge_fill_current_observation_locked(void);
static void bridge_patch_terminal_observation_locked(void);
static void bridge_initialize_observation_view_from_struct(brh_observation_view *view,
                                                           brh_observation *observation,
                                                           uint64_t observationMask);
static void bridge_initialize_observation_view_from_buffers(brh_observation_view *view,
                                                            const brh_observation_buffers *buffers,
                                                            uint64_t observationMask);
static boolean bridge_validate_observation_buffers_locked(const brh_observation_buffers *buffers,
                                                          uint64_t observationMask);
static void bridge_initialize_launch_state(uint64_t seed);
static int bridge_append_message_line(brh_observation_view *out, int offset, const char *line);
static int bridge_copy_plain_text(uint8_t *target, int targetLength, const char *source);
static int bridge_map_index(short x, short y);
static boolean bridge_loc_is_in_map(pos loc);
static int bridge_inventory_slot(const brh_observation_view *out, char letter);
static int16_t bridge_observed_item_kind(const item *theItem);
static int16_t bridge_observed_remembered_item_kind(uint16_t category, short kind);
static int16_t bridge_observed_enchant(const item *theItem, short enchant);
static int16_t bridge_observed_charges(const item *theItem);
static void bridge_store_map_item(brh_observation_view *out, int cellIndex, const item *theItem);
static void bridge_store_inventory_item(brh_observation_view *out, int slot, item *theItem);
static void bridge_store_monster(brh_observation_view *out, const creature *monst);
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
    return bridge_reset_common(seed, out, false);
}

int brh_step(long key, int control, int shift, brh_observation *out) {
    return bridge_step_common(key, control, shift, out, false);
}

int brh_reset_registered(uint64_t seed) {
    return bridge_reset_common(seed, NULL, true);
}

int brh_step_registered(long key, int control, int shift) {
    return bridge_step_common(key, control, shift, NULL, true);
}

uint64_t brh_supported_observation_mask(void) {
    return BRH_OBSERVATION_MASK_FULL;
}

int brh_register_observation_buffers(const brh_observation_buffers *buffers, uint64_t fieldMask) {
    pthread_mutex_lock(&bridgeMutex);
    if (!bridge_validate_observation_buffers_locked(buffers, fieldMask)) {
        pthread_mutex_unlock(&bridgeMutex);
        return -1;
    }
    bridgeRegisteredBuffers = *buffers;
    bridgeRegisteredObservationMask = fieldMask;
    bridgeRegisteredBuffersConfigured = true;
    pthread_mutex_unlock(&bridgeMutex);
    return 0;
}

void brh_clear_observation_buffers(void) {
    pthread_mutex_lock(&bridgeMutex);
    memset(&bridgeRegisteredBuffers, 0, sizeof(bridgeRegisteredBuffers));
    bridgeRegisteredObservationMask = 0;
    bridgeRegisteredBuffersConfigured = false;
    bridgeUseRegisteredObservation = false;
    pthread_mutex_unlock(&bridgeMutex);
}

static int bridge_reset_common(uint64_t seed, brh_observation *out, boolean useRegistered) {
    int rc;
    pthread_attr_t threadAttr;

    if (!useRegistered && out == NULL) {
        bridge_set_error("observation pointer must not be null");
        return -1;
    }

    brh_close();

    pthread_mutex_lock(&bridgeMutex);
    bridgeLastError[0] = '\0';
    bridgeWaitingForAction = false;
    bridgeActionReady = false;
    bridgeCloseRequested = false;
    bridgeExited = false;
    bridgeExitCode = 0;
    bridgeLastStepStatus = BRH_STEP_OK;
    bridgeActiveControl = false;
    bridgeActiveShift = false;
    bridgeUseRegisteredObservation = useRegistered;
    if (useRegistered && !bridgeRegisteredBuffersConfigured) {
        bridgeUseRegisteredObservation = false;
        bridge_set_error_locked("observation buffers are not registered");
        pthread_mutex_unlock(&bridgeMutex);
        return -1;
    }
    memset(&bridgeLastObservation, 0, sizeof(bridgeLastObservation));
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

    rc = pthread_attr_init(&threadAttr);
    if (rc != 0) {
        bridgeThreadStarted = false;
        bridge_set_error_locked("failed to initialize Brogue bridge thread attributes");
        pthread_mutex_unlock(&bridgeMutex);
        return -1;
    }
    rc = pthread_attr_setstacksize(&threadAttr, BRH_THREAD_STACK_SIZE);
    if (rc != 0) {
        pthread_attr_destroy(&threadAttr);
        bridgeThreadStarted = false;
        bridge_set_error_locked("failed to configure Brogue bridge thread stack");
        pthread_mutex_unlock(&bridgeMutex);
        return -1;
    }
    rc = pthread_create(&bridgeThread, &threadAttr, bridge_thread_main, NULL);
    pthread_attr_destroy(&threadAttr);
    if (rc != 0) {
        bridgeThreadStarted = false;
        bridge_set_error_locked("failed to start Brogue bridge thread");
        pthread_mutex_unlock(&bridgeMutex);
        return -1;
    }
    bridgeThreadStarted = true;

    rc = bridge_wait_for_observation_locked(false);
    if (rc == 0 && !useRegistered) {
        bridge_copy_observation(out, &bridgeLastObservation);
    }
    pthread_mutex_unlock(&bridgeMutex);
    return rc;
}

static int bridge_step_common(long key,
                              int control,
                              int shift,
                              brh_observation *out,
                              boolean useRegistered) {
    int rc;

    if (!useRegistered && out == NULL) {
        bridge_set_error("observation pointer must not be null");
        return -1;
    }

    pthread_mutex_lock(&bridgeMutex);
    if (!bridgeThreadStarted || bridgeExited) {
        bridge_set_error_locked("Brogue bridge is not running");
        pthread_mutex_unlock(&bridgeMutex);
        return -1;
    }
    if (!bridgeWaitingForAction) {
        bridge_set_error_locked("Brogue bridge is not waiting for an action");
        pthread_mutex_unlock(&bridgeMutex);
        return -1;
    }
    if (bridgeUseRegisteredObservation != useRegistered) {
        bridge_set_error_locked(
            useRegistered
                ? "Brogue bridge was not reset with registered observation buffers"
                : "Brogue bridge was reset with registered observation buffers");
        pthread_mutex_unlock(&bridgeMutex);
        return -1;
    }
    if (useRegistered && !bridgeRegisteredBuffersConfigured) {
        bridge_set_error_locked("observation buffers are not registered");
        pthread_mutex_unlock(&bridgeMutex);
        return -1;
    }

    bridgePendingEvent.eventType = KEYSTROKE;
    bridgePendingEvent.param1 = key;
    bridgePendingEvent.param2 = 0;
    bridgePendingEvent.controlKey = control ? true : false;
    bridgePendingEvent.shiftKey = shift ? true : false;
    bridgeLastStepStatus = BRH_STEP_OK;
    bridgeActionReady = true;
    bridgeWaitingForAction = false;
    pthread_cond_broadcast(&bridgeCond);

    rc = bridge_wait_for_observation_locked(true);
    if (rc == 0 && !useRegistered) {
        bridge_copy_observation(out, &bridgeLastObservation);
    }
    if (rc == 0) {
        rc = bridgeLastStepStatus;
    }
    pthread_mutex_unlock(&bridgeMutex);
    return rc;
}

uint32_t brh_abi_version(void) {
    return BRH_ABI_VERSION;
}

size_t brh_observation_size(void) {
    return sizeof(brh_observation);
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
    boolean shouldJoin;

    pthread_mutex_lock(&bridgeMutex);
    shouldJoin = bridgeThreadStarted;
    if (shouldJoin) {
        bridgeCloseRequested = true;
        bridgeActionReady = true;
        pthread_cond_broadcast(&bridgeCond);
    }
    pthread_mutex_unlock(&bridgeMutex);

    if (shouldJoin) {
        pthread_join(bridgeThread, NULL);
        pthread_mutex_lock(&bridgeMutex);
        bridgeThreadStarted = false;
        bridgeWaitingForAction = false;
        bridgeActionReady = false;
        bridgeCloseRequested = false;
        bridgeUseRegisteredObservation = false;
        pthread_mutex_unlock(&bridgeMutex);
    }
}

void brh_set_data_dir(const char *path) {
    if (path == NULL || path[0] == '\0') {
        return;
    }

    pthread_mutex_lock(&bridgeMutex);
    strncpy(dataDirectory, path, BROGUE_FILENAME_MAX - 1);
    dataDirectory[BROGUE_FILENAME_MAX - 1] = '\0';
    pthread_mutex_unlock(&bridgeMutex);
}

const char *brh_last_error(void) {
    return bridgeLastError;
}

void brh_mark_invalid_key(void) {
    pthread_mutex_lock(&bridgeMutex);
    if (bridgeThreadStarted && !bridgeExited) {
        bridgeLastStepStatus = BRH_STEP_INVALID_KEY;
    }
    pthread_mutex_unlock(&bridgeMutex);
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

static void *bridge_thread_main(void *unused) {
    int exitCode;

    (void) unused;
    exitCode = rogueMain();

    pthread_mutex_lock(&bridgeMutex);
    bridgeExitCode = exitCode;
    bridgeExited = true;
    bridgeWaitingForAction = false;
    /*
     * rogueMain() frees level-owned lists before returning. Keep the last
     * observation captured at an input boundary instead of walking freed
     * terrain/item/monster globals during shutdown.
     */
    bridge_patch_terminal_observation_locked();
    pthread_cond_broadcast(&bridgeCond);
    pthread_mutex_unlock(&bridgeMutex);
    return NULL;
}

static void bridge_gameLoop(void) {
    (void) rogueMain();
}

static boolean bridge_pauseForMilliseconds(short milliseconds, PauseBehavior behavior) {
    (void) milliseconds;
    (void) behavior;
    return false;
}

static void bridge_nextKeyOrMouseEvent(rogueEvent *returnEvent,
                                       boolean textInput,
                                       boolean colorsDance) {
    (void) textInput;
    (void) colorsDance;

    pthread_mutex_lock(&bridgeMutex);
    bridge_fill_current_observation_locked();
    bridgeWaitingForAction = true;
    bridgeActionReady = false;
    pthread_cond_broadcast(&bridgeCond);

    while (!bridgeActionReady && !bridgeCloseRequested) {
        pthread_cond_wait(&bridgeCond, &bridgeMutex);
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
        pthread_mutex_unlock(&bridgeMutex);
        return;
    }

    *returnEvent = bridgePendingEvent;
    bridgeActiveControl = bridgePendingEvent.controlKey;
    bridgeActiveShift = bridgePendingEvent.shiftKey;
    bridgeActionReady = false;
    pthread_mutex_unlock(&bridgeMutex);
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

static void bridge_fill_observation(brh_observation_view *out) {
    int x;
    int y;
    int messageOffset = 0;

    bridge_clear_enabled_observation(out);

    if (out->observationMask & BRH_OBSERVATION_MASK_SCREEN) {
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
    }

    if (out->observationMask & BRH_OBSERVATION_MASK_SEMANTIC_VISIBLE) {
        bridge_fill_map_semantics(out);
        bridge_fill_item_semantics(out);
        bridge_fill_monster_semantics(out);
    }
    if (out->observationMask & BRH_OBSERVATION_MASK_INVENTORY) {
        bridge_fill_inventory_semantics(out);
    }
}

static void bridge_clear_enabled_observation(brh_observation_view *out) {
    if (out->observationMask & BRH_OBSERVATION_MASK_SCREEN) {
        memset(out->glyphs, 0, BRH_OBS_CELLS * sizeof(out->glyphs[0]));
        memset(out->chars, 0, BRH_OBS_CELLS * sizeof(out->chars[0]));
        memset(out->colors_fg, 0, BRH_COLOR_CELLS * sizeof(out->colors_fg[0]));
        memset(out->colors_bg, 0, BRH_COLOR_CELLS * sizeof(out->colors_bg[0]));
        memset(out->specials, 0, BRH_OBS_CELLS * sizeof(out->specials[0]));
        memset(out->blstats, 0, BRH_BLSTATS_SIZE * sizeof(out->blstats[0]));
        memset(out->message, 0, BRH_MESSAGE_SIZE * sizeof(out->message[0]));
        memset(out->program_state, 0, BRH_PROGRAM_STATE_SIZE * sizeof(out->program_state[0]));
    }
    if (out->observationMask & BRH_OBSERVATION_MASK_SEMANTIC_VISIBLE) {
        memset(out->map_layers, 0, BRH_MAP_LAYER_CELLS * sizeof(out->map_layers[0]));
        memset(out->map_flags, 0, BRH_MAP_CELLS * sizeof(out->map_flags[0]));
        memset(out->map_volume, 0, BRH_MAP_CELLS * sizeof(out->map_volume[0]));
        memset(out->map_machine, 0, BRH_MAP_CELLS * sizeof(out->map_machine[0]));
        memset(out->map_light, 0, BRH_MAP_COLOR_CELLS * sizeof(out->map_light[0]));
        memset(out->map_has_item, 0, BRH_MAP_CELLS * sizeof(out->map_has_item[0]));
        memset(out->map_item_category, 0, BRH_MAP_CELLS * sizeof(out->map_item_category[0]));
        memset(out->map_item_flags, 0, BRH_MAP_CELLS * sizeof(out->map_item_flags[0]));
        memset(out->map_has_monster, 0, BRH_MAP_CELLS * sizeof(out->map_has_monster[0]));
    }
    if (out->observationMask & BRH_OBSERVATION_MASK_INVENTORY) {
        memset(out->inventory_present, 0, BRH_INVENTORY_SIZE * sizeof(out->inventory_present[0]));
        memset(out->inventory_letters, 0, BRH_INVENTORY_SIZE * sizeof(out->inventory_letters[0]));
        memset(out->inventory_strs, 0, BRH_INVENTORY_STR_CELLS * sizeof(out->inventory_strs[0]));
        memset(out->inventory_category, 0, BRH_INVENTORY_SIZE * sizeof(out->inventory_category[0]));
        memset(out->inventory_flags, 0, BRH_INVENTORY_SIZE * sizeof(out->inventory_flags[0]));
    }
    bridge_fill_unknown_semantics(out);
}

static void bridge_fill_unknown_semantics(brh_observation_view *out) {
    int i;

    if (out->observationMask & BRH_OBSERVATION_MASK_SEMANTIC_VISIBLE) {
        for (i = 0; i < BRH_MAP_CELLS; i++) {
            out->map_item_kind[i] = BRH_UNKNOWN_SHORT;
            out->map_item_quantity[i] = BRH_UNKNOWN_SHORT;
            out->map_monster_kind[i] = BRH_UNKNOWN_SHORT;
            out->map_monster_hp[i] = BRH_UNKNOWN_SHORT;
            out->map_monster_state[i] = BRH_UNKNOWN_SHORT;
        }
    }
    if (out->observationMask & BRH_OBSERVATION_MASK_INVENTORY) {
        for (i = 0; i < BRH_INVENTORY_SIZE; i++) {
            out->inventory_kind[i] = BRH_UNKNOWN_SHORT;
            out->inventory_quantity[i] = BRH_UNKNOWN_SHORT;
            out->inventory_enchant1[i] = BRH_UNKNOWN_SHORT;
            out->inventory_enchant2[i] = BRH_UNKNOWN_SHORT;
            out->inventory_charges[i] = BRH_UNKNOWN_SHORT;
        }
    }
}

static void bridge_fill_map_semantics(brh_observation_view *out) {
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

static void bridge_fill_item_semantics(brh_observation_view *out) {
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

static void bridge_fill_monster_semantics(brh_observation_view *out) {
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

static void bridge_fill_inventory_semantics(brh_observation_view *out) {
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
    pthread_mutex_lock(&bridgeMutex);
    bridge_set_error_locked(message);
    pthread_mutex_unlock(&bridgeMutex);
}

static void bridge_set_error_locked(const char *message) {
    strncpy(bridgeLastError, message, sizeof(bridgeLastError) - 1);
    bridgeLastError[sizeof(bridgeLastError) - 1] = '\0';
}

static int bridge_wait_for_observation_locked(boolean allowExit) {
    while (!bridgeWaitingForAction && !bridgeExited) {
        pthread_cond_wait(&bridgeCond, &bridgeMutex);
    }
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
    return 0;
}

static void bridge_copy_observation(brh_observation *to, const brh_observation *from) {
    memcpy(to, from, sizeof(*to));
}

static void bridge_fill_current_observation_locked(void) {
    brh_observation_view view;

    if (bridgeUseRegisteredObservation) {
        bridge_initialize_observation_view_from_buffers(&view,
                                                        &bridgeRegisteredBuffers,
                                                        bridgeRegisteredObservationMask);
    } else {
        bridge_initialize_observation_view_from_struct(&view,
                                                       &bridgeLastObservation,
                                                       BRH_OBSERVATION_MASK_FULL);
    }
    bridge_fill_observation(&view);
}

static void bridge_patch_terminal_observation_locked(void) {
    uint64_t *programState;

    if (bridgeUseRegisteredObservation && bridgeRegisteredBuffersConfigured) {
        programState = bridgeRegisteredBuffers.program_state;
    } else {
        programState = bridgeLastObservation.program_state;
    }
    if (programState == NULL) {
        return;
    }
    programState[1] = 1;
    programState[2] = bridgeEndWon ? 1 : 0;
    programState[6] = (uint64_t) bridgeEndScore;
}

static void bridge_initialize_observation_view_from_struct(brh_observation_view *view,
                                                           brh_observation *observation,
                                                           uint64_t observationMask) {
    view->glyphs = observation->glyphs;
    view->chars = observation->chars;
    view->colors_fg = observation->colors_fg;
    view->colors_bg = observation->colors_bg;
    view->specials = observation->specials;
    view->map_layers = observation->map_layers;
    view->map_flags = observation->map_flags;
    view->map_volume = observation->map_volume;
    view->map_machine = observation->map_machine;
    view->map_light = observation->map_light;
    view->map_has_item = observation->map_has_item;
    view->map_item_category = observation->map_item_category;
    view->map_item_kind = observation->map_item_kind;
    view->map_item_quantity = observation->map_item_quantity;
    view->map_item_flags = observation->map_item_flags;
    view->map_has_monster = observation->map_has_monster;
    view->map_monster_kind = observation->map_monster_kind;
    view->map_monster_hp = observation->map_monster_hp;
    view->map_monster_state = observation->map_monster_state;
    view->inventory_present = observation->inventory_present;
    view->inventory_letters = observation->inventory_letters;
    view->inventory_strs = observation->inventory_strs;
    view->inventory_category = observation->inventory_category;
    view->inventory_kind = observation->inventory_kind;
    view->inventory_quantity = observation->inventory_quantity;
    view->inventory_flags = observation->inventory_flags;
    view->inventory_enchant1 = observation->inventory_enchant1;
    view->inventory_enchant2 = observation->inventory_enchant2;
    view->inventory_charges = observation->inventory_charges;
    view->blstats = observation->blstats;
    view->message = observation->message;
    view->program_state = observation->program_state;
    view->observationMask = observationMask;
}

static void bridge_initialize_observation_view_from_buffers(brh_observation_view *view,
                                                            const brh_observation_buffers *buffers,
                                                            uint64_t observationMask) {
    view->glyphs = buffers->glyphs;
    view->chars = buffers->chars;
    view->colors_fg = buffers->colors_fg;
    view->colors_bg = buffers->colors_bg;
    view->specials = buffers->specials;
    view->map_layers = buffers->map_layers;
    view->map_flags = buffers->map_flags;
    view->map_volume = buffers->map_volume;
    view->map_machine = buffers->map_machine;
    view->map_light = buffers->map_light;
    view->map_has_item = buffers->map_has_item;
    view->map_item_category = buffers->map_item_category;
    view->map_item_kind = buffers->map_item_kind;
    view->map_item_quantity = buffers->map_item_quantity;
    view->map_item_flags = buffers->map_item_flags;
    view->map_has_monster = buffers->map_has_monster;
    view->map_monster_kind = buffers->map_monster_kind;
    view->map_monster_hp = buffers->map_monster_hp;
    view->map_monster_state = buffers->map_monster_state;
    view->inventory_present = buffers->inventory_present;
    view->inventory_letters = buffers->inventory_letters;
    view->inventory_strs = buffers->inventory_strs;
    view->inventory_category = buffers->inventory_category;
    view->inventory_kind = buffers->inventory_kind;
    view->inventory_quantity = buffers->inventory_quantity;
    view->inventory_flags = buffers->inventory_flags;
    view->inventory_enchant1 = buffers->inventory_enchant1;
    view->inventory_enchant2 = buffers->inventory_enchant2;
    view->inventory_charges = buffers->inventory_charges;
    view->blstats = buffers->blstats;
    view->message = buffers->message;
    view->program_state = buffers->program_state;
    view->observationMask = observationMask;
}

#define BRH_REQUIRE_REGISTERED_BUFFER(buffers, field) \
    do { \
        if ((buffers)->field == NULL) { \
            bridge_set_error_locked("registered observation buffer is missing field: " #field); \
            return false; \
        } \
    } while (0)

static boolean bridge_validate_observation_buffers_locked(const brh_observation_buffers *buffers,
                                                          uint64_t observationMask) {
    if (buffers == NULL) {
        bridge_set_error_locked("observation buffers pointer must not be null");
        return false;
    }
    if (observationMask == 0) {
        bridge_set_error_locked("observation mask must not be zero");
        return false;
    }
    if (observationMask & ~BRH_OBSERVATION_MASK_FULL) {
        bridge_set_error_locked("observation mask contains unsupported fields");
        return false;
    }
    if (observationMask & BRH_OBSERVATION_MASK_SCREEN) {
        BRH_REQUIRE_REGISTERED_BUFFER(buffers, glyphs);
        BRH_REQUIRE_REGISTERED_BUFFER(buffers, chars);
        BRH_REQUIRE_REGISTERED_BUFFER(buffers, colors_fg);
        BRH_REQUIRE_REGISTERED_BUFFER(buffers, colors_bg);
        BRH_REQUIRE_REGISTERED_BUFFER(buffers, specials);
        BRH_REQUIRE_REGISTERED_BUFFER(buffers, blstats);
        BRH_REQUIRE_REGISTERED_BUFFER(buffers, message);
        BRH_REQUIRE_REGISTERED_BUFFER(buffers, program_state);
    }
    if (observationMask & BRH_OBSERVATION_MASK_SEMANTIC_VISIBLE) {
        BRH_REQUIRE_REGISTERED_BUFFER(buffers, map_layers);
        BRH_REQUIRE_REGISTERED_BUFFER(buffers, map_flags);
        BRH_REQUIRE_REGISTERED_BUFFER(buffers, map_volume);
        BRH_REQUIRE_REGISTERED_BUFFER(buffers, map_machine);
        BRH_REQUIRE_REGISTERED_BUFFER(buffers, map_light);
        BRH_REQUIRE_REGISTERED_BUFFER(buffers, map_has_item);
        BRH_REQUIRE_REGISTERED_BUFFER(buffers, map_item_category);
        BRH_REQUIRE_REGISTERED_BUFFER(buffers, map_item_kind);
        BRH_REQUIRE_REGISTERED_BUFFER(buffers, map_item_quantity);
        BRH_REQUIRE_REGISTERED_BUFFER(buffers, map_item_flags);
        BRH_REQUIRE_REGISTERED_BUFFER(buffers, map_has_monster);
        BRH_REQUIRE_REGISTERED_BUFFER(buffers, map_monster_kind);
        BRH_REQUIRE_REGISTERED_BUFFER(buffers, map_monster_hp);
        BRH_REQUIRE_REGISTERED_BUFFER(buffers, map_monster_state);
    }
    if (observationMask & BRH_OBSERVATION_MASK_INVENTORY) {
        BRH_REQUIRE_REGISTERED_BUFFER(buffers, inventory_present);
        BRH_REQUIRE_REGISTERED_BUFFER(buffers, inventory_letters);
        BRH_REQUIRE_REGISTERED_BUFFER(buffers, inventory_strs);
        BRH_REQUIRE_REGISTERED_BUFFER(buffers, inventory_category);
        BRH_REQUIRE_REGISTERED_BUFFER(buffers, inventory_kind);
        BRH_REQUIRE_REGISTERED_BUFFER(buffers, inventory_quantity);
        BRH_REQUIRE_REGISTERED_BUFFER(buffers, inventory_flags);
        BRH_REQUIRE_REGISTERED_BUFFER(buffers, inventory_enchant1);
        BRH_REQUIRE_REGISTERED_BUFFER(buffers, inventory_enchant2);
        BRH_REQUIRE_REGISTERED_BUFFER(buffers, inventory_charges);
    }
    return true;
}

static void bridge_initialize_launch_state(uint64_t seed) {
    memset((void *) &rogue, 0, sizeof(playerCharacter));
    rogue.nextGame = seed == 0 ? NG_NEW_GAME : NG_NEW_GAME_WITH_SEED;
    rogue.nextGameSeed = seed;
    rogue.mode = GAME_MODE_NORMAL;
    rogue.displayStealthRangeMode = false;
    rogue.trueColorMode = false;
}

static int bridge_append_message_line(brh_observation_view *out, int offset, const char *line) {
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

static int bridge_inventory_slot(const brh_observation_view *out, char letter) {
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

static void bridge_store_map_item(brh_observation_view *out, int cellIndex, const item *theItem) {
    out->map_has_item[cellIndex] = 1;
    out->map_item_category[cellIndex] = (uint16_t) theItem->category;
    out->map_item_kind[cellIndex] = bridge_observed_item_kind(theItem);
    out->map_item_quantity[cellIndex] = theItem->quantity;
    out->map_item_flags[cellIndex] = (uint64_t) (theItem->flags & BRH_OBS_ITEM_FLAGS);
}

static void bridge_store_inventory_item(brh_observation_view *out, int slot, item *theItem) {
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

static void bridge_store_monster(brh_observation_view *out, const creature *monst) {
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
