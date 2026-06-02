#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "platform.h"
#include "GlobalsBase.h"

#define HEADLESS_DEFAULT_DUMP_DIR "headless-screens"

static char dumpDirectory[BROGUE_FILENAME_MAX];
static unsigned long frameNumber = 0;
static boolean dumpDirectoryReady = false;

static void headless_gameLoop(void);
static boolean headless_pauseForMilliseconds(short milliseconds, PauseBehavior behavior);
static void headless_nextKeyOrMouseEvent(rogueEvent *returnEvent, boolean textInput, boolean colorsDance);
static void headless_plotChar(enum displayGlyph inputChar,
                              short xLoc, short yLoc,
                              short foreRed, short foreGreen, short foreBlue,
                              short backRed, short backGreen, short backBlue);
static void headless_remap(const char *input_name, const char *output_name);
static boolean headless_modifierHeld(int modifier);
static void headless_notifyEvent(short eventId, int data1, int data2, const char *str1, const char *str2);
static void ensureDumpDirectory(void);
static void dumpScreen(const char *reason);
static void writeUtf8(FILE *file, unsigned int codepoint);
static boolean keyIsForbiddenInHeadless(int input);

static void headless_gameLoop(void) {
    ensureDumpDirectory();
    exit(rogueMain());
}

static boolean headless_pauseForMilliseconds(short milliseconds, PauseBehavior behavior) {
    (void) milliseconds;
    (void) behavior;

    // Headless runs should never wait on cosmetic animation timing.
    return false;
}

static void headless_nextKeyOrMouseEvent(rogueEvent *returnEvent, boolean textInput, boolean colorsDance) {
    int input;

    (void) textInput;
    (void) colorsDance;

    // Dump exactly when Brogue has reached an input boundary. For now this is our crude but
    // useful "frame after each action" signal.
    dumpScreen("input");

    do {
        input = getchar();

        // A driving script can close stdin when it only wants the latest dumped screen.
        if (input == EOF) {
            exit(0);
        }

        if (keyIsForbiddenInHeadless(input)) {
            fprintf(stderr, "Ignoring forbidden headless key: %c\n", input);
        }
    } while (input == '\n' || input == '\r' || keyIsForbiddenInHeadless(input));

    returnEvent->eventType = KEYSTROKE;
    returnEvent->param1 = input;
    returnEvent->param2 = 0;
    returnEvent->controlKey = false;
    returnEvent->shiftKey = false;
}

static void headless_plotChar(enum displayGlyph inputChar,
                              short xLoc, short yLoc,
                              short foreRed, short foreGreen, short foreBlue,
                              short backRed, short backGreen, short backBlue) {
    (void) inputChar;
    (void) xLoc;
    (void) yLoc;
    (void) foreRed;
    (void) foreGreen;
    (void) foreBlue;
    (void) backRed;
    (void) backGreen;
    (void) backBlue;

    // The canonical screen state already lives in displayBuffer. We dump from that buffer so the
    // text files match Brogue's own draw coalescing, not the platform callback order.
}

static void headless_remap(const char *input_name, const char *output_name) {
    (void) input_name;
    (void) output_name;
}

static boolean headless_modifierHeld(int modifier) {
    (void) modifier;
    return false;
}

static void headless_notifyEvent(short eventId, int data1, int data2, const char *str1, const char *str2) {
    (void) eventId;
    (void) data1;
    (void) data2;
    (void) str1;
    (void) str2;

    dumpScreen("event");
}

static void ensureDumpDirectory(void) {
    const char *configuredDirectory;

    if (dumpDirectoryReady) {
        return;
    }

    configuredDirectory = getenv("BRUHOGUE_HEADLESS_DUMP_DIR");
    if (configuredDirectory != NULL && configuredDirectory[0] != '\0') {
        strncpy(dumpDirectory, configuredDirectory, BROGUE_FILENAME_MAX - 1);
        dumpDirectory[BROGUE_FILENAME_MAX - 1] = '\0';
    } else {
        strncpy(dumpDirectory, HEADLESS_DEFAULT_DUMP_DIR, BROGUE_FILENAME_MAX - 1);
        dumpDirectory[BROGUE_FILENAME_MAX - 1] = '\0';
    }

    if (mkdir(dumpDirectory, 0777) && errno != EEXIST) {
        fprintf(stderr, "Could not create headless dump directory '%s': %s\n", dumpDirectory, strerror(errno));
        exit(1);
    }

    dumpDirectoryReady = true;
}

static void dumpScreen(const char *reason) {
    char path[BROGUE_FILENAME_MAX];
    FILE *file;
    short x, y;

    ensureDumpDirectory();

    snprintf(path, BROGUE_FILENAME_MAX, "%s/screen_%06lu_%s.txt", dumpDirectory, frameNumber++, reason);
    file = fopen(path, "w");
    if (file == NULL) {
        fprintf(stderr, "Could not write headless screen dump '%s': %s\n", path, strerror(errno));
        exit(1);
    }

    for (y = 0; y < ROWS; y++) {
        for (x = 0; x < COLS; x++) {
            enum displayGlyph glyph = displayBuffer.cells[x][y].character;
            if (glyph == 0) {
                fputc(' ', file);
            } else {
                writeUtf8(file, glyphToUnicode(glyph));
            }
        }
        fputc('\n', file);
    }

    fclose(file);
}

static boolean keyIsForbiddenInHeadless(int input) {
    // Ban full autopilot at the headless platform boundary. Auto-explore is still allowed because
    // it is a normal Brogue command and stops on danger; the Python runner handles death early-exit.
    return input == AUTOPLAY_KEY;
}

static void writeUtf8(FILE *file, unsigned int codepoint) {
    if (codepoint <= 0x7F) {
        fputc((int) codepoint, file);
    } else if (codepoint <= 0x7FF) {
        fputc((int) (0xC0 | (codepoint >> 6)), file);
        fputc((int) (0x80 | (codepoint & 0x3F)), file);
    } else if (codepoint <= 0xFFFF) {
        fputc((int) (0xE0 | (codepoint >> 12)), file);
        fputc((int) (0x80 | ((codepoint >> 6) & 0x3F)), file);
        fputc((int) (0x80 | (codepoint & 0x3F)), file);
    } else {
        fputc('?', file);
    }
}

struct brogueConsole headlessConsole = {
    headless_gameLoop,
    headless_pauseForMilliseconds,
    headless_nextKeyOrMouseEvent,
    headless_plotChar,
    headless_remap,
    headless_modifierHeld,
    headless_notifyEvent,
    NULL,
    NULL
};
