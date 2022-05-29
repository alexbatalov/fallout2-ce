#include "endgame.h"

#include "color.h"
#include "core.h"
#include "credits.h"
#include "cycle.h"
#include "db.h"
#include "dbox.h"
#include "debug.h"
#include "draw.h"
#include "game.h"
#include "game_config.h"
#include "game_mouse.h"
#include "game_movie.h"
#include "game_sound.h"
#include "map.h"
#include "memory.h"
#include "object.h"
#include "palette.h"
#include "pipboy.h"
#include "random.h"
#include "stat.h"
#include "text_font.h"
#include "window_manager.h"
#include "word_wrap.h"
#include "world_map.h"

#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

// The maximum number of subtitle lines per slide.
#define ENDGAME_ENDING_MAX_SUBTITLES (50)

#define ENDGAME_ENDING_WINDOW_WIDTH 640
#define ENDGAME_ENDING_WINDOW_HEIGHT 480

// 0x50B00C
char _aEnglish_2[] = ENGLISH;

// The number of lines in current subtitles file.
//
// It's used as a length for two arrays:
// - [gEndgameEndingSubtitles]
// - [gEndgameEndingSubtitlesTimings]
//
// This value does not exceed [ENDGAME_ENDING_SUBTITLES_CAPACITY].
//
// 0x518668
int gEndgameEndingSubtitlesLength = 0;

// The number of characters in current subtitles file.
//
// This value is used to determine
//
// 0x51866C
int gEndgameEndingSubtitlesCharactersCount = 0;

// 0x518670
int gEndgameEndingSubtitlesCurrentLine = 0;

// 0x518674
int _endgame_maybe_done = 0;

// enddeath.txt
//
// 0x518678
EndgameDeathEnding* gEndgameDeathEndings = NULL;

// The number of death endings in [gEndgameDeathEndings] array.
//
// 0x51867C
int gEndgameDeathEndingsLength = 0;

// Base file name for death ending.
//
// This value does not include extension.
//
// 0x570A90
char gEndgameDeathEndingFileName[40];

// This flag denotes whether speech sound was successfully loaded for
// the current slide.
//
// 0x570AB8
bool gEndgameEndingVoiceOverSpeechLoaded;

// 0x570ABC
char gEndgameEndingSubtitlesLocalizedPath[COMPAT_MAX_PATH];

// The flag used to denote voice over speech for current slide has ended.
//
// 0x570BC0
bool gEndgameEndingSpeechEnded;

// endgame.txt
//
// 0x570BC4
EndgameEnding* gEndgameEndings;

// The array of text lines in current subtitles file.
//
// The length is specified in [gEndgameEndingSubtitlesLength]. It's capacity
// is [ENDGAME_ENDING_SUBTITLES_CAPACITY].
//
// 0x570BC8
char** gEndgameEndingSubtitles;

// 0x570BCC
bool gEndgameEndingSubtitlesEnabled;

// The flag used to denote voice over subtitles for current slide has ended.
//
// 0x570BD0
bool gEndgameEndingSubtitlesEnded;

// 0x570BD4
bool _endgame_map_enabled;

// 0x570BD8
bool _endgame_mouse_state;

// The number of endings in [gEndgameEndings] array.
//
// 0x570BDC
int gEndgameEndingsLength = 0;

// This flag denotes whether subtitles was successfully loaded for
// the current slide.
//
// 0x570BE0
bool gEndgameEndingVoiceOverSubtitlesLoaded;

// Reference time is a timestamp when subtitle is first displayed.
//
// This value is used together with [gEndgameEndingSubtitlesTimings] array to
// determine when next line needs to be displayed.
//
// 0x570BE4
unsigned int gEndgameEndingSubtitlesReferenceTime;

// The array of timings for each line in current subtitles file.
//
// The length is specified in [gEndgameEndingSubtitlesLength]. It's capacity
// is [ENDGAME_ENDING_SUBTITLES_CAPACITY].
//
// 0x570BE8
unsigned int* gEndgameEndingSubtitlesTimings;

// Font that was current before endgame slideshow window was created.
//
// 0x570BEC
int gEndgameEndingSlideshowOldFont;

// 0x570BF0
unsigned char* gEndgameEndingSlideshowWindowBuffer;

// 0x570BF4
int gEndgameEndingSlideshowWindow;

// 0x43F788
void endgamePlaySlideshow()
{
    if (endgameEndingSlideshowWindowInit() == -1) {
        return;
    }

    for (int index = 0; index < gEndgameEndingsLength; index++) {
        EndgameEnding* ending = &(gEndgameEndings[index]);
        int value = gameGetGlobalVar(ending->gvar);
        if (value == ending->value) {
            if (ending->art_num == 327) {
                endgameEndingRenderPanningScene(ending->direction, ending->voiceOverBaseName);
            } else {
                int fid = buildFid(6, ending->art_num, 0, 0, 0);
                endgameEndingRenderStaticScene(fid, ending->voiceOverBaseName);
            }
        }
    }

    endgameEndingSlideshowWindowFree();
}

// 0x43F810
void endgamePlayMovie()
{
    backgroundSoundDelete();
    isoDisable();
    paletteFadeTo(gPaletteBlack);
    _endgame_maybe_done = 0;
    tickersAdd(_endgame_movie_bk_process);
    backgroundSoundSetEndCallback(_endgame_movie_callback);
    backgroundSoundLoad("akiss", 12, 14, 15);
    coreDelayProcessingEvents(3000);

    // NOTE: Result is ignored. I guess there was some kind of switch for male
    // vs. female ending, but it was not implemented.
    critterGetStat(gDude, STAT_GENDER);

    creditsOpen("credits.txt", -1, false);
    backgroundSoundDelete();
    backgroundSoundSetEndCallback(NULL);
    tickersRemove(_endgame_movie_bk_process);
    backgroundSoundDelete();
    colorPaletteLoad("color.pal");
    paletteFadeTo(_cmap);
    isoEnable();
    endgameEndingHandleContinuePlaying();
}

// 0x43F8C4
int endgameEndingHandleContinuePlaying()
{
    bool isoWasEnabled = isoDisable();

    bool gameMouseWasVisible;
    if (isoWasEnabled) {
        gameMouseWasVisible = gameMouseObjectsIsVisible();
    } else {
        gameMouseWasVisible = false;
    }

    if (gameMouseWasVisible) {
        gameMouseObjectsHide();
    }

    bool oldCursorIsHidden = cursorIsHidden();
    if (oldCursorIsHidden) {
        mouseShowCursor();
    }

    int oldCursor = gameMouseGetCursor();
    gameMouseSetCursor(MOUSE_CURSOR_ARROW);

    int rc;

    MessageListItem messageListItem;
    messageListItem.num = 30;
    if (messageListGetItem(&gMiscMessageList, &messageListItem)) {
        rc = showDialogBox(messageListItem.text, NULL, 0, 169, 117, _colorTable[32328], NULL, _colorTable[32328], DIALOG_BOX_YES_NO);
        if (rc == 0) {
            _game_user_wants_to_quit = 2;
        }
    } else {
        rc = -1;
    }

    gameMouseSetCursor(oldCursor);
    if (oldCursorIsHidden) {
        mouseHideCursor();
    }

    if (gameMouseWasVisible) {
        gameMouseObjectsShow();
    }

    if (isoWasEnabled) {
        isoEnable();
    }

    return rc;
}

// 0x43FBDC
void endgameEndingRenderPanningScene(int direction, const char* narratorFileName)
{
    int fid = buildFid(6, 327, 0, 0, 0);

    CacheEntry* backgroundHandle;
    Art* background = artLock(fid, &backgroundHandle);
    if (background != NULL) {
        int width = artGetWidth(background, 0, 0);
        int height = artGetHeight(background, 0, 0);
        unsigned char* backgroundData = artGetFrameData(background, 0, 0);
        bufferFill(gEndgameEndingSlideshowWindowBuffer, ENDGAME_ENDING_WINDOW_WIDTH, ENDGAME_ENDING_WINDOW_HEIGHT, ENDGAME_ENDING_WINDOW_WIDTH, _colorTable[0]);
        endgameEndingLoadPalette(6, 327);

        unsigned char palette[768];
        memcpy(palette, _cmap, 768);

        paletteSetEntries(gPaletteBlack);
        endgameEndingVoiceOverInit(narratorFileName);

        // TODO: Unclear math.
        int v8 = width - 640;
        int v32 = v8 / 4;
        unsigned int v9 = 16 * v8 / v8;
        unsigned int v9_ = 16 * v8;

        if (gEndgameEndingVoiceOverSpeechLoaded) {
            unsigned int v10 = 1000 * speechGetDuration();
            if (v10 > v9_ / 2) {
                v9 = (v10 + v9 * (v8 / 2)) / v8;
            }
        }

        int start;
        int end;
        if (direction == -1) {
            start = width - 640;
            end = 0;
        } else {
            start = 0;
            end = width - 640;
        }

        tickersDisable();

        bool subtitlesLoaded = false;

        unsigned int since = 0;
        while (start != end) {
            int v12 = 640 - v32;

            // TODO: Complex math, setup scene in debugger.
            if (getTicksSince(since) >= v9) {
                blitBufferToBuffer(backgroundData + start, ENDGAME_ENDING_WINDOW_WIDTH, ENDGAME_ENDING_WINDOW_HEIGHT, width, gEndgameEndingSlideshowWindowBuffer, ENDGAME_ENDING_WINDOW_WIDTH);

                if (subtitlesLoaded) {
                    endgameEndingRefreshSubtitles();
                }

                windowRefresh(gEndgameEndingSlideshowWindow);

                since = _get_time();

                bool v14;
                double v31;
                if (start > v32) {
                    if (v12 > start) {
                        v14 = false;
                    } else {
                        int v28 = v32 - (start - v12);
                        v31 = (double)v28 / (double)v32;
                        v14 = true;
                    }
                } else {
                    v14 = true;
                    v31 = (double)start / (double)v32;
                }

                if (v14) {
                    unsigned char darkenedPalette[768];
                    for (int index = 0; index < 768; index++) {
                        darkenedPalette[index] = (unsigned char)trunc(palette[index] * v31);
                    }
                    paletteSetEntries(darkenedPalette);
                }

                start += direction;

                if (direction == 1 && (start == v32)) {
                    // NOTE: Uninline.
                    endgameEndingVoiceOverReset();
                    subtitlesLoaded = true;
                } else if (direction == -1 && (start == v12)) {
                    // NOTE: Uninline.
                    endgameEndingVoiceOverReset();
                    subtitlesLoaded = true;
                }
            }

            soundContinueAll();

            if (_get_input() != -1) {
                // NOTE: Uninline.
                endgameEndingVoiceOverFree();
                break;
            }
        }

        tickersEnable();
        artUnlock(backgroundHandle);

        paletteFadeTo(gPaletteBlack);
        bufferFill(gEndgameEndingSlideshowWindowBuffer, ENDGAME_ENDING_WINDOW_WIDTH, ENDGAME_ENDING_WINDOW_HEIGHT, ENDGAME_ENDING_WINDOW_WIDTH, _colorTable[0]);
        windowRefresh(gEndgameEndingSlideshowWindow);
    }

    while (mouseGetEvent() != 0) {
        _get_input();
    }
}

// 0x440004
void endgameEndingRenderStaticScene(int fid, const char* narratorFileName)
{
    CacheEntry* backgroundHandle;
    Art* background = artLock(fid, &backgroundHandle);
    if (background == NULL) {
        return;
    }

    unsigned char* backgroundData = artGetFrameData(background, 0, 0);
    if (backgroundData != NULL) {
        blitBufferToBuffer(backgroundData, ENDGAME_ENDING_WINDOW_WIDTH, ENDGAME_ENDING_WINDOW_HEIGHT, ENDGAME_ENDING_WINDOW_WIDTH, gEndgameEndingSlideshowWindowBuffer, ENDGAME_ENDING_WINDOW_WIDTH);
        windowRefresh(gEndgameEndingSlideshowWindow);

        endgameEndingLoadPalette((fid & 0xF000000) >> 24, fid & 0xFFF);

        endgameEndingVoiceOverInit(narratorFileName);

        unsigned int delay;
        if (gEndgameEndingVoiceOverSubtitlesLoaded || gEndgameEndingVoiceOverSpeechLoaded) {
            delay = UINT_MAX;
        } else {
            delay = 3000;
        }

        paletteFadeTo(_cmap);

        coreDelayProcessingEvents(500);

        // NOTE: Uninline.
        endgameEndingVoiceOverReset();

        unsigned int referenceTime = _get_time();
        tickersDisable();

        int keyCode;
        while (true) {
            keyCode = _get_input();
            if (keyCode != -1) {
                break;
            }

            if (gEndgameEndingSpeechEnded) {
                break;
            }

            if (gEndgameEndingSubtitlesEnded) {
                break;
            }

            if (getTicksSince(referenceTime) > delay) {
                break;
            }

            blitBufferToBuffer(backgroundData, ENDGAME_ENDING_WINDOW_WIDTH, ENDGAME_ENDING_WINDOW_HEIGHT, ENDGAME_ENDING_WINDOW_WIDTH, gEndgameEndingSlideshowWindowBuffer, ENDGAME_ENDING_WINDOW_WIDTH);
            endgameEndingRefreshSubtitles();
            windowRefresh(gEndgameEndingSlideshowWindow);
            soundContinueAll();
        }

        tickersEnable();
        speechDelete();
        endgameEndingSubtitlesFree();

        gEndgameEndingVoiceOverSpeechLoaded = false;
        gEndgameEndingVoiceOverSubtitlesLoaded = false;

        if (keyCode == -1) {
            coreDelayProcessingEvents(500);
        }

        paletteFadeTo(gPaletteBlack);

        while (mouseGetEvent() != 0) {
            _get_input();
        }
    }

    artUnlock(backgroundHandle);
}

// 0x43F99C
int endgameEndingSlideshowWindowInit()
{
    if (endgameEndingInit() != 0) {
        return -1;
    }

    backgroundSoundDelete();

    _endgame_map_enabled = isoDisable();

    colorCycleDisable();
    gameMouseSetCursor(MOUSE_CURSOR_NONE);

    bool oldCursorIsHidden = cursorIsHidden();
    _endgame_mouse_state = oldCursorIsHidden == 0;

    if (oldCursorIsHidden) {
        mouseShowCursor();
    }

    gEndgameEndingSlideshowOldFont = fontGetCurrent();
    fontSetCurrent(101);

    paletteFadeTo(gPaletteBlack);

    int windowEndgameEndingX = (screenGetWidth() - ENDGAME_ENDING_WINDOW_WIDTH) / 2;
    int windowEndgameEndingY = (screenGetHeight() - ENDGAME_ENDING_WINDOW_HEIGHT) / 2;
    gEndgameEndingSlideshowWindow = windowCreate(windowEndgameEndingX, windowEndgameEndingY, ENDGAME_ENDING_WINDOW_WIDTH, ENDGAME_ENDING_WINDOW_HEIGHT, _colorTable[0], 4);
    if (gEndgameEndingSlideshowWindow == -1) {
        return -1;
    }

    gEndgameEndingSlideshowWindowBuffer = windowGetBuffer(gEndgameEndingSlideshowWindow);
    if (gEndgameEndingSlideshowWindowBuffer == NULL) {
        return -1;
    }

    colorCycleDisable();

    speechSetEndCallback(_endgame_voiceover_callback);

    gEndgameEndingSubtitlesEnabled = false;
    configGetBool(&gGameConfig, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_SUBTITLES_KEY, &gEndgameEndingSubtitlesEnabled);
    if (!gEndgameEndingSubtitlesEnabled) {
        return 0;
    }

    char* language;
    if (!configGetString(&gGameConfig, GAME_CONFIG_SYSTEM_KEY, GAME_CONFIG_LANGUAGE_KEY, &language)) {
        gEndgameEndingSubtitlesEnabled = false;
        return 0;
    }

    sprintf(gEndgameEndingSubtitlesLocalizedPath, "text\\%s\\cuts\\", language);

    gEndgameEndingSubtitles = (char**)internal_malloc(sizeof(*gEndgameEndingSubtitles) * ENDGAME_ENDING_MAX_SUBTITLES);
    if (gEndgameEndingSubtitles == NULL) {
        gEndgameEndingSubtitlesEnabled = false;
        return 0;
    }

    for (int index = 0; index < ENDGAME_ENDING_MAX_SUBTITLES; index++) {
        gEndgameEndingSubtitles[index] = NULL;
    }

    gEndgameEndingSubtitlesTimings = (unsigned int*)internal_malloc(sizeof(*gEndgameEndingSubtitlesTimings) * ENDGAME_ENDING_MAX_SUBTITLES);
    if (gEndgameEndingSubtitlesTimings == NULL) {
        internal_free(gEndgameEndingSubtitles);
        gEndgameEndingSubtitlesEnabled = false;
        return 0;
    }

    return 0;
}

// 0x43FB28
void endgameEndingSlideshowWindowFree()
{
    if (gEndgameEndingSubtitlesEnabled) {
        endgameEndingSubtitlesFree();

        internal_free(gEndgameEndingSubtitlesTimings);
        internal_free(gEndgameEndingSubtitles);

        gEndgameEndingSubtitles = NULL;
        gEndgameEndingSubtitlesEnabled = false;
    }

    // NOTE: Uninline.
    endgameEndingFree();

    fontSetCurrent(gEndgameEndingSlideshowOldFont);

    speechSetEndCallback(NULL);
    windowDestroy(gEndgameEndingSlideshowWindow);

    if (!_endgame_mouse_state) {
        mouseHideCursor();
    }

    gameMouseSetCursor(MOUSE_CURSOR_ARROW);

    colorPaletteLoad("color.pal");
    paletteFadeTo(_cmap);

    colorCycleEnable();

    if (_endgame_map_enabled) {
        isoEnable();
    }
}

// 0x4401A0
void endgameEndingVoiceOverInit(const char* fileBaseName)
{
    char path[COMPAT_MAX_PATH];

    // NOTE: Uninline.
    endgameEndingVoiceOverFree();

    gEndgameEndingVoiceOverSpeechLoaded = false;
    gEndgameEndingVoiceOverSubtitlesLoaded = false;

    // Build speech file path.
    sprintf(path, "%s%s", "narrator\\", fileBaseName);

    if (speechLoad(path, 10, 14, 15) != -1) {
        gEndgameEndingVoiceOverSpeechLoaded = true;
    }

    if (gEndgameEndingSubtitlesEnabled) {
        // Build subtitles file path.
        sprintf(path, "%s%s.txt", gEndgameEndingSubtitlesLocalizedPath, fileBaseName);

        if (endgameEndingSubtitlesLoad(path) != 0) {
            return;
        }

        double durationPerCharacter;
        if (gEndgameEndingVoiceOverSpeechLoaded) {
            durationPerCharacter = (double)speechGetDuration() / (double)gEndgameEndingSubtitlesCharactersCount;
        } else {
            durationPerCharacter = 0.08;
        }

        unsigned int timing = 0;
        for (int index = 0; index < gEndgameEndingSubtitlesLength; index++) {
            double charactersCount = strlen(gEndgameEndingSubtitles[index]);
            // NOTE: There is floating point math at 0x4402E6 used to add
            // timing.
            timing += (unsigned int)trunc(charactersCount * durationPerCharacter * 1000.0);
            gEndgameEndingSubtitlesTimings[index] = timing;
        }

        gEndgameEndingVoiceOverSubtitlesLoaded = true;
    }
}

// NOTE: This function was inlined at every call site.
//
// 0x440324
void endgameEndingVoiceOverReset()
{
    gEndgameEndingSubtitlesEnded = false;
    gEndgameEndingSpeechEnded = false;

    if (gEndgameEndingVoiceOverSpeechLoaded) {
        _gsound_speech_play_preloaded();
    }

    if (gEndgameEndingVoiceOverSubtitlesLoaded) {
        gEndgameEndingSubtitlesReferenceTime = _get_time();
    }
}

// NOTE: This function was inlined at every call site.
//
// 0x44035C
void endgameEndingVoiceOverFree()
{
    speechDelete();
    endgameEndingSubtitlesFree();
    gEndgameEndingVoiceOverSpeechLoaded = false;
    gEndgameEndingVoiceOverSubtitlesLoaded = false;
}

// 0x440378
void endgameEndingLoadPalette(int type, int id)
{
    char fileName[13];
    if (artCopyFileName(type, id, fileName) != 0) {
        return;
    }

    // Remove extension from file name.
    char* pch = strrchr(fileName, '.');
    if (pch != NULL) {
        *pch = '\0';
    }

    if (strlen(fileName) <= 8) {
        char path[COMPAT_MAX_PATH];
        sprintf(path, "%s\\%s.pal", "art\\intrface", fileName);
        colorPaletteLoad(path);
    }
}

// 0x4403F0
void _endgame_voiceover_callback()
{
    gEndgameEndingSpeechEnded = true;
}

// Loads subtitles file.
//
// 0x4403FC
int endgameEndingSubtitlesLoad(const char* filePath)
{
    endgameEndingSubtitlesFree();

    File* stream = fileOpen(filePath, "rt");
    if (stream == NULL) {
        return -1;
    }

    // FIXME: There is at least one subtitle for Arroyo ending (nar_ar1) that
    // does not fit into this buffer.
    char string[256];
    while (fileReadString(string, sizeof(string), stream)) {
        char* pch;

        // Find and clamp string at EOL.
        pch = string;
        while (*pch != '\0' && *pch != '\n') {
            pch++;
        }

        if (*pch != '\0') {
            *pch = '\0';
        }

        // Find separator. The value before separator is ignored (as opposed to
        // movie subtitles, where the value before separator is a timing).
        pch = string;
        while (*pch != '\0' && *pch != ':') {
            pch++;
        }

        if (*pch != '\0') {
            if (gEndgameEndingSubtitlesLength < ENDGAME_ENDING_MAX_SUBTITLES) {
                gEndgameEndingSubtitles[gEndgameEndingSubtitlesLength] = internal_strdup(pch + 1);
                gEndgameEndingSubtitlesLength++;
                gEndgameEndingSubtitlesCharactersCount += strlen(pch + 1);
            }
        }
    }

    fileClose(stream);

    return 0;
}

// Refreshes subtitles.
//
// 0x4404EC
void endgameEndingRefreshSubtitles()
{
    if (gEndgameEndingSubtitlesLength <= gEndgameEndingSubtitlesCurrentLine) {
        if (gEndgameEndingVoiceOverSubtitlesLoaded) {
            gEndgameEndingSubtitlesEnded = true;
        }
        return;
    }

    if (getTicksSince(gEndgameEndingSubtitlesReferenceTime) > gEndgameEndingSubtitlesTimings[gEndgameEndingSubtitlesCurrentLine]) {
        gEndgameEndingSubtitlesCurrentLine++;
        return;
    }

    char* text = gEndgameEndingSubtitles[gEndgameEndingSubtitlesCurrentLine];
    if (text == NULL) {
        return;
    }

    short beginnings[WORD_WRAP_MAX_COUNT];
    short count;
    if (wordWrap(text, 540, beginnings, &count) != 0) {
        return;
    }

    int height = fontGetLineHeight();
    int y = 480 - height * count;

    for (int index = 0; index < count - 1; index++) {
        char* beginning = text + beginnings[index];
        char* ending = text + beginnings[index + 1];

        if (ending[-1] == ' ') {
            ending--;
        }

        char c = *ending;
        *ending = '\0';

        int width = fontGetStringWidth(beginning);
        int x = (640 - width) / 2;
        bufferFill(gEndgameEndingSlideshowWindowBuffer + 640 * y + x, width, height, 640, _colorTable[0]);
        fontDrawText(gEndgameEndingSlideshowWindowBuffer + 640 * y + x, beginning, width, 640, _colorTable[32767]);

        *ending = c;

        y += height;
    }
}

// 0x4406CC
void endgameEndingSubtitlesFree()
{
    for (int index = 0; index < gEndgameEndingSubtitlesLength; index++) {
        if (gEndgameEndingSubtitles[index] != NULL) {
            internal_free(gEndgameEndingSubtitles[index]);
            gEndgameEndingSubtitles[index] = NULL;
        }
    }

    gEndgameEndingSubtitlesCurrentLine = 0;
    gEndgameEndingSubtitlesCharactersCount = 0;
    gEndgameEndingSubtitlesLength = 0;
}

// 0x440728
void _endgame_movie_callback()
{
    _endgame_maybe_done = 1;
}

// 0x440734
void _endgame_movie_bk_process()
{
    if (_endgame_maybe_done) {
        backgroundSoundLoad("10labone", 11, 14, 16);
        backgroundSoundSetEndCallback(NULL);
        tickersRemove(_endgame_movie_bk_process);
    }
}

// 0x440770
int endgameEndingInit()
{
    File* stream;
    char str[256];
    char *ch, *tok;
    const char* delim = " \t,";
    EndgameEnding entry;
    EndgameEnding* entries;
    int narrator_file_len;

    if (gEndgameEndings != NULL) {
        internal_free(gEndgameEndings);
        gEndgameEndings = NULL;
    }

    gEndgameEndingsLength = 0;

    stream = fileOpen("data\\endgame.txt", "rt");
    if (stream == NULL) {
        return -1;
    }

    while (fileReadString(str, sizeof(str), stream)) {
        ch = str;
        while (isspace(*ch)) {
            ch++;
        }

        if (*ch == '#') {
            continue;
        }

        tok = strtok(ch, delim);
        if (tok == NULL) {
            continue;
        }

        entry.gvar = atoi(tok);

        tok = strtok(NULL, delim);
        if (tok == NULL) {
            continue;
        }

        entry.value = atoi(tok);

        tok = strtok(NULL, delim);
        if (tok == NULL) {
            continue;
        }

        entry.art_num = atoi(tok);

        tok = strtok(NULL, delim);
        if (tok == NULL) {
            continue;
        }

        strcpy(entry.voiceOverBaseName, tok);

        narrator_file_len = strlen(entry.voiceOverBaseName);
        if (isspace(entry.voiceOverBaseName[narrator_file_len - 1])) {
            entry.voiceOverBaseName[narrator_file_len - 1] = '\0';
        }

        tok = strtok(NULL, delim);
        if (tok != NULL) {
            entry.direction = atoi(tok);
        } else {
            entry.direction = 1;
        }

        entries = (EndgameEnding*)internal_realloc(gEndgameEndings, sizeof(*entries) * (gEndgameEndingsLength + 1));
        if (entries == NULL) {
            goto err;
        }

        memcpy(&(entries[gEndgameEndingsLength]), &entry, sizeof(entry));

        gEndgameEndings = entries;
        gEndgameEndingsLength++;
    }

    fileClose(stream);

    return 0;

err:

    fileClose(stream);

    return -1;
}

// NOTE: There are no references to this function. It was inlined.
//
// 0x44095C
void endgameEndingFree()
{
    if (gEndgameEndings != NULL) {
        internal_free(gEndgameEndings);
        gEndgameEndings = NULL;
    }

    gEndgameEndingsLength = 0;
}

// endgameDeathEndingInit
// 0x440984
int endgameDeathEndingInit()
{
    File* stream;
    char str[256];
    char* ch;
    const char* delim = " \t,";
    char* tok;
    EndgameDeathEnding entry;
    EndgameDeathEnding* entries;
    int narrator_file_len;

    strcpy(gEndgameDeathEndingFileName, "narrator\\nar_5");

    stream = fileOpen("data\\enddeath.txt", "rt");
    if (stream == NULL) {
        return -1;
    }

    while (fileReadString(str, 256, stream)) {
        ch = str;
        while (isspace(*ch)) {
            ch++;
        }

        if (*ch == '#') {
            continue;
        }

        tok = strtok(ch, delim);
        if (tok == NULL) {
            continue;
        }

        entry.gvar = atoi(tok);

        tok = strtok(NULL, delim);
        if (tok == NULL) {
            continue;
        }

        entry.value = atoi(tok);

        tok = strtok(NULL, delim);
        if (tok == NULL) {
            continue;
        }

        entry.worldAreaKnown = atoi(tok);

        tok = strtok(NULL, delim);
        if (tok == NULL) {
            continue;
        }

        entry.worldAreaNotKnown = atoi(tok);

        tok = strtok(NULL, delim);
        if (tok == NULL) {
            continue;
        }

        entry.min_level = atoi(tok);

        tok = strtok(NULL, delim);
        if (tok == NULL) {
            continue;
        }

        entry.percentage = atoi(tok);

        tok = strtok(NULL, delim);
        if (tok == NULL) {
            continue;
        }

        // this code is slightly different from the original, but does the same thing
        narrator_file_len = strlen(tok);
        strncpy(entry.voiceOverBaseName, tok, narrator_file_len);

        entry.enabled = false;

        if (isspace(entry.voiceOverBaseName[narrator_file_len - 1])) {
            entry.voiceOverBaseName[narrator_file_len - 1] = '\0';
        }

        entries = (EndgameDeathEnding*)internal_realloc(gEndgameDeathEndings, sizeof(*entries) * (gEndgameDeathEndingsLength + 1));
        if (entries == NULL) {
            goto err;
        }

        memcpy(&(entries[gEndgameDeathEndingsLength]), &entry, sizeof(entry));

        gEndgameDeathEndings = entries;
        gEndgameDeathEndingsLength++;
    }

    fileClose(stream);

    return 0;

err:

    fileClose(stream);

    return -1;
}

// 0x440BA8
int endgameDeathEndingExit()
{
    if (gEndgameDeathEndings != NULL) {
        internal_free(gEndgameDeathEndings);
        gEndgameDeathEndings = NULL;

        gEndgameDeathEndingsLength = 0;
    }

    return 0;
}

// endgameSetupDeathEnding
// 0x440BD0
void endgameSetupDeathEnding(int reason)
{
    if (!gEndgameDeathEndingsLength) {
        debugPrint("\nError: endgameSetupDeathEnding: No endgame death info!");
        return;
    }

    // Build death ending file path.
    strcpy(gEndgameDeathEndingFileName, "narrator\\");

    int percentage = 0;
    endgameDeathEndingValidate(&percentage);

    int selectedEnding = 0;
    bool specialEndingSelected = false;

    switch (reason) {
    case ENDGAME_DEATH_ENDING_REASON_DEATH:
        if (gameGetGlobalVar(GVAR_MODOC_SHITTY_DEATH) != 0) {
            selectedEnding = 12;
            specialEndingSelected = true;
        }
        break;
    case ENDGAME_DEATH_ENDING_REASON_TIMEOUT:
        gameMoviePlay(MOVIE_TIMEOUT, GAME_MOVIE_FADE_IN | GAME_MOVIE_FADE_OUT | GAME_MOVIE_PAUSE_MUSIC);
        break;
    }

    if (!specialEndingSelected) {
        int chance = randomBetween(0, percentage);

        int accum = 0;
        for (int index = 0; index < gEndgameDeathEndingsLength; index++) {
            EndgameDeathEnding* deathEnding = &(gEndgameDeathEndings[index]);

            if (deathEnding->enabled) {
                accum += deathEnding->percentage;
                if (accum >= chance) {
                    break;
                }
                selectedEnding++;
            }
        }
    }

    EndgameDeathEnding* deathEnding = &(gEndgameDeathEndings[selectedEnding]);

    strcat(gEndgameDeathEndingFileName, deathEnding->voiceOverBaseName);

    debugPrint("\nendgameSetupDeathEnding: Death Filename Picked: %s", gEndgameDeathEndingFileName);
}

// Validates conditions imposed by death endings.
//
// Upon return [percentage] is set as a sum of all valid endings' percentages.
// Always returns 0.
//
// 0x440CF4
int endgameDeathEndingValidate(int* percentage)
{
    *percentage = 0;

    for (int index = 0; index < gEndgameDeathEndingsLength; index++) {
        EndgameDeathEnding* deathEnding = &(gEndgameDeathEndings[index]);

        deathEnding->enabled = false;

        if (deathEnding->gvar != -1) {
            if (gameGetGlobalVar(deathEnding->gvar) >= deathEnding->value) {
                continue;
            }
        }

        if (deathEnding->worldAreaKnown != -1) {
            if (!_wmAreaIsKnown(deathEnding->worldAreaKnown)) {
                continue;
            }
        }

        if (deathEnding->worldAreaNotKnown != -1) {
            if (_wmAreaIsKnown(deathEnding->worldAreaNotKnown)) {
                continue;
            }
        }

        if (pcGetStat(PC_STAT_LEVEL) < deathEnding->min_level) {
            continue;
        }

        deathEnding->enabled = true;

        *percentage += deathEnding->percentage;
    }

    return 0;
}

// Returns file name for voice over for death ending.
//
// This path does not include extension.
//
// 0x440D8C
char* endgameDeathEndingGetFileName()
{
    if (gEndgameDeathEndingsLength == 0) {
        debugPrint("\nError: endgameSetupDeathEnding: No endgame death info!");
        strcpy(gEndgameDeathEndingFileName, "narrator\\nar_4");
    }

    debugPrint("\nendgameSetupDeathEnding: Death Filename: %s", gEndgameDeathEndingFileName);

    return gEndgameDeathEndingFileName;
}
