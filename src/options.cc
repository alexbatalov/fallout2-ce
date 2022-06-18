#include "options.h"

#include "art.h"
#include "color.h"
#include "combat.h"
#include "combat_ai.h"
#include "core.h"
#include "cycle.h"
#include "debug.h"
#include "draw.h"
#include "game.h"
#include "game_config.h"
#include "game_mouse.h"
#include "game_sound.h"
#include "geometry.h"
#include "grayscale.h"
#include "loadsave.h"
#include "memory.h"
#include "message.h"
#include "platform_compat.h"
#include "scripts.h"
#include "text_font.h"
#include "text_object.h"
#include "tile.h"
#include "window_manager.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <algorithm>

#define PREFERENCES_WINDOW_WIDTH 640
#define PREFERENCES_WINDOW_HEIGHT 480

#define OPTIONS_WINDOW_BUTTONS_COUNT (10)
#define PRIMARY_OPTION_VALUE_COUNT (4)
#define SECONDARY_OPTION_VALUE_COUNT (2)

typedef enum Preference {
    PREF_GAME_DIFFICULTY,
    PREF_COMBAT_DIFFICULTY,
    PREF_VIOLENCE_LEVEL,
    PREF_TARGET_HIGHLIGHT,
    PREF_COMBAT_LOOKS,
    PREF_COMBAT_MESSAGES,
    PREF_COMBAT_TAUNTS,
    PREF_LANGUAGE_FILTER,
    PREF_RUNNING,
    PREF_SUBTITLES,
    PREF_ITEM_HIGHLIGHT,
    PREF_COMBAT_SPEED,
    PREF_TEXT_BASE_DELAY,
    PREF_MASTER_VOLUME,
    PREF_MUSIC_VOLUME,
    PREF_SFX_VOLUME,
    PREF_SPEECH_VOLUME,
    PREF_BRIGHTNESS,
    PREF_MOUSE_SENSITIVIY,
    PREF_COUNT,
    FIRST_PRIMARY_PREF = PREF_GAME_DIFFICULTY,
    LAST_PRIMARY_PREF = PREF_COMBAT_LOOKS,
    PRIMARY_PREF_COUNT = LAST_PRIMARY_PREF - FIRST_PRIMARY_PREF + 1,
    FIRST_SECONDARY_PREF = PREF_COMBAT_MESSAGES,
    LAST_SECONDARY_PREF = PREF_ITEM_HIGHLIGHT,
    SECONDARY_PREF_COUNT = LAST_SECONDARY_PREF - FIRST_SECONDARY_PREF + 1,
    FIRST_RANGE_PREF = PREF_COMBAT_SPEED,
    LAST_RANGE_PREF = PREF_MOUSE_SENSITIVIY,
    RANGE_PREF_COUNT = LAST_RANGE_PREF - FIRST_RANGE_PREF + 1,
} Preference;

typedef enum PauseWindowFrm {
    PAUSE_WINDOW_FRM_BACKGROUND,
    PAUSE_WINDOW_FRM_DONE_BOX,
    PAUSE_WINDOW_FRM_LITTLE_RED_BUTTON_UP,
    PAUSE_WINDOW_FRM_LITTLE_RED_BUTTON_DOWN,
    PAUSE_WINDOW_FRM_COUNT,
} PauseWindowFrm;

typedef enum OptionsWindowFrm {
    OPTIONS_WINDOW_FRM_BACKGROUND,
    OPTIONS_WINDOW_FRM_BUTTON_ON,
    OPTIONS_WINDOW_FRM_BUTTON_OFF,
    OPTIONS_WINDOW_FRM_COUNT,
} OptionsWindowFrm;

typedef enum PreferencesWindowFrm {
    PREFERENCES_WINDOW_FRM_BACKGROUND,
    // Knob (for range preferences)
    PREFERENCES_WINDOW_FRM_KNOB_OFF,
    // 4-way switch (for primary preferences)
    PREFERENCES_WINDOW_FRM_PRIMARY_SWITCH,
    // 2-way switch (for secondary preferences)
    PREFERENCES_WINDOW_FRM_SECONDARY_SWITCH,
    PREFERENCES_WINDOW_FRM_CHECKBOX_ON,
    PREFERENCES_WINDOW_FRM_CHECKBOX_OFF,
    PREFERENCES_WINDOW_FRM_6,
    // Active knob (for range preferences)
    PREFERENCES_WINDOW_FRM_KNOB_ON,
    PREFERENCES_WINDOW_FRM_LITTLE_RED_BUTTON_UP,
    PREFERENCES_WINDOW_FRM_LITTLE_RED_BUTTON_DOWN,
    PREFERENCES_WINDOW_FRM_COUNT,
} PreferencesWindowFrm;

#pragma pack(2)
typedef struct PreferenceDescription {
    // The number of options.
    short valuesCount;

    // Direction of rotation:
    // 0 - clockwise (incrementing value),
    // 1 - counter-clockwise (decrementing value)
    short direction;
    short knobX;
    short knobY;
    // Min x coordinate of the preference control bounding box.
    short minX;
    // Max x coordinate of the preference control bounding box.
    short maxX;
    short labelIds[PRIMARY_OPTION_VALUE_COUNT];
    int btn;
    char name[32];
    double minValue;
    double maxValue;
    int* valuePtr;
} PreferenceDescription;
#pragma pack()

static int optionsWindowInit();
static int optionsWindowFree();
static void _ShadeScreen(bool a1);
static void _SetSystemPrefs();
static void _SaveSettings();
static void _RestoreSettings();
static void preferencesSetDefaults(bool a1);
static void _JustUpdate_();
static void _UpdateThing(int index);
int _SavePrefs(bool save);
static int preferencesWindowInit();
static int preferencesWindowFree();
static int _do_prefscreen();
static void _DoThing(int eventCode);

// 0x48FBD0
static const int _row1Ytab[PRIMARY_PREF_COUNT] = {
    48,
    125,
    203,
    286,
    363,
};

// 0x48FBDA
static const int _row2Ytab[SECONDARY_PREF_COUNT] = {
    49,
    116,
    181,
    247,
    313,
    380,
};

// 0x48FBE6
static const int _row3Ytab[RANGE_PREF_COUNT] = {
    19,
    94,
    165,
    216,
    268,
    319,
    369,
    420,
};

// x offsets for primary preferences from the knob position
// 0x48FBF6
static const short word_48FBF6[PRIMARY_OPTION_VALUE_COUNT] = {
    2,
    25,
    46,
    46,
};

// y offsets for primary preference option values from the knob position
// 0x48FBFE
static const short word_48FBFE[PRIMARY_OPTION_VALUE_COUNT] = {
    10,
    -4,
    10,
    31,
};

// x offsets for secondary prefrence option values from the knob position
// 0x48FC06
static const short word_48FC06[SECONDARY_OPTION_VALUE_COUNT] = {
    4,
    21,
};

// 0x48FC0C
static const int gPauseWindowFrmIds[PAUSE_WINDOW_FRM_COUNT] = {
    208, // charwin.frm - character editor
    209, // donebox.frm - character editor
    8, // lilredup.frm - little red button up
    9, // lilreddn.frm - little red button down
};

// y offsets for secondary preferences
// 0x48FC30
static const int dword_48FC30[SECONDARY_PREF_COUNT] = {
    66, // combat messages
    133, // combat taunts
    200, // language filter
    264, // running
    331, // subtitles
    397, // item highlight
};

// y offsets for primary preferences
// 0x48FC1C
static const int dword_48FC1C[PRIMARY_PREF_COUNT] = {
    66, // game difficulty
    143, // combat difficulty
    222, // violence level
    304, // target highlight
    382, // combat looks
};

// 0x50C168
static const double dbl_50C168 = 1.17999267578125;

// 0x50C170
static const double dbl_50C170 = 0.01124954223632812;

// 0x50C178
static const double dbl_50C178 = -0.01124954223632812;

// 0x50C180
static const double dbl_50C180 = 1.17999267578125;

// 0x50C2D0
static const double dbl_50C2D0 = -1.0;

// 0x50C2D8
static const double dbl_50C2D8 = 0.2;

// 0x50C2E0
static const double dbl_50C2E0 = 2.0;

// 0x5197C0
static const int gOptionsWindowFrmIds[OPTIONS_WINDOW_FRM_COUNT] = {
    220, // opbase.frm - character editor
    222, // opbtnon.frm - character editor
    221, // opbtnoff.frm - character editor
};

// 0x5197CC
static const int gPreferencesWindowFrmIds[PREFERENCES_WINDOW_FRM_COUNT] = {
    240, // prefscrn.frm - options screen
    241, // prfsldof.frm - options screen
    242, // prfbknbs.frm - options screen
    243, // prflknbs.frm - options screen
    244, // prfxin.frm - options screen
    245, // prfxout.frm - options screen
    246, // prefcvr.frm - options screen
    247, // prfsldon.frm - options screen
    8, // lilredup.frm - little red button up
    9, // lilreddn.frm - little red button down
};

// 0x5197F8
static PreferenceDescription gPreferenceDescriptions[PREF_COUNT] = {
    { 3, 0, 76, 71, 0, 0, { 203, 204, 205, 0 }, 0, GAME_CONFIG_GAME_DIFFICULTY_KEY, 0, 0, &gPreferencesGameDifficulty1 },
    { 3, 0, 76, 149, 0, 0, { 206, 204, 208, 0 }, 0, GAME_CONFIG_COMBAT_DIFFICULTY_KEY, 0, 0, &gPreferencesCombatDifficulty1 },
    { 4, 0, 76, 226, 0, 0, { 214, 215, 204, 216 }, 0, GAME_CONFIG_VIOLENCE_LEVEL_KEY, 0, 0, &gPreferencesViolenceLevel1 },
    { 3, 0, 76, 309, 0, 0, { 202, 201, 213, 0 }, 0, GAME_CONFIG_TARGET_HIGHLIGHT_KEY, 0, 0, &gPreferencesTargetHighlight1 },
    { 2, 0, 76, 387, 0, 0, { 202, 201, 0, 0 }, 0, GAME_CONFIG_COMBAT_LOOKS_KEY, 0, 0, &gPreferencesCombatLooks1 },
    { 2, 0, 299, 74, 0, 0, { 211, 212, 0, 0 }, 0, GAME_CONFIG_COMBAT_MESSAGES_KEY, 0, 0, &gPreferencesCombatMessages1 },
    { 2, 0, 299, 141, 0, 0, { 202, 201, 0, 0 }, 0, GAME_CONFIG_COMBAT_TAUNTS_KEY, 0, 0, &gPreferencesCombatTaunts1 },
    { 2, 0, 299, 207, 0, 0, { 202, 201, 0, 0 }, 0, GAME_CONFIG_LANGUAGE_FILTER_KEY, 0, 0, &gPreferencesLanguageFilter1 },
    { 2, 0, 299, 271, 0, 0, { 209, 219, 0, 0 }, 0, GAME_CONFIG_RUNNING_KEY, 0, 0, &gPreferencesRunning1 },
    { 2, 0, 299, 338, 0, 0, { 202, 201, 0, 0 }, 0, GAME_CONFIG_SUBTITLES_KEY, 0, 0, &gPreferencesSubtitles1 },
    { 2, 0, 299, 404, 0, 0, { 202, 201, 0, 0 }, 0, GAME_CONFIG_ITEM_HIGHLIGHT_KEY, 0, 0, &gPreferencesItemHighlight1 },
    { 2, 0, 374, 50, 0, 0, { 207, 210, 0, 0 }, 0, GAME_CONFIG_COMBAT_SPEED_KEY, 0.0, 50.0, &gPreferencesCombatSpeed1 },
    { 3, 0, 374, 125, 0, 0, { 217, 209, 218, 0 }, 0, GAME_CONFIG_TEXT_BASE_DELAY_KEY, 1.0, 6.0, NULL },
    { 4, 0, 374, 196, 0, 0, { 202, 221, 209, 222 }, 0, GAME_CONFIG_MASTER_VOLUME_KEY, 0, 32767.0, &gPreferencesMasterVolume1 },
    { 4, 0, 374, 247, 0, 0, { 202, 221, 209, 222 }, 0, GAME_CONFIG_MUSIC_VOLUME_KEY, 0, 32767.0, &gPreferencesMusicVolume1 },
    { 4, 0, 374, 298, 0, 0, { 202, 221, 209, 222 }, 0, GAME_CONFIG_SNDFX_VOLUME_KEY, 0, 32767.0, &gPreferencesSoundEffectsVolume1 },
    { 4, 0, 374, 349, 0, 0, { 202, 221, 209, 222 }, 0, GAME_CONFIG_SPEECH_VOLUME_KEY, 0, 32767.0, &gPreferencesSpeechVolume1 },
    { 2, 0, 374, 400, 0, 0, { 207, 223, 0, 0 }, 0, GAME_CONFIG_BRIGHTNESS_KEY, 1.0, 1.17999267578125, NULL },
    { 2, 0, 374, 451, 0, 0, { 207, 218, 0, 0 }, 0, GAME_CONFIG_MOUSE_SENSITIVITY_KEY, 1.0, 2.5, NULL },
};

// 0x6637D0
static Size gOptionsWindowFrmSizes[OPTIONS_WINDOW_FRM_COUNT];

// 0x6637E8
static MessageList gOptionsMessageList;

// 0x6637F0
static Size gPreferencesWindowFrmSizes[PREFERENCES_WINDOW_FRM_COUNT];

// 0x663840
static MessageListItem gOptionsMessageListItem;

// 0x663850
static unsigned char* gPreferencesWindowFrmData[PREFERENCES_WINDOW_FRM_COUNT];

// 0x663878
static unsigned char* _opbtns[OPTIONS_WINDOW_BUTTONS_COUNT];

// 0x6638A0
static CacheEntry* gPreferencesWindowFrmHandles[PREFERENCES_WINDOW_FRM_COUNT];

// 0x6638C8
static double gPreferencesTextBaseDelay2;

// 0x6638D0
static double gPreferencesBrightness1;

// 0x6638D8
static double gPreferencesBrightness2;

// 0x6638E0
static double gPreferencesTextBaseDelay1;

// 0x6638E8
static double gPreferencesMouseSensitivity1;

// 0x6638F0
static double gPreferencesMouseSensitivity2;

// 0x6638F8
static unsigned char* gPreferencesWindowBuffer;

// 0x6638FC
static bool gOptionsWindowGameMouseObjectsWasVisible;

// 0x663900
static int gOptionsWindow;

// 0x663904
static int gPreferencesWindow;

// 0x663908
static unsigned char* gOptionsWindowBuffer;

// 0x66390C
static CacheEntry* gOptionsWindowFrmHandles[OPTIONS_WINDOW_FRM_COUNT];

// 0x663918
static unsigned char* gOptionsWindowFrmData[OPTIONS_WINDOW_FRM_COUNT];

// 0x663924
static int gPreferencesGameDifficulty2;

// 0x663928
static int gPreferencesCombatDifficulty2;

// 0x66392C
static int gPreferencesViolenceLevel2;

// 0x663930
static int gPreferencesTargetHighlight2;

// 0x663934
static int gPreferencesCombatLooks2;

// 0x663938
static int gPreferencesCombatMessages2;

// 0x66393C
static int gPreferencesCombatTaunts2;

// 0x663940
static int gPreferencesLanguageFilter2;

// 0x663944
static int gPreferencesRunning2;

// 0x663948
static int gPreferencesSubtitles2;

// 0x66394C
static int gPreferencesItemHighlight2;

// 0x663950
static int gPreferencesCombatSpeed2;

// 0x663954
static int gPreferencesPlayerSpeedup2;

// 0x663958
static int gPreferencesMasterVolume2;

// 0x66395C
static int gPreferencesMusicVolume2;

// 0x663960
static int gPreferencesSoundEffectsVolume2;

// 0x663964
static int gPreferencesSpeechVolume2;

// 0x663970
int gPreferencesSoundEffectsVolume1;

// 0x663974
int gPreferencesSubtitles1;

// 0x663978
int gPreferencesLanguageFilter1;

// 0x66397C
int gPreferencesSpeechVolume1;

// 0x663980
int gPreferencesMasterVolume1;

// 0x663984
int gPreferencesPlayerSpeedup1;

// 0x663988
int gPreferencesCombatTaunts1;

// 0x66398C
static int gOptionsWindowOldFont;

// 0x663990
int gPreferencesMusicVolume1;

// 0x663994
static bool gOptionsWindowIsoWasEnabled;

// 0x663998
int gPreferencesRunning1;

// 0x66399C
int gPreferencesCombatSpeed1;

//
static int _plyrspdbid;

// 0x6639A4
int gPreferencesItemHighlight1;

// 0x6639A8
static bool _changed;

// 0x6639AC
int gPreferencesCombatMessages1;

// 0x6639B0
int gPreferencesTargetHighlight1;

// 0x6639B4
int gPreferencesCombatDifficulty1;

// 0x6639B8
int gPreferencesViolenceLevel1;

// 0x6639BC
int gPreferencesGameDifficulty1;

// 0x6639C0
int gPreferencesCombatLooks1;

// 0x48FC48
int showOptions()
{
    return showOptionsWithInitialKeyCode(-1);
}

// 0x48FC50
int showOptionsWithInitialKeyCode(int initialKeyCode)
{
    if (optionsWindowInit() == -1) {
        debugPrint("\nOPTION MENU: Error loading option dialog data!\n");
        return -1;
    }

    int rc = -1;
    while (rc == -1) {
        int keyCode = _get_input();
        bool showPreferences = false;

        if (initialKeyCode != -1) {
            keyCode = initialKeyCode;
            rc = 1;
        }

        if (keyCode == KEY_ESCAPE || keyCode == 504 || _game_user_wants_to_quit != 0) {
            rc = 0;
        } else {
            switch (keyCode) {
            case KEY_RETURN:
            case KEY_UPPERCASE_O:
            case KEY_LOWERCASE_O:
            case KEY_UPPERCASE_D:
            case KEY_LOWERCASE_D:
                soundPlayFile("ib1p1xx1");
                rc = 0;
                break;
            case KEY_UPPERCASE_S:
            case KEY_LOWERCASE_S:
            case 500:
                if (lsgSaveGame(1) != 1) {
                    rc = 1;
                }
                break;
            case KEY_UPPERCASE_L:
            case KEY_LOWERCASE_L:
            case 501:
                if (lsgLoadGame(LOAD_SAVE_MODE_NORMAL) == 1) {
                    rc = 1;
                }
                break;
            case KEY_UPPERCASE_P:
            case KEY_LOWERCASE_P:
                soundPlayFile("ib1p1xx1");
                // FALLTHROUGH
            case 502:
                // PREFERENCES
                showPreferences = true;
                break;
            case KEY_PLUS:
            case KEY_EQUAL:
                brightnessIncrease();
                break;
            case KEY_UNDERSCORE:
            case KEY_MINUS:
                brightnessDecrease();
                break;
            }
        }

        if (showPreferences) {
            _do_prefscreen();
        } else {
            switch (keyCode) {
            case KEY_F12:
                takeScreenshot();
                break;
            case KEY_UPPERCASE_E:
            case KEY_LOWERCASE_E:
            case KEY_CTRL_Q:
            case KEY_CTRL_X:
            case KEY_F10:
            case 503:
                showQuitConfirmationDialog();
                break;
            }
        }
    }

    optionsWindowFree();

    return rc;
}

// 0x48FE14
static int optionsWindowInit()
{
    gOptionsWindowOldFont = fontGetCurrent();

    if (!messageListInit(&gOptionsMessageList)) {
        return -1;
    }

    char path[COMPAT_MAX_PATH];
    sprintf(path, "%s%s", asc_5186C8, "options.msg");
    if (!messageListLoad(&gOptionsMessageList, path)) {
        return -1;
    }

    for (int index = 0; index < OPTIONS_WINDOW_FRM_COUNT; index++) {
        int fid = buildFid(6, gOptionsWindowFrmIds[index], 0, 0, 0);
        gOptionsWindowFrmData[index] = artLockFrameDataReturningSize(fid, &(gOptionsWindowFrmHandles[index]), &(gOptionsWindowFrmSizes[index].width), &(gOptionsWindowFrmSizes[index].height));

        if (gOptionsWindowFrmData[index] == NULL) {
            while (--index >= 0) {
                artUnlock(gOptionsWindowFrmHandles[index]);
            }

            messageListFree(&gOptionsMessageList);

            return -1;
        }
    }

    int cycle = 0;
    for (int index = 0; index < OPTIONS_WINDOW_BUTTONS_COUNT; index++) {
        _opbtns[index] = (unsigned char*)internal_malloc(gOptionsWindowFrmSizes[OPTIONS_WINDOW_FRM_BUTTON_ON].width * gOptionsWindowFrmSizes[OPTIONS_WINDOW_FRM_BUTTON_ON].height + 1024);
        if (_opbtns[index] == NULL) {
            while (--index >= 0) {
                internal_free(_opbtns[index]);
            }

            for (int index = 0; index < OPTIONS_WINDOW_FRM_COUNT; index++) {
                artUnlock(gOptionsWindowFrmHandles[index]);
            }

            messageListFree(&gOptionsMessageList);

            return -1;
        }

        cycle = cycle ^ 1;

        memcpy(_opbtns[index], gOptionsWindowFrmData[cycle + 1], gOptionsWindowFrmSizes[OPTIONS_WINDOW_FRM_BUTTON_ON].width * gOptionsWindowFrmSizes[OPTIONS_WINDOW_FRM_BUTTON_ON].height);
    }

    int optionsWindowX = (screenGetWidth() - gOptionsWindowFrmSizes[OPTIONS_WINDOW_FRM_BACKGROUND].width) / 2;
    int optionsWindowY = (screenGetHeight() - gOptionsWindowFrmSizes[OPTIONS_WINDOW_FRM_BACKGROUND].height) / 2 - 60;
    gOptionsWindow = windowCreate(optionsWindowX,
        optionsWindowY,
        gOptionsWindowFrmSizes[0].width,
        gOptionsWindowFrmSizes[0].height,
        256,
        WINDOW_FLAG_0x10 | WINDOW_FLAG_0x02);

    if (gOptionsWindow == -1) {
        for (int index = 0; index < OPTIONS_WINDOW_BUTTONS_COUNT; index++) {
            internal_free(_opbtns[index]);
        }

        for (int index = 0; index < OPTIONS_WINDOW_FRM_COUNT; index++) {
            artUnlock(gOptionsWindowFrmHandles[index]);
        }

        messageListFree(&gOptionsMessageList);

        return -1;
    }

    gOptionsWindowIsoWasEnabled = isoDisable();

    gOptionsWindowGameMouseObjectsWasVisible = gameMouseObjectsIsVisible();
    if (gOptionsWindowGameMouseObjectsWasVisible) {
        gameMouseObjectsHide();
    }

    gameMouseSetCursor(MOUSE_CURSOR_ARROW);

    gOptionsWindowBuffer = windowGetBuffer(gOptionsWindow);
    memcpy(gOptionsWindowBuffer, gOptionsWindowFrmData[OPTIONS_WINDOW_FRM_BACKGROUND], gOptionsWindowFrmSizes[OPTIONS_WINDOW_FRM_BACKGROUND].width * gOptionsWindowFrmSizes[OPTIONS_WINDOW_FRM_BACKGROUND].height);

    fontSetCurrent(103);

    int textY = (gOptionsWindowFrmSizes[OPTIONS_WINDOW_FRM_BUTTON_ON].height - fontGetLineHeight()) / 2 + 1;
    int buttonY = 17;

    for (int index = 0; index < OPTIONS_WINDOW_BUTTONS_COUNT; index += 2) {
        char text[128];

        const char* msg = getmsg(&gOptionsMessageList, &gOptionsMessageListItem, index / 2);
        strcpy(text, msg);

        int textX = (gOptionsWindowFrmSizes[OPTIONS_WINDOW_FRM_BUTTON_ON].width - fontGetStringWidth(text)) / 2;
        if (textX < 0) {
            textX = 0;
        }

        fontDrawText(_opbtns[index] + gOptionsWindowFrmSizes[OPTIONS_WINDOW_FRM_BUTTON_ON].width * textY + textX, text, gOptionsWindowFrmSizes[OPTIONS_WINDOW_FRM_BUTTON_ON].width, gOptionsWindowFrmSizes[OPTIONS_WINDOW_FRM_BUTTON_ON].width, _colorTable[18979]);
        fontDrawText(_opbtns[index + 1] + gOptionsWindowFrmSizes[OPTIONS_WINDOW_FRM_BUTTON_ON].width * textY + textX, text, gOptionsWindowFrmSizes[OPTIONS_WINDOW_FRM_BUTTON_ON].width, gOptionsWindowFrmSizes[OPTIONS_WINDOW_FRM_BUTTON_ON].width, _colorTable[14723]);

        int btn = buttonCreate(gOptionsWindow, 13, buttonY, gOptionsWindowFrmSizes[OPTIONS_WINDOW_FRM_BUTTON_ON].width, gOptionsWindowFrmSizes[OPTIONS_WINDOW_FRM_BUTTON_ON].height, -1, -1, -1, index / 2 + 500, _opbtns[index], _opbtns[index + 1], NULL, 32);
        if (btn != -1) {
            buttonSetCallbacks(btn, _gsound_lrg_butt_press, _gsound_lrg_butt_release);
        }

        buttonY += gOptionsWindowFrmSizes[OPTIONS_WINDOW_FRM_BUTTON_ON].height + 3;
    }

    fontSetCurrent(101);

    windowRefresh(gOptionsWindow);

    return 0;
}

// 0x490244
static int optionsWindowFree()
{
    windowDestroy(gOptionsWindow);
    fontSetCurrent(gOptionsWindowOldFont);
    messageListFree(&gOptionsMessageList);

    for (int index = 0; index < OPTIONS_WINDOW_BUTTONS_COUNT; index++) {
        internal_free(_opbtns[index]);
    }

    for (int index = 0; index < OPTIONS_WINDOW_FRM_COUNT; index++) {
        artUnlock(gOptionsWindowFrmHandles[index]);
    }

    if (gOptionsWindowGameMouseObjectsWasVisible) {
        gameMouseObjectsShow();
    }

    if (gOptionsWindowIsoWasEnabled) {
        isoEnable();
    }

    return 0;
}

// 0x4902B0
int showPause(bool a1)
{
    int graphicIds[PAUSE_WINDOW_FRM_COUNT];
    unsigned char* frmData[PAUSE_WINDOW_FRM_COUNT];
    CacheEntry* frmHandles[PAUSE_WINDOW_FRM_COUNT];
    Size frmSizes[PAUSE_WINDOW_FRM_COUNT];

    memcpy(graphicIds, gPauseWindowFrmIds, sizeof(gPauseWindowFrmIds));

    bool gameMouseWasVisible;
    if (!a1) {
        gOptionsWindowIsoWasEnabled = isoDisable();
        colorCycleDisable();

        gameMouseWasVisible = gameMouseObjectsIsVisible();
        if (gameMouseWasVisible) {
            gameMouseObjectsHide();
        }
    }

    gameMouseSetCursor(MOUSE_CURSOR_ARROW);
    _ShadeScreen(a1);

    for (int index = 0; index < PAUSE_WINDOW_FRM_COUNT; index++) {
        int fid = buildFid(6, graphicIds[index], 0, 0, 0);
        frmData[index] = artLockFrameDataReturningSize(fid, &(frmHandles[index]), &(frmSizes[index].width), &(frmSizes[index].height));
        if (frmData[index] == NULL) {
            while (--index >= 0) {
                artUnlock(frmHandles[index]);
            }

            debugPrint("\n** Error loading pause window graphics! **\n");
            return -1;
        }
    }

    if (!messageListInit(&gOptionsMessageList)) {
        // FIXME: Leaking graphics.
        return -1;
    }

    char path[COMPAT_MAX_PATH];
    sprintf(path, "%s%s", asc_5186C8, "options.msg");
    if (!messageListLoad(&gOptionsMessageList, path)) {
        // FIXME: Leaking graphics.
        return -1;
    }

    int pauseWindowX = (screenGetWidth() - frmSizes[PAUSE_WINDOW_FRM_BACKGROUND].width) / 2;
    int pauseWindowY = (screenGetHeight() - frmSizes[PAUSE_WINDOW_FRM_BACKGROUND].height) / 2;

    if (a1) {
        pauseWindowX -= 65;
        pauseWindowY -= 24;
    } else {
        pauseWindowY -= 54;
    }

    int window = windowCreate(pauseWindowX,
        pauseWindowY,
        frmSizes[PAUSE_WINDOW_FRM_BACKGROUND].width,
        frmSizes[PAUSE_WINDOW_FRM_BACKGROUND].height,
        256,
        WINDOW_FLAG_0x10 | WINDOW_FLAG_0x02);
    if (window == -1) {
        for (int index = 0; index < PAUSE_WINDOW_FRM_COUNT; index++) {
            artUnlock(frmHandles[index]);
        }

        messageListFree(&gOptionsMessageList);

        debugPrint("\n** Error opening pause window! **\n");
        return -1;
    }

    unsigned char* windowBuffer = windowGetBuffer(window);
    memcpy(windowBuffer,
        frmData[PAUSE_WINDOW_FRM_BACKGROUND],
        frmSizes[PAUSE_WINDOW_FRM_BACKGROUND].width * frmSizes[PAUSE_WINDOW_FRM_BACKGROUND].height);

    blitBufferToBufferTrans(frmData[PAUSE_WINDOW_FRM_DONE_BOX],
        frmSizes[PAUSE_WINDOW_FRM_DONE_BOX].width,
        frmSizes[PAUSE_WINDOW_FRM_DONE_BOX].height,
        frmSizes[PAUSE_WINDOW_FRM_DONE_BOX].width,
        windowBuffer + frmSizes[PAUSE_WINDOW_FRM_BACKGROUND].width * 42 + 13,
        frmSizes[PAUSE_WINDOW_FRM_BACKGROUND].width);

    gOptionsWindowOldFont = fontGetCurrent();
    fontSetCurrent(103);

    char* messageItemText;

    messageItemText = getmsg(&gOptionsMessageList, &gOptionsMessageListItem, 300);
    fontDrawText(windowBuffer + frmSizes[PAUSE_WINDOW_FRM_BACKGROUND].width * 45 + 52,
        messageItemText,
        frmSizes[PAUSE_WINDOW_FRM_BACKGROUND].width,
        frmSizes[PAUSE_WINDOW_FRM_BACKGROUND].width,
        _colorTable[18979]);

    fontSetCurrent(104);

    messageItemText = getmsg(&gOptionsMessageList, &gOptionsMessageListItem, 301);
    strcpy(path, messageItemText);

    int length = fontGetStringWidth(path);
    fontDrawText(windowBuffer + frmSizes[PAUSE_WINDOW_FRM_BACKGROUND].width * 10 + 2 + (frmSizes[PAUSE_WINDOW_FRM_BACKGROUND].width - length) / 2,
        path,
        frmSizes[PAUSE_WINDOW_FRM_BACKGROUND].width,
        frmSizes[PAUSE_WINDOW_FRM_BACKGROUND].width,
        _colorTable[18979]);

    int doneBtn = buttonCreate(window,
        26,
        46,
        frmSizes[PAUSE_WINDOW_FRM_LITTLE_RED_BUTTON_UP].width,
        frmSizes[PAUSE_WINDOW_FRM_LITTLE_RED_BUTTON_UP].height,
        -1,
        -1,
        -1,
        504,
        frmData[PAUSE_WINDOW_FRM_LITTLE_RED_BUTTON_UP],
        frmData[PAUSE_WINDOW_FRM_LITTLE_RED_BUTTON_DOWN],
        NULL,
        BUTTON_FLAG_TRANSPARENT);
    if (doneBtn != -1) {
        buttonSetCallbacks(doneBtn, _gsound_red_butt_press, _gsound_red_butt_release);
    }

    windowRefresh(window);

    bool done = false;
    while (!done) {
        int keyCode = _get_input();
        switch (keyCode) {
        case KEY_PLUS:
        case KEY_EQUAL:
            brightnessIncrease();
            break;
        case KEY_MINUS:
        case KEY_UNDERSCORE:
            brightnessDecrease();
            break;
        default:
            if (keyCode != -1 && keyCode != -2) {
                done = true;
            }

            if (_game_user_wants_to_quit != 0) {
                done = true;
            }
        }
    }

    if (!a1) {
        tileWindowRefresh();
    }

    windowDestroy(window);

    for (int index = 0; index < PAUSE_WINDOW_FRM_COUNT; index++) {
        artUnlock(frmHandles[index]);
    }

    messageListFree(&gOptionsMessageList);

    if (!a1) {
        if (gameMouseWasVisible) {
            gameMouseObjectsShow();
        }

        if (gOptionsWindowIsoWasEnabled) {
            isoEnable();
        }

        colorCycleEnable();

        gameMouseSetCursor(MOUSE_CURSOR_ARROW);
    }

    fontSetCurrent(gOptionsWindowOldFont);

    return 0;
}

// 0x490748
static void _ShadeScreen(bool a1)
{
    if (a1) {
        mouseHideCursor();
    } else {
        mouseHideCursor();
        tileWindowRefresh();

        int windowWidth = windowGetWidth(gIsoWindow);
        int windowHeight = windowGetHeight(gIsoWindow);
        unsigned char* windowBuffer = windowGetBuffer(gIsoWindow);
        grayscalePaletteApply(windowBuffer, windowWidth, windowHeight, windowWidth);

        windowRefresh(gIsoWindow);
    }

    mouseShowCursor();
}

// 0x492AA8
static void _SetSystemPrefs()
{
    preferencesSetDefaults(false);

    configGetInt(&gGameConfig, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_GAME_DIFFICULTY_KEY, &gPreferencesGameDifficulty1);
    configGetInt(&gGameConfig, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_COMBAT_DIFFICULTY_KEY, &gPreferencesCombatDifficulty1);
    configGetInt(&gGameConfig, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_VIOLENCE_LEVEL_KEY, &gPreferencesViolenceLevel1);
    configGetInt(&gGameConfig, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_TARGET_HIGHLIGHT_KEY, &gPreferencesTargetHighlight1);
    configGetInt(&gGameConfig, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_COMBAT_MESSAGES_KEY, &gPreferencesCombatMessages1);
    configGetInt(&gGameConfig, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_COMBAT_LOOKS_KEY, &gPreferencesCombatLooks1);
    configGetInt(&gGameConfig, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_COMBAT_TAUNTS_KEY, &gPreferencesCombatTaunts1);
    configGetInt(&gGameConfig, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_LANGUAGE_FILTER_KEY, &gPreferencesLanguageFilter1);
    configGetInt(&gGameConfig, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_RUNNING_KEY, &gPreferencesRunning1);
    configGetInt(&gGameConfig, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_SUBTITLES_KEY, &gPreferencesSubtitles1);
    configGetInt(&gGameConfig, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_ITEM_HIGHLIGHT_KEY, &gPreferencesItemHighlight1);
    configGetInt(&gGameConfig, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_COMBAT_SPEED_KEY, &gPreferencesCombatSpeed1);
    configGetDouble(&gGameConfig, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_TEXT_BASE_DELAY_KEY, &gPreferencesTextBaseDelay1);
    configGetInt(&gGameConfig, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_PLAYER_SPEEDUP_KEY, &gPreferencesPlayerSpeedup1);
    configGetInt(&gGameConfig, GAME_CONFIG_SOUND_KEY, GAME_CONFIG_MASTER_VOLUME_KEY, &gPreferencesMasterVolume1);
    configGetInt(&gGameConfig, GAME_CONFIG_SOUND_KEY, GAME_CONFIG_MUSIC_VOLUME_KEY, &gPreferencesMusicVolume1);
    configGetInt(&gGameConfig, GAME_CONFIG_SOUND_KEY, GAME_CONFIG_SNDFX_VOLUME_KEY, &gPreferencesSoundEffectsVolume1);
    configGetInt(&gGameConfig, GAME_CONFIG_SOUND_KEY, GAME_CONFIG_SPEECH_VOLUME_KEY, &gPreferencesSpeechVolume1);
    configGetDouble(&gGameConfig, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_BRIGHTNESS_KEY, &gPreferencesBrightness1);
    configGetDouble(&gGameConfig, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_MOUSE_SENSITIVITY_KEY, &gPreferencesMouseSensitivity1);

    _JustUpdate_();
}

// Copy options (1) to (2).
//
// 0x493054
static void _SaveSettings()
{
    gPreferencesGameDifficulty2 = gPreferencesGameDifficulty1;
    gPreferencesCombatDifficulty2 = gPreferencesCombatDifficulty1;
    gPreferencesViolenceLevel2 = gPreferencesViolenceLevel1;
    gPreferencesTargetHighlight2 = gPreferencesTargetHighlight1;
    gPreferencesCombatLooks2 = gPreferencesCombatLooks1;
    gPreferencesCombatMessages2 = gPreferencesCombatMessages1;
    gPreferencesCombatTaunts2 = gPreferencesCombatTaunts1;
    gPreferencesLanguageFilter2 = gPreferencesLanguageFilter1;
    gPreferencesRunning2 = gPreferencesRunning1;
    gPreferencesSubtitles2 = gPreferencesSubtitles1;
    gPreferencesItemHighlight2 = gPreferencesItemHighlight1;
    gPreferencesCombatSpeed2 = gPreferencesCombatSpeed1;
    gPreferencesPlayerSpeedup2 = gPreferencesPlayerSpeedup1;
    gPreferencesMasterVolume2 = gPreferencesMasterVolume1;
    gPreferencesTextBaseDelay2 = gPreferencesTextBaseDelay1;
    gPreferencesMusicVolume2 = gPreferencesMusicVolume1;
    gPreferencesBrightness2 = gPreferencesBrightness1;
    gPreferencesSoundEffectsVolume2 = gPreferencesSoundEffectsVolume1;
    gPreferencesMouseSensitivity2 = gPreferencesMouseSensitivity1;
    gPreferencesSpeechVolume2 = gPreferencesSpeechVolume1;
}

// Copy options (2) to (1).
//
// 0x493128
static void _RestoreSettings()
{
    gPreferencesGameDifficulty1 = gPreferencesGameDifficulty2;
    gPreferencesCombatDifficulty1 = gPreferencesCombatDifficulty2;
    gPreferencesViolenceLevel1 = gPreferencesViolenceLevel2;
    gPreferencesTargetHighlight1 = gPreferencesTargetHighlight2;
    gPreferencesCombatLooks1 = gPreferencesCombatLooks2;
    gPreferencesCombatMessages1 = gPreferencesCombatMessages2;
    gPreferencesCombatTaunts1 = gPreferencesCombatTaunts2;
    gPreferencesLanguageFilter1 = gPreferencesLanguageFilter2;
    gPreferencesRunning1 = gPreferencesRunning2;
    gPreferencesSubtitles1 = gPreferencesSubtitles2;
    gPreferencesItemHighlight1 = gPreferencesItemHighlight2;
    gPreferencesCombatSpeed1 = gPreferencesCombatSpeed2;
    gPreferencesPlayerSpeedup1 = gPreferencesPlayerSpeedup2;
    gPreferencesMasterVolume1 = gPreferencesMasterVolume2;
    gPreferencesTextBaseDelay1 = gPreferencesTextBaseDelay2;
    gPreferencesMusicVolume1 = gPreferencesMusicVolume2;
    gPreferencesBrightness1 = gPreferencesBrightness2;
    gPreferencesSoundEffectsVolume1 = gPreferencesSoundEffectsVolume2;
    gPreferencesMouseSensitivity1 = gPreferencesMouseSensitivity2;
    gPreferencesSpeechVolume1 = gPreferencesSpeechVolume2;

    _JustUpdate_();
}

// 0x492F60
static void preferencesSetDefaults(bool a1)
{
    gPreferencesCombatDifficulty1 = COMBAT_DIFFICULTY_NORMAL;
    gPreferencesViolenceLevel1 = VIOLENCE_LEVEL_MAXIMUM_BLOOD;
    gPreferencesTargetHighlight1 = TARGET_HIGHLIGHT_TARGETING_ONLY;
    gPreferencesCombatMessages1 = 1;
    gPreferencesCombatLooks1 = 0;
    gPreferencesCombatTaunts1 = 1;
    gPreferencesRunning1 = 0;
    gPreferencesSubtitles1 = 0;
    gPreferencesItemHighlight1 = 1;
    gPreferencesCombatSpeed1 = 0;
    gPreferencesPlayerSpeedup1 = 0;
    gPreferencesTextBaseDelay1 = 3.5;
    gPreferencesBrightness1 = 1.0;
    gPreferencesMouseSensitivity1 = 1.0;
    gPreferencesGameDifficulty1 = 1;
    gPreferencesLanguageFilter1 = 0;
    gPreferencesMasterVolume1 = 22281;
    gPreferencesMusicVolume1 = 22281;
    gPreferencesSoundEffectsVolume1 = 22281;
    gPreferencesSpeechVolume1 = 22281;

    if (a1) {
        for (int index = 0; index < PREF_COUNT; index++) {
            _UpdateThing(index);
        }
        _win_set_button_rest_state(_plyrspdbid, gPreferencesPlayerSpeedup1, 0);
        windowRefresh(gPreferencesWindow);
        _changed = true;
    }
}

// 0x4931F8
static void _JustUpdate_()
{
    gPreferencesGameDifficulty1 = std::clamp(gPreferencesGameDifficulty1, 0, 2);
    gPreferencesCombatDifficulty1 = std::clamp(gPreferencesCombatDifficulty1, 0, 2);
    gPreferencesViolenceLevel1 = std::clamp(gPreferencesViolenceLevel1, 0, 3);
    gPreferencesTargetHighlight1 = std::clamp(gPreferencesTargetHighlight1, 0, 2);
    gPreferencesCombatMessages1 = std::clamp(gPreferencesCombatMessages1, 0, 1);
    gPreferencesCombatLooks1 = std::clamp(gPreferencesCombatLooks1, 0, 1);
    gPreferencesCombatTaunts1 = std::clamp(gPreferencesCombatTaunts1, 0, 1);
    gPreferencesLanguageFilter1 = std::clamp(gPreferencesLanguageFilter1, 0, 1);
    gPreferencesRunning1 = std::clamp(gPreferencesRunning1, 0, 1);
    gPreferencesSubtitles1 = std::clamp(gPreferencesSubtitles1, 0, 1);
    gPreferencesItemHighlight1 = std::clamp(gPreferencesItemHighlight1, 0, 1);
    gPreferencesCombatSpeed1 = std::clamp(gPreferencesCombatSpeed1, 0, 50);
    gPreferencesPlayerSpeedup1 = std::clamp(gPreferencesPlayerSpeedup1, 0, 1);
    gPreferencesTextBaseDelay1 = std::clamp(gPreferencesTextBaseDelay1, 6.0, 10.0);
    gPreferencesMasterVolume1 = std::clamp(gPreferencesMasterVolume1, 0, VOLUME_MAX);
    gPreferencesMusicVolume1 = std::clamp(gPreferencesMusicVolume1, 0, VOLUME_MAX);
    gPreferencesSoundEffectsVolume1 = std::clamp(gPreferencesSoundEffectsVolume1, 0, VOLUME_MAX);
    gPreferencesSpeechVolume1 = std::clamp(gPreferencesSpeechVolume1, 0, VOLUME_MAX);
    gPreferencesBrightness1 = std::clamp(gPreferencesBrightness1, 1.0, 1.17999267578125);
    gPreferencesMouseSensitivity1 = std::clamp(gPreferencesMouseSensitivity1, 1.0, 2.5);

    textObjectsSetBaseDelay(gPreferencesTextBaseDelay1);
    gameMouseLoadItemHighlight();

    double textLineDelay = (gPreferencesTextBaseDelay1 + (-1.0)) * 0.2 * 2.0;
    textLineDelay = std::clamp(textLineDelay, 0.0, 2.0);

    textObjectsSetLineDelay(textLineDelay);
    aiMessageListReloadIfNeeded();
    _scr_message_free();
    gameSoundSetMasterVolume(gPreferencesMasterVolume1);
    backgroundSoundSetVolume(gPreferencesMusicVolume1);
    soundEffectsSetVolume(gPreferencesSoundEffectsVolume1);
    speechSetVolume(gPreferencesSpeechVolume1);
    mouseSetSensitivity(gPreferencesMouseSensitivity1);
    colorSetBrightness(gPreferencesBrightness1);
}

// init_options_menu
// 0x4928B8
int _init_options_menu()
{
    for (int index = 0; index < 11; index++) {
        gPreferenceDescriptions[index].direction = 0;
    }

    _SetSystemPrefs();

    grayscalePaletteUpdate(0, 255);

    return 0;
}

// 0x491A68
static void _UpdateThing(int index)
{
    fontSetCurrent(101);

    PreferenceDescription* meta = &(gPreferenceDescriptions[index]);

    if (index >= FIRST_PRIMARY_PREF && index <= LAST_PRIMARY_PREF) {
        int primaryOptionIndex = index - FIRST_PRIMARY_PREF;

        int offsets[PRIMARY_PREF_COUNT];
        memcpy(offsets, dword_48FC1C, sizeof(dword_48FC1C));

        blitBufferToBuffer(gPreferencesWindowFrmData[PREFERENCES_WINDOW_FRM_BACKGROUND] + 640 * offsets[primaryOptionIndex] + 23, 160, 54, 640, gPreferencesWindowBuffer + 640 * offsets[primaryOptionIndex] + 23, 640);

        for (int valueIndex = 0; valueIndex < meta->valuesCount; valueIndex++) {
            const char* text = getmsg(&gOptionsMessageList, &gOptionsMessageListItem, meta->labelIds[valueIndex]);

            char copy[100]; // TODO: Size is probably wrong.
            strcpy(copy, text);

            int x = meta->knobX + word_48FBF6[valueIndex];
            int len = fontGetStringWidth(copy);
            switch (valueIndex) {
            case 0:
                x -= fontGetStringWidth(copy);
                meta->minX = x;
                break;
            case 1:
                x -= len / 2;
                meta->maxX = x + len;
                break;
            case 2:
            case 3:
                meta->maxX = x + len;
                break;
            }

            char* p = copy;
            while (*p != '\0' && *p != ' ') {
                p++;
            }

            int y = meta->knobY + word_48FBFE[valueIndex];
            const char* s;
            if (*p != '\0') {
                *p = '\0';
                fontDrawText(gPreferencesWindowBuffer + 640 * y + x, copy, 640, 640, _colorTable[18979]);
                s = p + 1;
                y += fontGetLineHeight();
            } else {
                s = copy;
            }

            fontDrawText(gPreferencesWindowBuffer + 640 * y + x, s, 640, 640, _colorTable[18979]);
        }

        int value = *(meta->valuePtr);
        blitBufferToBufferTrans(gPreferencesWindowFrmData[PREFERENCES_WINDOW_FRM_PRIMARY_SWITCH] + (46 * 47) * value, 46, 47, 46, gPreferencesWindowBuffer + 640 * meta->knobY + meta->knobX, 640);
    } else if (index >= FIRST_SECONDARY_PREF && index <= LAST_SECONDARY_PREF) {
        int secondaryOptionIndex = index - FIRST_SECONDARY_PREF;

        int offsets[SECONDARY_PREF_COUNT];
        memcpy(offsets, dword_48FC30, sizeof(dword_48FC30));

        blitBufferToBuffer(gPreferencesWindowFrmData[PREFERENCES_WINDOW_FRM_BACKGROUND] + 640 * offsets[secondaryOptionIndex] + 251, 113, 34, 640, gPreferencesWindowBuffer + 640 * offsets[secondaryOptionIndex] + 251, 640);

        // Secondary options are booleans, so it's index is also it's value.
        for (int value = 0; value < 2; value++) {
            const char* text = getmsg(&gOptionsMessageList, &gOptionsMessageListItem, meta->labelIds[value]);

            int x;
            if (value) {
                x = meta->knobX + word_48FC06[value];
                meta->maxX = x + fontGetStringWidth(text);
            } else {
                x = meta->knobX + word_48FC06[value] - fontGetStringWidth(text);
                meta->minX = x;
            }
            fontDrawText(gPreferencesWindowBuffer + 640 * (meta->knobY - 5) + x, text, 640, 640, _colorTable[18979]);
        }

        int value = *(meta->valuePtr);
        if (index == PREF_COMBAT_MESSAGES) {
            value ^= 1;
        }
        blitBufferToBufferTrans(gPreferencesWindowFrmData[PREFERENCES_WINDOW_FRM_SECONDARY_SWITCH] + (22 * 25) * value, 22, 25, 22, gPreferencesWindowBuffer + 640 * meta->knobY + meta->knobX, 640);
    } else if (index >= FIRST_RANGE_PREF && index <= LAST_RANGE_PREF) {
        blitBufferToBuffer(gPreferencesWindowFrmData[PREFERENCES_WINDOW_FRM_BACKGROUND] + 640 * (meta->knobY - 12) + 384, 240, 24, 640, gPreferencesWindowBuffer + 640 * (meta->knobY - 12) + 384, 640);
        switch (index) {
        case PREF_COMBAT_SPEED:
            if (1) {
                double value = *meta->valuePtr;
                value = std::clamp(value, 0.0, 50.0);

                int x = (int)((value - meta->minValue) * 219.0 / (meta->maxValue - meta->minValue) + 384.0);
                blitBufferToBufferTrans(gPreferencesWindowFrmData[PREFERENCES_WINDOW_FRM_KNOB_OFF], 21, 12, 21, gPreferencesWindowBuffer + 640 * meta->knobY + x, 640);
            }
            break;
        case PREF_TEXT_BASE_DELAY:
            if (1) {
                gPreferencesTextBaseDelay1 = std::clamp(gPreferencesTextBaseDelay1, 1.0, 6.0);

                int x = (int)((6.0 - gPreferencesTextBaseDelay1) * 43.8 + 384.0);
                blitBufferToBufferTrans(gPreferencesWindowFrmData[PREFERENCES_WINDOW_FRM_KNOB_OFF], 21, 12, 21, gPreferencesWindowBuffer + 640 * meta->knobY + x, 640);

                double value = (gPreferencesTextBaseDelay1 - 1.0) * 0.2 * 2.0;
                value = std::clamp(value, 0.0, 2.0);

                textObjectsSetBaseDelay(gPreferencesTextBaseDelay1);
                textObjectsSetLineDelay(value);
            }
            break;
        case PREF_MASTER_VOLUME:
        case PREF_MUSIC_VOLUME:
        case PREF_SFX_VOLUME:
        case PREF_SPEECH_VOLUME:
            if (1) {
                double value = *meta->valuePtr;
                value = std::clamp(value, meta->minValue, meta->maxValue);

                int x = (int)((value - meta->minValue) * 219.0 / (meta->maxValue - meta->minValue) + 384.0);
                blitBufferToBufferTrans(gPreferencesWindowFrmData[PREFERENCES_WINDOW_FRM_KNOB_OFF], 21, 12, 21, gPreferencesWindowBuffer + 640 * meta->knobY + x, 640);

                switch (index) {
                case PREF_MASTER_VOLUME:
                    gameSoundSetMasterVolume(gPreferencesMasterVolume1);
                    break;
                case PREF_MUSIC_VOLUME:
                    backgroundSoundSetVolume(gPreferencesMusicVolume1);
                    break;
                case PREF_SFX_VOLUME:
                    soundEffectsSetVolume(gPreferencesSoundEffectsVolume1);
                    break;
                case PREF_SPEECH_VOLUME:
                    speechSetVolume(gPreferencesSpeechVolume1);
                    break;
                }
            }
            break;
        case PREF_BRIGHTNESS:
            if (1) {
                gPreferencesBrightness1 = std::clamp(gPreferencesBrightness1, 1.0, 1.17999267578125);

                int x = (int)((gPreferencesBrightness1 - meta->minValue) * (219.0 / (meta->maxValue - meta->minValue)) + 384.0);
                blitBufferToBufferTrans(gPreferencesWindowFrmData[PREFERENCES_WINDOW_FRM_KNOB_OFF], 21, 12, 21, gPreferencesWindowBuffer + 640 * meta->knobY + x, 640);

                colorSetBrightness(gPreferencesBrightness1);
            }
            break;
        case PREF_MOUSE_SENSITIVIY:
            if (1) {
                gPreferencesMouseSensitivity1 = std::clamp(gPreferencesMouseSensitivity1, 1.0, 2.5);

                int x = (int)((gPreferencesMouseSensitivity1 - meta->minValue) * (219.0 / (meta->maxValue - meta->minValue)) + 384.0);
                blitBufferToBufferTrans(gPreferencesWindowFrmData[PREFERENCES_WINDOW_FRM_KNOB_OFF], 21, 12, 21, gPreferencesWindowBuffer + 640 * meta->knobY + x, 640);

                mouseSetSensitivity(gPreferencesMouseSensitivity1);
            }
            break;
        }

        for (int optionIndex = 0; optionIndex < meta->valuesCount; optionIndex++) {
            const char* str = getmsg(&gOptionsMessageList, &gOptionsMessageListItem, meta->labelIds[optionIndex]);

            int x;
            switch (optionIndex) {
            case 0:
                // 0x4926AA
                x = 384;
                // TODO: Incomplete.
                break;
            case 1:
                // 0x4926F3
                switch (meta->valuesCount) {
                case 2:
                    x = 624 - fontGetStringWidth(str);
                    break;
                case 3:
                    // This code path does not use floating-point arithmetic
                    x = 504 - fontGetStringWidth(str) / 2 - 2;
                    break;
                case 4:
                    // Uses floating-point arithmetic
                    x = 444 + fontGetStringWidth(str) / 2 - 8;
                    break;
                }
                break;
            case 2:
                // 0x492766
                switch (meta->valuesCount) {
                case 3:
                    x = 624 - fontGetStringWidth(str);
                    break;
                case 4:
                    // Uses floating-point arithmetic
                    x = 564 - fontGetStringWidth(str) - 4;
                    break;
                }
                break;
            case 3:
                // 0x49279E
                x = 624 - fontGetStringWidth(str);
                break;
            }
            fontDrawText(gPreferencesWindowBuffer + 640 * (meta->knobY - 12) + x, str, 640, 640, _colorTable[18979]);
        }
    } else {
        // return false;
    }

    // TODO: Incomplete.

    // return true;
}

// 0x492CB0
int _SavePrefs(bool save)
{
    configSetInt(&gGameConfig, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_GAME_DIFFICULTY_KEY, gPreferencesGameDifficulty1);
    configSetInt(&gGameConfig, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_COMBAT_DIFFICULTY_KEY, gPreferencesCombatDifficulty1);
    configSetInt(&gGameConfig, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_VIOLENCE_LEVEL_KEY, gPreferencesViolenceLevel1);
    configSetInt(&gGameConfig, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_TARGET_HIGHLIGHT_KEY, gPreferencesTargetHighlight1);
    configSetInt(&gGameConfig, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_COMBAT_MESSAGES_KEY, gPreferencesCombatMessages1);
    configSetInt(&gGameConfig, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_COMBAT_LOOKS_KEY, gPreferencesCombatLooks1);
    configSetInt(&gGameConfig, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_COMBAT_TAUNTS_KEY, gPreferencesCombatTaunts1);
    configSetInt(&gGameConfig, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_LANGUAGE_FILTER_KEY, gPreferencesLanguageFilter1);
    configSetInt(&gGameConfig, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_RUNNING_KEY, gPreferencesRunning1);
    configSetInt(&gGameConfig, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_SUBTITLES_KEY, gPreferencesSubtitles1);
    configSetInt(&gGameConfig, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_ITEM_HIGHLIGHT_KEY, gPreferencesItemHighlight1);
    configSetInt(&gGameConfig, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_COMBAT_SPEED_KEY, gPreferencesCombatSpeed1);
    configSetDouble(&gGameConfig, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_TEXT_BASE_DELAY_KEY, gPreferencesTextBaseDelay1);

    double textLineDelay = (gPreferencesTextBaseDelay1 + dbl_50C2D0) * dbl_50C2D8 * dbl_50C2E0;
    if (textLineDelay >= 0.0) {
        if (textLineDelay > dbl_50C2E0) {
            textLineDelay = 2.0;
        }

        configSetDouble(&gGameConfig, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_TEXT_LINE_DELAY_KEY, textLineDelay);
    } else {
        configSetDouble(&gGameConfig, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_TEXT_LINE_DELAY_KEY, 0.0);
    }

    configSetInt(&gGameConfig, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_PLAYER_SPEEDUP_KEY, gPreferencesPlayerSpeedup1);
    configSetInt(&gGameConfig, GAME_CONFIG_SOUND_KEY, GAME_CONFIG_MASTER_VOLUME_KEY, gPreferencesMasterVolume1);
    configSetInt(&gGameConfig, GAME_CONFIG_SOUND_KEY, GAME_CONFIG_MUSIC_VOLUME_KEY, gPreferencesMusicVolume1);
    configSetInt(&gGameConfig, GAME_CONFIG_SOUND_KEY, GAME_CONFIG_SNDFX_VOLUME_KEY, gPreferencesSoundEffectsVolume1);
    configSetInt(&gGameConfig, GAME_CONFIG_SOUND_KEY, GAME_CONFIG_SPEECH_VOLUME_KEY, gPreferencesSpeechVolume1);

    configSetDouble(&gGameConfig, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_BRIGHTNESS_KEY, gPreferencesBrightness1);
    configSetDouble(&gGameConfig, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_MOUSE_SENSITIVITY_KEY, gPreferencesMouseSensitivity1);

    if (save) {
        gameConfigSave();
    }

    return 0;
}

// 0x493224
int preferencesSave(File* stream)
{
    float textBaseDelay = (float)gPreferencesTextBaseDelay1;
    float brightness = (float)gPreferencesBrightness1;
    float mouseSensitivity = (float)gPreferencesMouseSensitivity1;

    if (fileWriteInt32(stream, gPreferencesGameDifficulty1) == -1) goto err;
    if (fileWriteInt32(stream, gPreferencesCombatDifficulty1) == -1) goto err;
    if (fileWriteInt32(stream, gPreferencesViolenceLevel1) == -1) goto err;
    if (fileWriteInt32(stream, gPreferencesTargetHighlight1) == -1) goto err;
    if (fileWriteInt32(stream, gPreferencesCombatLooks1) == -1) goto err;
    if (fileWriteInt32(stream, gPreferencesCombatMessages1) == -1) goto err;
    if (fileWriteInt32(stream, gPreferencesCombatTaunts1) == -1) goto err;
    if (fileWriteInt32(stream, gPreferencesLanguageFilter1) == -1) goto err;
    if (fileWriteInt32(stream, gPreferencesRunning1) == -1) goto err;
    if (fileWriteInt32(stream, gPreferencesSubtitles1) == -1) goto err;
    if (fileWriteInt32(stream, gPreferencesItemHighlight1) == -1) goto err;
    if (fileWriteInt32(stream, gPreferencesCombatSpeed1) == -1) goto err;
    if (fileWriteInt32(stream, gPreferencesPlayerSpeedup1) == -1) goto err;
    if (fileWriteFloat(stream, textBaseDelay) == -1) goto err;
    if (fileWriteInt32(stream, gPreferencesMasterVolume1) == -1) goto err;
    if (fileWriteInt32(stream, gPreferencesMusicVolume1) == -1) goto err;
    if (fileWriteInt32(stream, gPreferencesSoundEffectsVolume1) == -1) goto err;
    if (fileWriteInt32(stream, gPreferencesSpeechVolume1) == -1) goto err;
    if (fileWriteFloat(stream, brightness) == -1) goto err;
    if (fileWriteFloat(stream, mouseSensitivity) == -1) goto err;

    return 0;

err:

    debugPrint("\nOPTION MENU: Error save option data!\n");

    return -1;
}

// 0x49340C
int preferencesLoad(File* stream)
{
    float textBaseDelay;
    float brightness;
    float mouseSensitivity;

    preferencesSetDefaults(false);

    if (fileReadInt32(stream, &gPreferencesGameDifficulty1) == -1) goto err;
    if (fileReadInt32(stream, &gPreferencesCombatDifficulty1) == -1) goto err;
    if (fileReadInt32(stream, &gPreferencesViolenceLevel1) == -1) goto err;
    if (fileReadInt32(stream, &gPreferencesTargetHighlight1) == -1) goto err;
    if (fileReadInt32(stream, &gPreferencesCombatLooks1) == -1) goto err;
    if (fileReadInt32(stream, &gPreferencesCombatMessages1) == -1) goto err;
    if (fileReadInt32(stream, &gPreferencesCombatTaunts1) == -1) goto err;
    if (fileReadInt32(stream, &gPreferencesLanguageFilter1) == -1) goto err;
    if (fileReadInt32(stream, &gPreferencesRunning1) == -1) goto err;
    if (fileReadInt32(stream, &gPreferencesSubtitles1) == -1) goto err;
    if (fileReadInt32(stream, &gPreferencesItemHighlight1) == -1) goto err;
    if (fileReadInt32(stream, &gPreferencesCombatSpeed1) == -1) goto err;
    if (fileReadInt32(stream, &gPreferencesPlayerSpeedup1) == -1) goto err;
    if (fileReadFloat(stream, &textBaseDelay) == -1) goto err;
    if (fileReadInt32(stream, &gPreferencesMasterVolume1) == -1) goto err;
    if (fileReadInt32(stream, &gPreferencesMusicVolume1) == -1) goto err;
    if (fileReadInt32(stream, &gPreferencesSoundEffectsVolume1) == -1) goto err;
    if (fileReadInt32(stream, &gPreferencesSpeechVolume1) == -1) goto err;
    if (fileReadFloat(stream, &brightness) == -1) goto err;
    if (fileReadFloat(stream, &mouseSensitivity) == -1) goto err;

    gPreferencesBrightness1 = brightness;
    gPreferencesMouseSensitivity1 = mouseSensitivity;
    gPreferencesTextBaseDelay1 = textBaseDelay;

    _JustUpdate_();
    _SavePrefs(0);

    return 0;

err:

    debugPrint("\nOPTION MENU: Error loading option data!, using defaults.\n");

    preferencesSetDefaults(false);
    _JustUpdate_();
    _SavePrefs(0);

    return -1;
}

// 0x4928E4
void brightnessIncrease()
{
    gPreferencesBrightness1 = 1.0;
    configGetDouble(&gGameConfig, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_BRIGHTNESS_KEY, &gPreferencesBrightness1);

    if (gPreferencesBrightness1 < dbl_50C168) {
        gPreferencesBrightness1 += dbl_50C170;

        if (gPreferencesBrightness1 >= 1.0) {
            if (gPreferencesBrightness1 > dbl_50C168) {
                gPreferencesBrightness1 = dbl_50C168;
            }
        } else {
            gPreferencesBrightness1 = 1.0;
        }

        colorSetBrightness(gPreferencesBrightness1);

        configSetDouble(&gGameConfig, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_BRIGHTNESS_KEY, gPreferencesBrightness1);

        gameConfigSave();
    }
}

// 0x4929C8
void brightnessDecrease()
{
    gPreferencesBrightness1 = 1.0;
    configGetDouble(&gGameConfig, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_BRIGHTNESS_KEY, &gPreferencesBrightness1);

    if (gPreferencesBrightness1 > 1.0) {
        gPreferencesBrightness1 += dbl_50C178;

        if (gPreferencesBrightness1 >= 1.0) {
            if (gPreferencesBrightness1 > dbl_50C180) {
                gPreferencesBrightness1 = dbl_50C180;
            }
        } else {
            gPreferencesBrightness1 = 1.0;
        }

        colorSetBrightness(gPreferencesBrightness1);

        configSetDouble(&gGameConfig, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_BRIGHTNESS_KEY, gPreferencesBrightness1);

        gameConfigSave();
    }
}

// 0x4908A0
static int preferencesWindowInit()
{
    int i;
    int fid;
    char* messageItemText;
    int x;
    int y;
    int width;
    int height;
    int messageItemId;
    int btn;

    _SaveSettings();

    for (i = 0; i < PREFERENCES_WINDOW_FRM_COUNT; i++) {
        fid = buildFid(6, gPreferencesWindowFrmIds[i], 0, 0, 0);
        gPreferencesWindowFrmData[i] = artLockFrameDataReturningSize(fid, &(gPreferencesWindowFrmHandles[i]), &(gPreferencesWindowFrmSizes[i].width), &(gPreferencesWindowFrmSizes[i].height));
        if (gPreferencesWindowFrmData[i] == NULL) {
            for (; i != 0; i--) {
                artUnlock(gPreferencesWindowFrmHandles[i - 1]);
            }
            return -1;
        }
    }

    _changed = false;

    int preferencesWindowX = (screenGetWidth() - PREFERENCES_WINDOW_WIDTH) / 2;
    int preferencesWindowY = (screenGetHeight() - PREFERENCES_WINDOW_HEIGHT) / 2;
    gPreferencesWindow = windowCreate(preferencesWindowX,
        preferencesWindowY,
        PREFERENCES_WINDOW_WIDTH,
        PREFERENCES_WINDOW_HEIGHT,
        256,
        WINDOW_FLAG_0x10 | WINDOW_FLAG_0x02);
    if (gPreferencesWindow == -1) {
        for (i = 0; i < PREFERENCES_WINDOW_FRM_COUNT; i++) {
            artUnlock(gPreferencesWindowFrmHandles[i]);
        }
        return -1;
    }

    gPreferencesWindowBuffer = windowGetBuffer(gPreferencesWindow);
    memcpy(gPreferencesWindowBuffer,
        gPreferencesWindowFrmData[PREFERENCES_WINDOW_FRM_BACKGROUND],
        gPreferencesWindowFrmSizes[PREFERENCES_WINDOW_FRM_BACKGROUND].width * gPreferencesWindowFrmSizes[PREFERENCES_WINDOW_FRM_BACKGROUND].height);

    fontSetCurrent(104);

    messageItemText = getmsg(&gOptionsMessageList, &gOptionsMessageListItem, 100);
    fontDrawText(gPreferencesWindowBuffer + PREFERENCES_WINDOW_WIDTH * 10 + 74, messageItemText, PREFERENCES_WINDOW_WIDTH, PREFERENCES_WINDOW_WIDTH, _colorTable[18979]);

    fontSetCurrent(103);

    messageItemId = 101;
    for (i = 0; i < PRIMARY_PREF_COUNT; i++) {
        messageItemText = getmsg(&gOptionsMessageList, &gOptionsMessageListItem, messageItemId++);
        x = 99 - fontGetStringWidth(messageItemText) / 2;
        fontDrawText(gPreferencesWindowBuffer + PREFERENCES_WINDOW_WIDTH * _row1Ytab[i] + x, messageItemText, PREFERENCES_WINDOW_WIDTH, PREFERENCES_WINDOW_WIDTH, _colorTable[18979]);
    }

    for (i = 0; i < SECONDARY_PREF_COUNT; i++) {
        messageItemText = getmsg(&gOptionsMessageList, &gOptionsMessageListItem, messageItemId++);
        fontDrawText(gPreferencesWindowBuffer + PREFERENCES_WINDOW_WIDTH * _row2Ytab[i] + 206, messageItemText, PREFERENCES_WINDOW_WIDTH, PREFERENCES_WINDOW_WIDTH, _colorTable[18979]);
    }

    for (i = 0; i < RANGE_PREF_COUNT; i++) {
        messageItemText = getmsg(&gOptionsMessageList, &gOptionsMessageListItem, messageItemId++);
        fontDrawText(gPreferencesWindowBuffer + PREFERENCES_WINDOW_WIDTH * _row3Ytab[i] + 384, messageItemText, PREFERENCES_WINDOW_WIDTH, PREFERENCES_WINDOW_WIDTH, _colorTable[18979]);
    }

    // DEFAULT
    messageItemText = getmsg(&gOptionsMessageList, &gOptionsMessageListItem, 120);
    fontDrawText(gPreferencesWindowBuffer + PREFERENCES_WINDOW_WIDTH * 449 + 43, messageItemText, PREFERENCES_WINDOW_WIDTH, PREFERENCES_WINDOW_WIDTH, _colorTable[18979]);

    // DONE
    messageItemText = getmsg(&gOptionsMessageList, &gOptionsMessageListItem, 4);
    fontDrawText(gPreferencesWindowBuffer + PREFERENCES_WINDOW_WIDTH * 449 + 169, messageItemText, PREFERENCES_WINDOW_WIDTH, PREFERENCES_WINDOW_WIDTH, _colorTable[18979]);

    // CANCEL
    messageItemText = getmsg(&gOptionsMessageList, &gOptionsMessageListItem, 121);
    fontDrawText(gPreferencesWindowBuffer + PREFERENCES_WINDOW_WIDTH * 449 + 283, messageItemText, PREFERENCES_WINDOW_WIDTH, PREFERENCES_WINDOW_WIDTH, _colorTable[18979]);

    // Affect player speed
    messageItemText = getmsg(&gOptionsMessageList, &gOptionsMessageListItem, 122);
    fontDrawText(gPreferencesWindowBuffer + PREFERENCES_WINDOW_WIDTH * 72 + 405, messageItemText, PREFERENCES_WINDOW_WIDTH, PREFERENCES_WINDOW_WIDTH, _colorTable[18979]);

    for (i = 0; i < PREF_COUNT; i++) {
        _UpdateThing(i);
    }

    for (i = 0; i < PREF_COUNT; i++) {
        int mouseEnterEventCode;
        int mouseExitEventCode;
        int mouseDownEventCode;
        int mouseUpEventCode;

        if (i >= FIRST_RANGE_PREF) {
            x = 384;
            y = gPreferenceDescriptions[i].knobY - 12;
            width = 240;
            height = 23;
            mouseEnterEventCode = 526;
            mouseExitEventCode = 526;
            mouseDownEventCode = 505 + i;
            mouseUpEventCode = 526;

        } else if (i >= FIRST_SECONDARY_PREF) {
            x = gPreferenceDescriptions[i].minX;
            y = gPreferenceDescriptions[i].knobY - 5;
            width = gPreferenceDescriptions[i].maxX - x;
            height = 28;
            mouseEnterEventCode = -1;
            mouseExitEventCode = -1;
            mouseDownEventCode = -1;
            mouseUpEventCode = 505 + i;
        } else {
            x = gPreferenceDescriptions[i].minX;
            y = gPreferenceDescriptions[i].knobY - 4;
            width = gPreferenceDescriptions[i].maxX - x;
            height = 48;
            mouseEnterEventCode = -1;
            mouseExitEventCode = -1;
            mouseDownEventCode = -1;
            mouseUpEventCode = 505 + i;
        }

        gPreferenceDescriptions[i].btn = buttonCreate(gPreferencesWindow, x, y, width, height, mouseEnterEventCode, mouseExitEventCode, mouseDownEventCode, mouseUpEventCode, NULL, NULL, NULL, 32);
    }

    _plyrspdbid = buttonCreate(gPreferencesWindow,
        383,
        68,
        gPreferencesWindowFrmSizes[PREFERENCES_WINDOW_FRM_CHECKBOX_OFF].width,
        gPreferencesWindowFrmSizes[PREFERENCES_WINDOW_FRM_CHECKBOX_ON].height,
        -1,
        -1,
        524,
        524,
        gPreferencesWindowFrmData[PREFERENCES_WINDOW_FRM_CHECKBOX_OFF],
        gPreferencesWindowFrmData[PREFERENCES_WINDOW_FRM_CHECKBOX_ON],
        NULL,
        BUTTON_FLAG_TRANSPARENT | BUTTON_FLAG_0x01 | BUTTON_FLAG_0x02);
    if (_plyrspdbid != -1) {
        _win_set_button_rest_state(_plyrspdbid, gPreferencesPlayerSpeedup1, 0);
    }

    buttonSetCallbacks(_plyrspdbid, _gsound_med_butt_press, _gsound_med_butt_press);

    // DEFAULT
    btn = buttonCreate(gPreferencesWindow,
        23,
        450,
        gPreferencesWindowFrmSizes[PREFERENCES_WINDOW_FRM_LITTLE_RED_BUTTON_UP].width,
        gPreferencesWindowFrmSizes[PREFERENCES_WINDOW_FRM_LITTLE_RED_BUTTON_DOWN].height,
        -1,
        -1,
        -1,
        527,
        gPreferencesWindowFrmData[PREFERENCES_WINDOW_FRM_LITTLE_RED_BUTTON_UP],
        gPreferencesWindowFrmData[PREFERENCES_WINDOW_FRM_LITTLE_RED_BUTTON_DOWN],
        NULL,
        BUTTON_FLAG_TRANSPARENT);
    if (btn != -1) {
        buttonSetCallbacks(btn, _gsound_red_butt_press, _gsound_red_butt_release);
    }

    // DONE
    btn = buttonCreate(gPreferencesWindow,
        148,
        450,
        gPreferencesWindowFrmSizes[PREFERENCES_WINDOW_FRM_LITTLE_RED_BUTTON_UP].width,
        gPreferencesWindowFrmSizes[PREFERENCES_WINDOW_FRM_LITTLE_RED_BUTTON_DOWN].height,
        -1,
        -1,
        -1,
        504,
        gPreferencesWindowFrmData[PREFERENCES_WINDOW_FRM_LITTLE_RED_BUTTON_UP],
        gPreferencesWindowFrmData[PREFERENCES_WINDOW_FRM_LITTLE_RED_BUTTON_DOWN],
        NULL,
        BUTTON_FLAG_TRANSPARENT);
    if (btn != -1) {
        buttonSetCallbacks(btn, _gsound_red_butt_press, _gsound_red_butt_release);
    }

    // CANCEL
    btn = buttonCreate(gPreferencesWindow,
        263,
        450,
        gPreferencesWindowFrmSizes[PREFERENCES_WINDOW_FRM_LITTLE_RED_BUTTON_UP].width,
        gPreferencesWindowFrmSizes[PREFERENCES_WINDOW_FRM_LITTLE_RED_BUTTON_DOWN].height,
        -1,
        -1,
        -1,
        528,
        gPreferencesWindowFrmData[PREFERENCES_WINDOW_FRM_LITTLE_RED_BUTTON_UP],
        gPreferencesWindowFrmData[PREFERENCES_WINDOW_FRM_LITTLE_RED_BUTTON_DOWN],
        NULL,
        BUTTON_FLAG_TRANSPARENT);
    if (btn != -1) {
        buttonSetCallbacks(btn, _gsound_red_butt_press, _gsound_red_butt_release);
    }

    fontSetCurrent(101);

    windowRefresh(gPreferencesWindow);

    return 0;
}

// 0x492870
static int preferencesWindowFree()
{
    if (_changed) {
        _SavePrefs(1);
        _JustUpdate_();
        _combat_highlight_change();
    }

    windowDestroy(gPreferencesWindow);

    for (int index = 0; index < PREFERENCES_WINDOW_FRM_COUNT; index++) {
        artUnlock(gPreferencesWindowFrmHandles[index]);
    }

    return 0;
}

// 0x490798
static int _do_prefscreen()
{
    if (preferencesWindowInit() == -1) {
        debugPrint("\nPREFERENCE MENU: Error loading preference dialog data!\n");
        return -1;
    }

    int rc = -1;
    while (rc == -1) {
        int eventCode = _get_input();

        switch (eventCode) {
        case KEY_RETURN:
        case KEY_UPPERCASE_P:
        case KEY_LOWERCASE_P:
            soundPlayFile("ib1p1xx1");
            // FALLTHROUGH
        case 504:
            rc = 1;
            break;
        case KEY_CTRL_Q:
        case KEY_CTRL_X:
        case KEY_F10:
            showQuitConfirmationDialog();
            break;
        case KEY_EQUAL:
        case KEY_PLUS:
            brightnessIncrease();
            break;
        case KEY_MINUS:
        case KEY_UNDERSCORE:
            brightnessDecrease();
            break;
        case KEY_F12:
            takeScreenshot();
            break;
        case 527:
            preferencesSetDefaults(true);
            break;
        default:
            if (eventCode == KEY_ESCAPE || eventCode == 528 || _game_user_wants_to_quit != 0) {
                _RestoreSettings();
                rc = 0;
            } else if (eventCode >= 505 && eventCode <= 524) {
                _DoThing(eventCode);
            }
            break;
        }
    }

    preferencesWindowFree();

    return rc;
}

// 0x490E8C
static void _DoThing(int eventCode)
{
    int x;
    int y;
    mouseGetPositionInWindow(gPreferencesWindow, &x, &y);

    // This preference index also contains out-of-bounds value 19,
    // which is the only preference expressed as checkbox.
    int preferenceIndex = eventCode - 505;

    if (preferenceIndex >= FIRST_PRIMARY_PREF && preferenceIndex <= LAST_PRIMARY_PREF) {
        PreferenceDescription* meta = &(gPreferenceDescriptions[preferenceIndex]);
        int* valuePtr = meta->valuePtr;
        int value = *valuePtr;
        bool valueChanged = false;

        int v1 = meta->knobX + 23;
        int v2 = meta->knobY + 21;

        if (sqrt(pow((double)x - (double)v1, 2) + pow((double)y - (double)v2, 2)) > 16.0) {
            if (y > meta->knobY) {
                int v14 = meta->knobY + word_48FBFE[0];
                if (y >= v14 && y <= v14 + fontGetLineHeight()) {
                    if (x >= meta->minX && x <= meta->knobX) {
                        *valuePtr = 0;
                        meta->direction = 0;
                        valueChanged = true;
                    } else {
                        if (meta->valuesCount >= 3 && x >= meta->knobX + word_48FBF6[2] && x <= meta->maxX) {
                            *valuePtr = 2;
                            meta->direction = 0;
                            valueChanged = true;
                        }
                    }
                }
            } else {
                if (x >= meta->knobX + 9 && x <= meta->knobX + 37) {
                    *valuePtr = 1;
                    if (value != 0) {
                        meta->direction = 1;
                    } else {
                        meta->direction = 0;
                    }
                    valueChanged = true;
                }
            }

            if (meta->valuesCount == 4) {
                int v19 = meta->knobY + word_48FBFE[3];
                if (y >= v19 && y <= v19 + 2 * fontGetLineHeight() && x >= meta->knobX + word_48FBF6[3] && x <= meta->maxX) {
                    *valuePtr = 3;
                    meta->direction = 1;
                    valueChanged = true;
                }
            }
        } else {
            if (meta->direction != 0) {
                if (value == 0) {
                    meta->direction = 0;
                }
            } else {
                if (value == meta->valuesCount - 1) {
                    meta->direction = 1;
                }
            }

            if (meta->direction != 0) {
                *valuePtr = value - 1;
            } else {
                *valuePtr = value + 1;
            }

            valueChanged = true;
        }

        if (valueChanged) {
            soundPlayFile("ib3p1xx1");
            coreDelay(70);
            soundPlayFile("ib3lu1x1");
            _UpdateThing(preferenceIndex);
            windowRefresh(gPreferencesWindow);
            _changed = true;
            return;
        }
    } else if (preferenceIndex >= FIRST_SECONDARY_PREF && preferenceIndex <= LAST_SECONDARY_PREF) {
        PreferenceDescription* meta = &(gPreferenceDescriptions[preferenceIndex]);
        int* valuePtr = meta->valuePtr;
        int value = *valuePtr;
        bool valueChanged = false;

        int v1 = meta->knobX + 11;
        int v2 = meta->knobY + 12;

        if (sqrt(pow((double)x - (double)v1, 2) + pow((double)y - (double)v2, 2)) > 10.0) {
            int v23 = meta->knobY - 5;
            if (y >= v23 && y <= v23 + fontGetLineHeight() + 2) {
                if (x >= meta->minX && x <= meta->knobX) {
                    *valuePtr = preferenceIndex == PREF_COMBAT_MESSAGES ? 1 : 0;
                    valueChanged = true;
                } else if (x >= meta->knobX + 22.0 && x <= meta->maxX) {
                    *valuePtr = preferenceIndex == PREF_COMBAT_MESSAGES ? 0 : 1;
                    valueChanged = true;
                }
            }
        } else {
            *valuePtr ^= 1;
            valueChanged = true;
        }

        if (valueChanged) {
            soundPlayFile("ib2p1xx1");
            coreDelay(70);
            soundPlayFile("ib2lu1x1");
            _UpdateThing(preferenceIndex);
            windowRefresh(gPreferencesWindow);
            _changed = true;
            return;
        }
    } else if (preferenceIndex >= FIRST_RANGE_PREF && preferenceIndex <= LAST_RANGE_PREF) {
        PreferenceDescription* meta = &(gPreferenceDescriptions[preferenceIndex]);
        int* valuePtr = meta->valuePtr;

        soundPlayFile("ib1p1xx1");

        double value;
        switch (preferenceIndex) {
        case PREF_TEXT_BASE_DELAY:
            value = 6.0 - gPreferencesTextBaseDelay1 + 1.0;
            break;
        case PREF_BRIGHTNESS:
            value = gPreferencesBrightness1;
            break;
        case PREF_MOUSE_SENSITIVIY:
            value = gPreferencesMouseSensitivity1;
            break;
        default:
            value = *valuePtr;
            break;
        }

        int knobX = (int)(219.0 / (meta->maxValue - meta->minValue));
        int v31 = (int)((value - meta->minValue) * (219.0 / (meta->maxValue - meta->minValue)) + 384.0);
        blitBufferToBuffer(gPreferencesWindowFrmData[PREFERENCES_WINDOW_FRM_BACKGROUND] + PREFERENCES_WINDOW_WIDTH * meta->knobY + 384, 240, 12, PREFERENCES_WINDOW_WIDTH, gPreferencesWindowBuffer + PREFERENCES_WINDOW_WIDTH * meta->knobY + 384, PREFERENCES_WINDOW_WIDTH);
        blitBufferToBufferTrans(gPreferencesWindowFrmData[PREFERENCES_WINDOW_FRM_KNOB_ON], 21, 12, 21, gPreferencesWindowBuffer + PREFERENCES_WINDOW_WIDTH * meta->knobY + v31, PREFERENCES_WINDOW_WIDTH);

        windowRefresh(gPreferencesWindow);

        int sfxVolumeExample = 0;
        int speechVolumeExample = 0;
        while (true) {
            _get_input();

            int tick = _get_time();

            mouseGetPositionInWindow(gPreferencesWindow, &x, &y);

            if (mouseGetEvent() & 0x10) {
                soundPlayFile("ib1lu1x1");
                _UpdateThing(preferenceIndex);
                windowRefresh(gPreferencesWindow);
                _changed = true;
                return;
            }

            if (v31 + 14 > x) {
                if (v31 + 6 > x) {
                    v31 = x - 6;
                    if (v31 < 384) {
                        v31 = 384;
                    }
                }
            } else {
                v31 = x - 6;
                if (v31 > 603) {
                    v31 = 603;
                }
            }

            double newValue = ((double)v31 - 384.0) / (219.0 / (meta->maxValue - meta->minValue)) + meta->minValue;

            int v52 = 0;

            switch (preferenceIndex) {
            case PREF_COMBAT_SPEED:
                *meta->valuePtr = (int)newValue;
                break;
            case PREF_TEXT_BASE_DELAY:
                gPreferencesTextBaseDelay1 = 6.0 - newValue + 1.0;
                break;
            case PREF_MASTER_VOLUME:
                *meta->valuePtr = (int)newValue;
                gameSoundSetMasterVolume(gPreferencesMasterVolume1);
                v52 = 1;
                break;
            case PREF_MUSIC_VOLUME:
                *meta->valuePtr = (int)newValue;
                backgroundSoundSetVolume(gPreferencesMusicVolume1);
                v52 = 1;
                break;
            case PREF_SFX_VOLUME:
                *meta->valuePtr = (int)newValue;
                soundEffectsSetVolume(gPreferencesSoundEffectsVolume1);
                v52 = 1;
                if (sfxVolumeExample == 0) {
                    soundPlayFile("butin1");
                    sfxVolumeExample = 7;
                } else {
                    sfxVolumeExample--;
                }
                break;
            case PREF_SPEECH_VOLUME:
                *meta->valuePtr = (int)newValue;
                speechSetVolume(gPreferencesSpeechVolume1);
                v52 = 1;
                if (speechVolumeExample == 0) {
                    speechLoad("narrator\\options", 12, 13, 15);
                    speechVolumeExample = 40;
                } else {
                    speechVolumeExample--;
                }
                break;
            case PREF_BRIGHTNESS:
                gPreferencesBrightness1 = newValue;
                colorSetBrightness(newValue);
                break;
            case PREF_MOUSE_SENSITIVIY:
                gPreferencesMouseSensitivity1 = newValue;
                break;
            }

            if (v52) {
                int off = PREFERENCES_WINDOW_WIDTH * (meta->knobY - 12) + 384;
                blitBufferToBuffer(gPreferencesWindowFrmData[PREFERENCES_WINDOW_FRM_BACKGROUND] + off, 240, 24, PREFERENCES_WINDOW_WIDTH, gPreferencesWindowBuffer + off, PREFERENCES_WINDOW_WIDTH);

                for (int optionIndex = 0; optionIndex < meta->valuesCount; optionIndex++) {
                    const char* str = getmsg(&gOptionsMessageList, &gOptionsMessageListItem, meta->labelIds[optionIndex]);

                    int x;
                    switch (optionIndex) {
                    case 0:
                        // 0x4926AA
                        x = 384;
                        // TODO: Incomplete.
                        break;
                    case 1:
                        // 0x4926F3
                        switch (meta->valuesCount) {
                        case 2:
                            x = 624 - fontGetStringWidth(str);
                            break;
                        case 3:
                            // This code path does not use floating-point arithmetic
                            x = 504 - fontGetStringWidth(str) / 2 - 2;
                            break;
                        case 4:
                            // Uses floating-point arithmetic
                            x = 444 + fontGetStringWidth(str) / 2 - 8;
                            break;
                        }
                        break;
                    case 2:
                        // 0x492766
                        switch (meta->valuesCount) {
                        case 3:
                            x = 624 - fontGetStringWidth(str);
                            break;
                        case 4:
                            // Uses floating-point arithmetic
                            x = 564 - fontGetStringWidth(str) - 4;
                            break;
                        }
                        break;
                    case 3:
                        // 0x49279E
                        x = 624 - fontGetStringWidth(str);
                        break;
                    }
                    fontDrawText(gPreferencesWindowBuffer + PREFERENCES_WINDOW_WIDTH * (meta->knobY - 12) + x, str, PREFERENCES_WINDOW_WIDTH, PREFERENCES_WINDOW_WIDTH, _colorTable[18979]);
                }
            } else {
                int off = PREFERENCES_WINDOW_WIDTH * meta->knobY + 384;
                blitBufferToBuffer(gPreferencesWindowFrmData[PREFERENCES_WINDOW_FRM_BACKGROUND] + off, 240, 12, PREFERENCES_WINDOW_WIDTH, gPreferencesWindowBuffer + off, PREFERENCES_WINDOW_WIDTH);
            }

            blitBufferToBufferTrans(gPreferencesWindowFrmData[PREFERENCES_WINDOW_FRM_KNOB_ON], 21, 12, 21, gPreferencesWindowBuffer + PREFERENCES_WINDOW_WIDTH * meta->knobY + v31, PREFERENCES_WINDOW_WIDTH);
            windowRefresh(gPreferencesWindow);

            while (getTicksSince(tick) < 35)
                ;
        }
    } else if (preferenceIndex == 19) {
        gPreferencesPlayerSpeedup1 ^= 1;
    }

    _changed = true;
}
