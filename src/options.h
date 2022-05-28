#ifndef OPTIONS_H
#define OPTIONS_H

#include "art.h"
#include "db.h"
#include "message.h"
#include "geometry.h"

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

extern const int _row1Ytab[5];
extern const int _row2Ytab[6];
extern const int _row3Ytab[8];
extern const short word_48FBF6[PRIMARY_OPTION_VALUE_COUNT];
extern const short word_48FBFE[PRIMARY_OPTION_VALUE_COUNT];
extern const short word_48FC06[SECONDARY_OPTION_VALUE_COUNT];
extern const int gPauseWindowFrmIds[PAUSE_WINDOW_FRM_COUNT];
extern const int dword_48FC30[SECONDARY_PREF_COUNT];
extern const int dword_48FC1C[PRIMARY_PREF_COUNT];

extern const double dbl_50C168;
extern const double dbl_50C170;
extern const double dbl_50C178;
extern const double dbl_50C180;
extern const double dbl_50C2D0;
extern const double dbl_50C2D8;
extern const double dbl_50C2E0;

extern const int gOptionsWindowFrmIds[OPTIONS_WINDOW_FRM_COUNT];
extern const int gPreferencesWindowFrmIds[PREFERENCES_WINDOW_FRM_COUNT];
extern PreferenceDescription gPreferenceDescriptions[PREF_COUNT];

extern Size gOptionsWindowFrmSizes[OPTIONS_WINDOW_FRM_COUNT];
extern MessageList gOptionsMessageList;
extern Size gPreferencesWindowFrmSizes[PREFERENCES_WINDOW_FRM_COUNT];
extern MessageListItem gOptionsMessageListItem;
extern unsigned char* gPreferencesWindowFrmData[PREFERENCES_WINDOW_FRM_COUNT];
extern unsigned char* _opbtns[OPTIONS_WINDOW_BUTTONS_COUNT];
extern CacheEntry* gPreferencesWindowFrmHandles[PREFERENCES_WINDOW_FRM_COUNT];
extern double gPreferencesTextBaseDelay2;
extern double gPreferencesBrightness1;
extern double gPreferencesBrightness2;
extern double gPreferencesTextBaseDelay1;
extern double gPreferencesMouseSensitivity1;
extern double gPreferencesMouseSensitivity2;
extern unsigned char* gPreferencesWindowBuffer;
extern bool gOptionsWindowGameMouseObjectsWasVisible;
extern int gOptionsWindow;
extern int gPreferencesWindow;
extern unsigned char* gOptionsWindowBuffer;
extern CacheEntry* gOptionsWindowFrmHandles[OPTIONS_WINDOW_FRM_COUNT];
extern unsigned char* gOptionsWindowFrmData[OPTIONS_WINDOW_FRM_COUNT];
extern int gPreferencesGameDifficulty2;
extern int gPreferencesCombatDifficulty2;
extern int gPreferencesViolenceLevel2;
extern int gPreferencesTargetHighlight2;
extern int gPreferencesCombatLooks2;
extern int gPreferencesCombatMessages2;
extern int gPreferencesCombatTaunts2;
extern int gPreferencesLanguageFilter2;
extern int gPreferencesRunning2;
extern int gPreferencesSubtitles2;
extern int gPreferencesItemHighlight2;
extern int gPreferencesCombatSpeed2;
extern int gPreferencesPlayerSpeedup2;
extern int gPreferencesMasterVolume2;
extern int gPreferencesMusicVolume2;
extern int gPreferencesSoundEffectsVolume2;
extern int gPreferencesSpeechVolume2;
extern int gPreferencesSoundEffectsVolume1;
extern int gPreferencesSubtitles1;
extern int gPreferencesLanguageFilter1;
extern int gPreferencesSpeechVolume1;
extern int gPreferencesMasterVolume1;
extern int gPreferencesPlayerSpeedup1;
extern int gPreferencesCombatTaunts1;
extern int gOptionsWindowOldFont;
extern int gPreferencesMusicVolume1;
extern bool gOptionsWindowIsoWasEnabled;
extern int gPreferencesRunning1;
extern int gPreferencesCombatSpeed1;
extern int _plyrspdbid;
extern int gPreferencesItemHighlight1;
extern bool _changed;
extern int gPreferencesCombatMessages1;
extern int gPreferencesTargetHighlight1;
extern int gPreferencesCombatDifficulty1;
extern int gPreferencesViolenceLevel1;
extern int gPreferencesGameDifficulty1;
extern int gPreferencesCombatLooks1;

int showOptions();
int showOptionsWithInitialKeyCode(int initialKeyCode);
int optionsWindowInit();
int optionsWindowFree();
int showPause(bool a1);
void _ShadeScreen(bool a1);
void _SetSystemPrefs();
void _SaveSettings();
void _RestoreSettings();
void preferencesSetDefaults(bool a1);
void _JustUpdate_();
void _UpdateThing(int index);
int _init_options_menu();
int _SavePrefs(bool save);
int preferencesSave(File* stream);
int preferencesLoad(File* stream);
void brightnessIncrease();
void brightnessDecrease();
int preferencesWindowInit();
int preferencesWindowFree();
int _do_prefscreen();
void _DoThing(int eventCode);
void brightnessIncrease();
void brightnessDecrease();
int _do_options();

#endif /* OPTIONS_H */
