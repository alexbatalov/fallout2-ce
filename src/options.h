#ifndef OPTIONS_H
#define OPTIONS_H

#include "db.h"

extern int gPreferencesSoundEffectsVolume1;
extern int gPreferencesSubtitles1;
extern int gPreferencesLanguageFilter1;
extern int gPreferencesSpeechVolume1;
extern int gPreferencesMasterVolume1;
extern int gPreferencesPlayerSpeedup1;
extern int gPreferencesCombatTaunts1;
extern int gPreferencesMusicVolume1;
extern int gPreferencesRunning1;
extern int gPreferencesCombatSpeed1;
extern int gPreferencesItemHighlight1;
extern int gPreferencesCombatMessages1;
extern int gPreferencesTargetHighlight1;
extern int gPreferencesCombatDifficulty1;
extern int gPreferencesViolenceLevel1;
extern int gPreferencesGameDifficulty1;
extern int gPreferencesCombatLooks1;

int showOptions();
int showOptionsWithInitialKeyCode(int initialKeyCode);
int showPause(bool a1);
int _init_options_menu();
int preferencesSave(File* stream);
int preferencesLoad(File* stream);
void brightnessIncrease();
void brightnessDecrease();
int _do_options();

#endif /* OPTIONS_H */
