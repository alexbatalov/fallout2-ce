#ifndef ENDGAME_H
#define ENDGAME_H

#include "platform_compat.h"

typedef enum EndgameDeathEndingReason {
    // Dude died.
    ENDGAME_DEATH_ENDING_REASON_DEATH = 0,

    // 13 years passed.
    ENDGAME_DEATH_ENDING_REASON_TIMEOUT = 2,
} EndgameDeathEndingReason;

typedef struct EndgameDeathEnding {
    int gvar;
    int value;
    int worldAreaKnown;
    int worldAreaNotKnown;
    int min_level;
    int percentage;
    char voiceOverBaseName[16];

    // This flag denotes that the conditions for this ending is met and it was
    // selected as a candidate for final random selection.
    bool enabled;
} EndgameDeathEnding;

typedef struct EndgameEnding {
    int gvar;
    int value;
    int art_num;
    char voiceOverBaseName[12];
    int direction;
} EndgameEnding;

extern char _aEnglish_2[];

extern int gEndgameEndingSubtitlesLength;
extern int gEndgameEndingSubtitlesCharactersCount;
extern int gEndgameEndingSubtitlesCurrentLine;
extern int _endgame_maybe_done;
extern EndgameDeathEnding* gEndgameDeathEndings;
extern int gEndgameDeathEndingsLength;

extern char gEndgameDeathEndingFileName[40];
extern bool gEndgameEndingVoiceOverSpeechLoaded;
extern char gEndgameEndingSubtitlesLocalizedPath[COMPAT_MAX_PATH];
extern bool gEndgameEndingSpeechEnded;
extern EndgameEnding* gEndgameEndings;
extern char** gEndgameEndingSubtitles;
extern bool gEndgameEndingSubtitlesEnabled;
extern bool gEndgameEndingSubtitlesEnded;
extern bool _endgame_map_enabled;
extern bool _endgame_mouse_state;
extern int gEndgameEndingsLength;
extern bool gEndgameEndingVoiceOverSubtitlesLoaded;
extern unsigned int gEndgameEndingSubtitlesReferenceTime;
extern unsigned int* gEndgameEndingSubtitlesTimings;
extern int gEndgameEndingSlideshowOldFont;
extern unsigned char* gEndgameEndingSlideshowWindowBuffer;
extern int gEndgameEndingSlideshowWindow;

void endgamePlaySlideshow();
void endgamePlayMovie();
void endgameEndingRenderPanningScene(int direction, const char* narratorFileName);
void endgameEndingRenderStaticScene(int fid, const char* narratorFileName);
int endgameEndingHandleContinuePlaying();
int endgameEndingSlideshowWindowInit();
void endgameEndingSlideshowWindowFree();
void endgameEndingVoiceOverInit(const char* fname);
void endgameEndingVoiceOverReset();
void endgameEndingVoiceOverFree();
void endgameEndingLoadPalette(int type, int id);
void _endgame_voiceover_callback();
int endgameEndingSubtitlesLoad(const char* filePath);
void endgameEndingRefreshSubtitles();
void endgameEndingSubtitlesFree();
void _endgame_movie_callback();
void _endgame_movie_bk_process();
int endgameEndingInit();
void endgameEndingFree();
int endgameDeathEndingInit();
int endgameDeathEndingExit();
void endgameSetupDeathEnding(int reason);
int endgameDeathEndingValidate(int* percentage);
char* endgameDeathEndingGetFileName();

#endif /* ENDGAME_H */
