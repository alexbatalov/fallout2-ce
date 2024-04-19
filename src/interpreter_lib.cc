#include "interpreter_lib.h"

#include "color.h"
#include "datafile.h"
#include "debug.h"
#include "dialog.h"
#include "input.h"
#include "interpreter_extra.h"
#include "memory_manager.h"
#include "mouse.h"
#include "mouse_manager.h"
#include "nevs.h"
#include "select_file_list.h"
#include "sound.h"
#include "svga.h"
#include "text_font.h"
#include "window.h"
#include "window_manager_private.h"

namespace fallout {

#define INT_LIB_SOUNDS_CAPACITY (32)
#define INT_LIB_KEY_HANDLERS_CAPACITY (256)

typedef struct IntLibKeyHandlerEntry {
    Program* program;
    int proc;
} IntLibKeyHandlerEntry;

static void opFillWin3x3(Program* program);
static void opFormat(Program* program);
static void opPrint(Program* program);
static void opSelectFileList(Program* program);
static void opTokenize(Program* program);
static void opPrintRect(Program* program);
static void opSelect(Program* program);
static void opDisplay(Program* program);
static void opDisplayRaw(Program* program);
static void _interpretFadePaletteBK(unsigned char* oldPalette, unsigned char* newPalette, int a3, float duration, bool shouldProcessBk);
static void interpretFadePalette(unsigned char* oldPalette, unsigned char* newPalette, int a3, float duration);
static int intlibGetFadeIn();
static void interpretFadeOut(float duration);
static void interpretFadeIn(float duration);
static void interpretFadeOutNoBK(float duration);
static void interpretFadeInNoBK(float duration);
static void opFadeIn(Program* program);
static void opFadeOut(Program* program);
static int intLibCheckMovie(Program* program);
static void opSetMovieFlags(Program* program);
static void opPlayMovie(Program* program);
static void opStopMovie(Program* program);
static void opAddRegionProc(Program* program);
static void opAddRegionRightProc(Program* program);
static void opCreateWin(Program* program);
static void opResizeWin(Program* program);
static void opScaleWin(Program* program);
static void opDeleteWin(Program* program);
static void opSayStart(Program* program);
static void opDeleteRegion(Program* program);
static void opActivateRegion(Program* program);
static void opCheckRegion(Program* program);
static void opAddRegion(Program* program);
static void opSayStartPos(Program* program);
static void opSayReplyTitle(Program* program);
static void opSayGoToReply(Program* program);
static void opSayReply(Program* program);
static void opSayOption(Program* program);
static int intLibCheckDialog(Program* program);
static void opSayEnd(Program* program);
static void opSayGetLastPos(Program* program);
static void opSayQuit(Program* program);
static int getTimeOut();
static void setTimeOut(int value);
static void opSayMessageTimeout(Program* program);
static void opSayMessage(Program* program);
static void opGotoXY(Program* program);
static void opAddButtonFlag(Program* program);
static void opAddRegionFlag(Program* program);
static void opAddButton(Program* program);
static void opAddButtonText(Program* program);
static void opAddButtonGfx(Program* program);
static void opAddButtonProc(Program* program);
static void opAddButtonRightProc(Program* program);
static void opShowWin(Program* program);
static void opDeleteButton(Program* program);
static void opFillWin(Program* program);
static void opFillRect(Program* program);
static void opHideMouse(Program* program);
static void opShowMouse(Program* program);
static void opMouseShape(Program* program);
static void opSetGlobalMouseFunc(Program* Program);
static void opDisplayGfx(Program* program);
static void opLoadPaletteTable(Program* program);
static void opAddNamedEvent(Program* program);
static void opAddNamedHandler(Program* program);
static void opClearNamed(Program* program);
static void opSignalNamed(Program* program);
static void opAddKey(Program* program);
static void opDeleteKey(Program* program);
static void opRefreshMouse(Program* program);
static void opSetFont(Program* program);
static void opSetTextFlags(Program* program);
static void opSetTextColor(Program* program);
static void opSayOptionColor(Program* program);
static void opSayReplyColor(Program* program);
static void opSetHighlightColor(Program* program);
static void opSayReplyWindow(Program* program);
static void opSayReplyFlags(Program* program);
static void opSayOptionFlags(Program* program);
static void opSayOptionWindow(Program* program);
static void opSayBorder(Program* program);
static void opSayScrollUp(Program* program);
static void opSayScrollDown(Program* program);
static void opSaySetSpacing(Program* program);
static void opSayRestart(Program* program);
static void intLibSoundCallback(void* userData, int a2);
static int intLibSoundDelete(int value);
static int intLibSoundPlay(char* fileName, int mode);
static int intLibSoundPause(int value);
static int intLibSoundRewind(int value);
static int intLibSoundResume(int value);
static void opSoundPlay(Program* program);
static void opSoundPause(Program* program);
static void opSoundResume(Program* program);
static void opSoundStop(Program* program);
static void opSoundRewind(Program* program);
static void opSoundDelete(Program* program);
static void opSetOneOptPause(Program* program);
static bool intLibDoInput(int key);

// 0x59D5D0
static Sound* gIntLibSounds[INT_LIB_SOUNDS_CAPACITY];

// 0x59D650
static unsigned char gIntLibFadePalette[256 * 3];

// 0x59D950
static IntLibKeyHandlerEntry gIntLibKeyHandlerEntries[INT_LIB_KEY_HANDLERS_CAPACITY];

// 0x59E150
static bool gIntLibIsPaletteFaded;

// 0x59E154
static int gIntLibGenericKeyHandlerProc;

// 0x59E158
static int gIntLibProgramDeleteCallbacksLength;

// 0x59E15C
static Program* gIntLibGenericKeyHandlerProgram;

// 0x59E160
static IntLibProgramDeleteCallback** gIntLibProgramDeleteCallbacks;

// 0x59E164
static int gIntLibSayStartingPosition;

// 0x59E168
static char gIntLibPlayMovieFileName[100];

// 0x59E1CC
static char gIntLibPlayMovieRectFileName[100];

// fillwin3x3
// 0x461780
void opFillWin3x3(Program* program)
{
    char* fileName = programStackPopString(program);
    char* mangledFileName = _interpretMangleName(fileName);

    int imageWidth;
    int imageHeight;
    unsigned char* imageData = datafileRead(mangledFileName, &imageWidth, &imageHeight);
    if (imageData == nullptr) {
        programFatalError("cannot load 3x3 file '%s'", mangledFileName);
    }

    _selectWindowID(program->windowId);

    int windowHeight = _windowHeight();
    int windowWidth = _windowWidth();
    unsigned char* windowBuffer = _windowGetBuffer();
    _fillBuf3x3(imageData,
        imageWidth,
        imageHeight,
        windowBuffer,
        windowWidth,
        windowHeight);

    internal_free_safe(imageData, __FILE__, __LINE__); // "..\\int\\INTLIB.C", 94
}

// format
// 0x461850
static void opFormat(Program* program)
{
    int textAlignment = programStackPopInteger(program);
    int height = programStackPopInteger(program);
    int width = programStackPopInteger(program);
    int y = programStackPopInteger(program);
    int x = programStackPopInteger(program);
    char* string = programStackPopString(program);

    if (!_windowFormatMessage(string, x, y, width, height, textAlignment)) {
        programFatalError("Error formatting message\n");
    }
}

// print
// 0x461A5C
static void opPrint(Program* program)
{
    _selectWindowID(program->windowId);

    ProgramValue value = programStackPopValue(program);
    char string[80];

    // SFALL: Fix broken Print() script function.
    // CE: Original code uses `interpretOutput` to handle printing. However
    // this function looks invalid or broken itself. Check `opSelect` - it sets
    // `outputFunc` to `windowOutput`, but `outputFunc` is never called. I'm not
    // sure if this fix can be moved into `interpretOutput` because it is also
    // used in procedure setup functions.
    //
    // The fix is slightly different, Sfall fixes strings only, ints and floats
    // are still passed to `interpretOutput`.
    switch (value.opcode & VALUE_TYPE_MASK) {
    case VALUE_TYPE_STRING:
        _windowOutput(programGetString(program, value.opcode, value.integerValue));
        break;
    case VALUE_TYPE_FLOAT:
        snprintf(string, sizeof(string), "%.5f", value.floatValue);
        _windowOutput(string);
        break;
    case VALUE_TYPE_INT:
        snprintf(string, sizeof(string), "%d", value.integerValue);
        _windowOutput(string);
        break;
    }
}

// selectfilelist
// 0x461B10
void opSelectFileList(Program* program)
{
    program->flags |= PROGRAM_FLAG_0x20;

    char* pattern = programStackPopString(program);
    char* title = programStackPopString(program);

    int fileListLength;
    char** fileList = _getFileList(_interpretMangleName(pattern), &fileListLength);
    if (fileList != nullptr && fileListLength != 0) {
        int selectedIndex = _win_list_select(title,
            fileList,
            fileListLength,
            nullptr,
            320 - fontGetStringWidth(title) / 2,
            200,
            _colorTable[0x7FFF] | 0x10000);

        if (selectedIndex != -1) {
            programStackPushString(program, fileList[selectedIndex]);
        } else {
            programStackPushInteger(program, 0);
        }

        _freeFileList(fileList);
    } else {
        programStackPushInteger(program, 0);
    }

    program->flags &= ~PROGRAM_FLAG_0x20;
}

// tokenize
// 0x461CA0
void opTokenize(Program* program)
{
    int ch = programStackPopInteger(program);

    ProgramValue prevValue = programStackPopValue(program);

    char* prev = nullptr;
    if ((prevValue.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_INT) {
        if (prevValue.integerValue != 0) {
            programFatalError("Error, invalid arg 2 to tokenize. (only accept 0 for int value)");
        }
    } else if ((prevValue.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
        prev = programGetString(program, prevValue.opcode, prevValue.integerValue);
    } else {
        programFatalError("Error, invalid arg 2 to tokenize. (string)");
    }

    char* string = programStackPopString(program);
    char* temp = nullptr;

    if (prev != nullptr) {
        char* start = strstr(string, prev);
        if (start != nullptr) {
            start += strlen(prev);
            while (*start != ch && *start != '\0') {
                start++;
            }
        }

        if (*start == ch) {
            int length = 0;
            char* end = start + 1;
            while (*end != ch && *end != '\0') {
                end++;
                length++;
            }

            temp = (char*)internal_calloc_safe(1, length + 1, __FILE__, __LINE__); // "..\\int\\INTLIB.C, 230
            strncpy(temp, start, length);
            programStackPushString(program, temp);
        } else {
            programStackPushInteger(program, 0);
        }
    } else {
        int length = 0;
        char* end = string;
        while (*end != ch && *end != '\0') {
            end++;
            length++;
        }

        if (string != nullptr) {
            temp = (char*)internal_calloc_safe(1, length + 1, __FILE__, __LINE__); // "..\\int\\INTLIB.C", 248
            strncpy(temp, string, length);
            programStackPushString(program, temp);
        } else {
            programStackPushInteger(program, 0);
        }
    }

    if (temp != nullptr) {
        internal_free_safe(temp, __FILE__, __LINE__); // "..\\int\\INTLIB.C" , 260
    }
}

// printrect
// 0x461F1C
static void opPrintRect(Program* program)
{
    _selectWindowID(program->windowId);

    int v1 = programStackPopInteger(program);
    if (v1 > 2) {
        programFatalError("Invalid arg 3 given to printrect, expecting int");
    }

    int v2 = programStackPopInteger(program);

    ProgramValue value = programStackPopValue(program);
    char string[80];
    switch (value.opcode & VALUE_TYPE_MASK) {
    case VALUE_TYPE_STRING:
        snprintf(string, sizeof(string), "%s", programGetString(program, value.opcode, value.integerValue));
        break;
    case VALUE_TYPE_FLOAT:
        snprintf(string, sizeof(string), "%.5f", value.floatValue);
        break;
    case VALUE_TYPE_INT:
        snprintf(string, sizeof(string), "%d", value.integerValue);
        break;
    }

    if (!_windowPrintRect(string, v2, v1)) {
        programFatalError("Error in printrect");
    }
}

// 0x46209C
void opSelect(Program* program)
{
    const char* windowName = programStackPopString(program);
    int win = _pushWindow(windowName);
    if (win == -1) {
        programFatalError("Error selecing window %s\n", windowName);
    }

    program->windowId = win;

    _interpretOutputFunc(_windowOutput);
}

// display
// 0x46213C
void opDisplay(Program* program)
{
    char* fileName = programStackPopString(program);

    _selectWindowID(program->windowId);

    char* mangledFileName = _interpretMangleName(fileName);
    _displayFile(mangledFileName);
}

// displayraw
// 0x4621B4
void opDisplayRaw(Program* program)
{
    char* fileName = programStackPopString(program);

    _selectWindowID(program->windowId);

    char* mangledFileName = _interpretMangleName(fileName);
    _displayFileRaw(mangledFileName);
}

// 0x46222C
static void _interpretFadePaletteBK(unsigned char* oldPalette, unsigned char* newPalette, int a3, float duration, int shouldProcessBk)
{
    unsigned int time;
    unsigned int previousTime;
    unsigned int delta;
    int step;
    int steps;
    int index;
    unsigned char palette[256 * 3];

    time = getTicks();
    previousTime = time;
    steps = (int)duration;
    step = 0;
    delta = 0;

    // TODO: Check if it needs throttling.
    if (duration != 0.0) {
        while (step < steps) {
            if (delta != 0) {
                for (index = 0; index < 768; index++) {
                    palette[index] = oldPalette[index] - (oldPalette[index] - newPalette[index]) * step / steps;
                }

                _setSystemPalette(palette);
                renderPresent();

                previousTime = time;
                step += delta;
            }

            if (shouldProcessBk) {
                _process_bk();
            }

            time = getTicks();
            delta = time - previousTime;
        }
    }

    _setSystemPalette(newPalette);
    renderPresent();
}

// NOTE: Unused.
//
// 0x462330
static void interpretFadePalette(unsigned char* oldPalette, unsigned char* newPalette, int a3, float duration)
{
    _interpretFadePaletteBK(oldPalette, newPalette, a3, duration, 1);
}

// NOTE: Unused.
static int intlibGetFadeIn()
{
    return gIntLibIsPaletteFaded;
}

// NOTE: Inlined.
//
// 0x462348
static void interpretFadeOut(float duration)
{
    int cursorWasHidden;

    cursorWasHidden = cursorIsHidden();
    mouseHideCursor();

    _interpretFadePaletteBK(_getSystemPalette(), gIntLibFadePalette, 64, duration, 1);

    if (!cursorWasHidden) {
        mouseShowCursor();
    }
}

// NOTE: Inlined.
//
// 0x462380
static void interpretFadeIn(float duration)
{
    _interpretFadePaletteBK(gIntLibFadePalette, _cmap, 64, duration, 1);
}

// NOTE: Unused.
//
// 0x4623A4
static void interpretFadeOutNoBK(float duration)
{
    int cursorWasHidden;

    cursorWasHidden = cursorIsHidden();
    mouseHideCursor();

    _interpretFadePaletteBK(_getSystemPalette(), gIntLibFadePalette, 64, duration, 0);

    if (!cursorWasHidden) {
        mouseShowCursor();
    }
}

// NOTE: Unused.
//
// 0x4623DC
static void interpretFadeInNoBK(float duration)
{
    _interpretFadePaletteBK(gIntLibFadePalette, _cmap, 64, duration, 0);
}

// fadein
// 0x462400
void opFadeIn(Program* program)
{
    int data = programStackPopInteger(program);

    program->flags |= PROGRAM_FLAG_0x20;

    _setSystemPalette(gIntLibFadePalette);

    // NOTE: Uninline.
    interpretFadeIn((float)data);

    gIntLibIsPaletteFaded = true;

    program->flags &= ~PROGRAM_FLAG_0x20;
}

// fadeout
// 0x4624B4
void opFadeOut(Program* program)
{
    int data = programStackPopInteger(program);

    program->flags |= PROGRAM_FLAG_0x20;

    bool cursorWasHidden = cursorIsHidden();
    mouseHideCursor();

    // NOTE: Uninline.
    interpretFadeOut((float)data);

    if (!cursorWasHidden) {
        mouseShowCursor();
    }

    gIntLibIsPaletteFaded = false;

    program->flags &= ~PROGRAM_FLAG_0x20;
}

// 0x462570
int intLibCheckMovie(Program* program)
{
    if (_dialogGetDialogDepth() > 0) {
        return 1;
    }

    return _windowMoviePlaying();
}

// movieflags
// 0x462584
static void opSetMovieFlags(Program* program)
{
    int data = programStackPopInteger(program);

    if (!_windowSetMovieFlags(data)) {
        programFatalError("Error setting movie flags\n");
    }
}

// playmovie
// 0x4625D0
void opPlayMovie(Program* program)
{
    char* movieFileName = programStackPopString(program);

    strcpy(gIntLibPlayMovieFileName, movieFileName);

    if (strrchr(gIntLibPlayMovieFileName, '.') == nullptr) {
        strcat(gIntLibPlayMovieFileName, ".mve");
    }

    _selectWindowID(program->windowId);

    program->flags |= PROGRAM_IS_WAITING;
    program->checkWaitFunc = intLibCheckMovie;

    char* mangledFileName = _interpretMangleName(gIntLibPlayMovieFileName);
    if (!_windowPlayMovie(mangledFileName)) {
        programFatalError("Error playing movie");
    }
}

// playmovierect
// 0x4626C4
void opPlayMovieRect(Program* program)
{
    int height = programStackPopInteger(program);
    int width = programStackPopInteger(program);
    int y = programStackPopInteger(program);
    int x = programStackPopInteger(program);
    char* movieFileName = programStackPopString(program);

    strcpy(gIntLibPlayMovieRectFileName, movieFileName);

    if (strrchr(gIntLibPlayMovieRectFileName, '.') == nullptr) {
        strcat(gIntLibPlayMovieRectFileName, ".mve");
    }

    _selectWindowID(program->windowId);

    program->checkWaitFunc = intLibCheckMovie;
    program->flags |= PROGRAM_IS_WAITING;

    char* mangledFileName = _interpretMangleName(gIntLibPlayMovieRectFileName);
    if (!_windowPlayMovieRect(mangledFileName, x, y, width, height)) {
        programFatalError("Error playing movie");
    }
}

// stopmovie
// 0x46287C
static void opStopMovie(Program* program)
{
    _windowStopMovie();
    program->flags |= PROGRAM_FLAG_0x40;
}

// deleteregion
// 0x462890
static void opDeleteRegion(Program* program)
{
    ProgramValue value = programStackPopValue(program);

    switch (value.opcode & 0xF7FF) {
    case VALUE_TYPE_STRING:
        break;
    case VALUE_TYPE_INT:
        if (value.integerValue == -1) {
            break;
        }
        // FALLTHROUGH
    default:
        programFatalError("Invalid type given to deleteregion");
    }

    _selectWindowID(program->windowId);

    const char* regionName = value.integerValue != -1 ? programGetString(program, value.opcode, value.integerValue) : nullptr;
    _windowDeleteRegion(regionName);
}

// activateregion
// 0x462924
void opActivateRegion(Program* program)
{
    int v1 = programStackPopInteger(program);
    char* regionName = programStackPopString(program);

    _windowActivateRegion(regionName, v1);
}

// checkregion
// 0x4629A0
static void opCheckRegion(Program* program)
{
    const char* regionName = programStackPopString(program);

    bool regionExists = _windowCheckRegionExists(regionName);
    programStackPushInteger(program, regionExists);
}

// addregion
// 0x462A1C
void opAddRegion(Program* program)
{
    int args = programStackPopInteger(program);

    if (args < 2) {
        programFatalError("addregion call without enough points!");
    }

    _selectWindowID(program->windowId);

    _windowStartRegion(args / 2);

    while (args >= 2) {
        int y = programStackPopInteger(program);
        int x = programStackPopInteger(program);

        y = (y * _windowGetYres() + 479) / 480;
        x = (x * _windowGetXres() + 639) / 640;
        args -= 2;

        _windowAddRegionPoint(x, y, true);
    }

    if (args == 0) {
        programFatalError("Unnamed regions not allowed\n");
        _windowEndRegion();
    } else {
        const char* regionName = programStackPopString(program);
        _windowAddRegionName(regionName);
        _windowEndRegion();
    }
}

// addregionproc
// 0x462C10
static void opAddRegionProc(Program* program)
{
    int v1 = programStackPopInteger(program);
    int v2 = programStackPopInteger(program);
    int v3 = programStackPopInteger(program);
    int v4 = programStackPopInteger(program);
    const char* regionName = programStackPopString(program);

    _selectWindowID(program->windowId);

    if (!_windowAddRegionProc(regionName, program, v4, v3, v2, v1)) {
        programFatalError("Error setting procedures to region %s\n", regionName);
    }
}

// addregionrightproc
// 0x462DDC
static void opAddRegionRightProc(Program* program)
{
    int v1 = programStackPopInteger(program);
    int v2 = programStackPopInteger(program);
    const char* regionName = programStackPopString(program);
    _selectWindowID(program->windowId);

    if (!_windowAddRegionRightProc(regionName, program, v2, v1)) {
        programFatalError("ErrorError setting right button procedures to region %s\n", regionName);
    }
}

// createwin
// 0x462F08
void opCreateWin(Program* program)
{
    int height = programStackPopInteger(program);
    int width = programStackPopInteger(program);
    int y = programStackPopInteger(program);
    int x = programStackPopInteger(program);
    char* windowName = programStackPopString(program);

    x = (x * _windowGetXres() + 639) / 640;
    y = (y * _windowGetYres() + 479) / 480;
    width = (width * _windowGetXres() + 639) / 640;
    height = (height * _windowGetYres() + 479) / 480;

    if (_createWindow(windowName, x, y, width, height, _colorTable[0], 0) == -1) {
        programFatalError("Couldn't create window.");
    }
}

// resizewin
// 0x46308C
void opResizeWin(Program* program)
{
    int height = programStackPopInteger(program);
    int width = programStackPopInteger(program);
    int y = programStackPopInteger(program);
    int x = programStackPopInteger(program);
    char* windowName = programStackPopString(program);

    x = (x * _windowGetXres() + 639) / 640;
    y = (y * _windowGetYres() + 479) / 480;
    width = (width * _windowGetXres() + 639) / 640;
    height = (height * _windowGetYres() + 479) / 480;

    if (sub_4B7AC4(windowName, x, y, width, height) == -1) {
        programFatalError("Couldn't resize window.");
    }
}

// scalewin
// 0x463204
void opScaleWin(Program* program)
{
    int height = programStackPopInteger(program);
    int width = programStackPopInteger(program);
    int y = programStackPopInteger(program);
    int x = programStackPopInteger(program);
    char* windowName = programStackPopString(program);

    x = (x * _windowGetXres() + 639) / 640;
    y = (y * _windowGetYres() + 479) / 480;
    width = (width * _windowGetXres() + 639) / 640;
    height = (height * _windowGetYres() + 479) / 480;

    if (sub_4B7E7C(windowName, x, y, width, height) == -1) {
        programFatalError("Couldn't scale window.");
    }
}

// deletewin
// 0x46337C
void opDeleteWin(Program* program)
{
    char* windowName = programStackPopString(program);

    if (!_deleteWindow(windowName)) {
        programFatalError("Error deleting window %s\n", windowName);
    }

    program->windowId = _popWindow();
}

// saystart
// 0x4633E4
static void opSayStart(Program* program)
{
    gIntLibSayStartingPosition = 0;

    program->flags |= PROGRAM_FLAG_0x20;
    int rc = _dialogStart(program);
    program->flags &= ~PROGRAM_FLAG_0x20;

    if (rc != 0) {
        programFatalError("Error starting dialog.");
    }
}

// saystartpos
// 0x463430
static void opSayStartPos(Program* program)
{
    gIntLibSayStartingPosition = programStackPopInteger(program);

    program->flags |= PROGRAM_FLAG_0x20;
    int rc = _dialogStart(program);
    program->flags &= ~PROGRAM_FLAG_0x20;

    if (rc != 0) {
        programFatalError("Error starting dialog.");
    }
}

// sayreplytitle
// 0x46349C
static void opSayReplyTitle(Program* program)
{
    ProgramValue value = programStackPopValue(program);

    char* string = nullptr;
    if ((value.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
        string = programGetString(program, value.opcode, value.integerValue);
    }

    if (dialogSetReplyTitle(string) != 0) {
        programFatalError("Error setting title.");
    }
}

// saygotoreply
// 0x463510
static void opSayGoToReply(Program* program)
{
    ProgramValue value = programStackPopValue(program);

    char* string = nullptr;
    if ((value.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
        string = programGetString(program, value.opcode, value.integerValue);
    }

    if (_dialogGotoReply(string) != 0) {
        programFatalError("Error during goto, couldn't find reply target %s", string);
    }
}

// sayreply
// 0x463584
void opSayReply(Program* program)
{
    program->flags |= PROGRAM_FLAG_0x20;

    ProgramValue v3 = programStackPopValue(program);
    ProgramValue v2 = programStackPopValue(program);

    const char* v1;
    if ((v2.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
        v1 = programGetString(program, v2.opcode, v2.integerValue);
    } else {
        v1 = nullptr;
    }

    if ((v3.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
        const char* v2 = programGetString(program, v3.opcode, v3.integerValue);
        if (_dialogOption(v1, v2) != 0) {
            program->flags &= ~PROGRAM_FLAG_0x20;
            programFatalError("Error setting option.");
        }
    } else if ((v3.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_INT) {
        if (_dialogOptionProc(v1, v3.integerValue) != 0) {
            program->flags &= ~PROGRAM_FLAG_0x20;
            programFatalError("Error setting option.");
        }
    } else {
        programFatalError("Invalid arg 2 to sayOption");
    }

    program->flags &= ~PROGRAM_FLAG_0x20;
}

// sayoption
void opSayOption(Program* program)
{
    program->flags |= PROGRAM_FLAG_0x20;

    ProgramValue v3 = programStackPopValue(program);
    ProgramValue v4 = programStackPopValue(program);

    const char* v1;
    if ((v4.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
        v1 = programGetString(program, v4.opcode, v4.integerValue);
    } else {
        v1 = nullptr;
    }

    const char* v2;
    if ((v3.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
        v2 = programGetString(program, v3.opcode, v3.integerValue);
    } else {
        v2 = nullptr;
    }

    if (_dialogReply(v1, v2) != 0) {
        program->flags &= ~PROGRAM_FLAG_0x20;
        programFatalError("Error setting option.");
    }

    program->flags &= ~PROGRAM_FLAG_0x20;
}

// 0x46378C
int intLibCheckDialog(Program* program)
{
    program->flags |= PROGRAM_FLAG_0x40;
    return _dialogGetDialogDepth() != -1;
}

// 0x4637A4
void opSayEnd(Program* program)
{
    program->flags |= PROGRAM_FLAG_0x20;
    int rc = sub_431088(gIntLibSayStartingPosition);
    program->flags &= ~PROGRAM_FLAG_0x20;

    if (rc == -2) {
        program->checkWaitFunc = intLibCheckDialog;
        program->flags |= PROGRAM_IS_WAITING;
    }
}

// saygetlastpos
// 0x4637EC
static void opSayGetLastPos(Program* program)
{
    int value = _dialogGetExitPoint();
    programStackPushInteger(program, value);
}

// sayquit
// 0x463810
static void opSayQuit(Program* program)
{
    if (_dialogQuit() != 0) {
        programFatalError("Error quitting option.");
    }
}

// NOTE: Unused.
//
// 0x463828
static int getTimeOut()
{
    return _TimeOut;
}

// NOTE: Unused.
//
// 0x463830
static void setTimeOut(int value)
{
    _TimeOut = value;
}

// saymessagetimeout
// 0x463838
static void opSayMessageTimeout(Program* program)
{
    ProgramValue value = programStackPopValue(program);

    // TODO: What the hell is this?
    if ((value.opcode & VALUE_TYPE_MASK) == 0x4000) {
        programFatalError("sayMsgTimeout:  invalid var type passed.");
    }

    _TimeOut = value.integerValue;
}

// saymessage
// 0x463890
void opSayMessage(Program* program)
{
    program->flags |= PROGRAM_FLAG_0x20;

    ProgramValue v3 = programStackPopValue(program);
    ProgramValue v4 = programStackPopValue(program);

    const char* v1;
    if ((v4.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
        v1 = programGetString(program, v4.opcode, v4.integerValue);
    } else {
        v1 = nullptr;
    }

    const char* v2;
    if ((v3.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
        v2 = programGetString(program, v3.opcode, v3.integerValue);
    } else {
        v2 = nullptr;
    }

    if (sub_430FD4(v1, v2, _TimeOut) != 0) {
        program->flags &= ~PROGRAM_FLAG_0x20;
        programFatalError("Error setting option.");
    }

    program->flags &= ~PROGRAM_FLAG_0x20;
}

// gotoxy
// 0x463980
void opGotoXY(Program* program)
{
    int y = programStackPopInteger(program);
    int x = programStackPopInteger(program);

    _selectWindowID(program->windowId);

    _windowGotoXY(x, y);
}

// addbuttonflag
// 0x463A38
static void opAddButtonFlag(Program* program)
{
    int flag = programStackPopInteger(program);
    const char* buttonName = programStackPopString(program);
    if (!_windowSetButtonFlag(buttonName, flag)) {
        // NOTE: Original code calls programGetString one more time with the
        // same params.
        programFatalError("Error setting flag on button %s", buttonName);
    }
}

// addregionflag
// 0x463B10
static void opAddRegionFlag(Program* program)
{
    int flag = programStackPopInteger(program);
    const char* regionName = programStackPopString(program);
    if (!_windowSetRegionFlag(regionName, flag)) {
        // NOTE: Original code calls programGetString one more time with the
        // same params.
        programFatalError("Error setting flag on region %s", regionName);
    }
}

// addbutton
// 0x463BE8
void opAddButton(Program* program)
{
    int height = programStackPopInteger(program);
    int width = programStackPopInteger(program);
    int y = programStackPopInteger(program);
    int x = programStackPopInteger(program);
    char* buttonName = programStackPopString(program);

    _selectWindowID(program->windowId);

    height = (height * _windowGetYres() + 479) / 480;
    width = (width * _windowGetXres() + 639) / 640;
    y = (y * _windowGetYres() + 479) / 480;
    x = (x * _windowGetXres() + 639) / 640;

    _windowAddButton(buttonName, x, y, width, height, 0);
}

// addbuttontext
// 0x463DF4
void opAddButtonText(Program* program)
{
    const char* text = programStackPopString(program);
    const char* buttonName = programStackPopString(program);

    if (!_windowAddButtonText(buttonName, text)) {
        programFatalError("Error setting text to button %s\n", buttonName);
    }
}

// addbuttongfx
// 0x463EEC
void opAddButtonGfx(Program* program)
{
    ProgramValue v1 = programStackPopValue(program);
    ProgramValue v2 = programStackPopValue(program);
    ProgramValue v3 = programStackPopValue(program);
    char* buttonName = programStackPopString(program);

    if (((v3.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING || ((v3.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_INT && v3.integerValue == 0))
        || ((v2.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING || ((v2.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_INT && v2.integerValue == 0))
        || ((v1.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING || ((v1.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_INT && v1.integerValue == 0))) {
        char* pressedFileName = _interpretMangleName(programGetString(program, v3.opcode, v3.integerValue));
        char* normalFileName = _interpretMangleName(programGetString(program, v2.opcode, v2.integerValue));
        char* hoverFileName = _interpretMangleName(programGetString(program, v1.opcode, v1.integerValue));

        _selectWindowID(program->windowId);

        if (!_windowAddButtonGfx(buttonName, pressedFileName, normalFileName, hoverFileName)) {
            programFatalError("Error setting graphics to button %s\n", buttonName);
        }
    } else {
        programFatalError("Invalid filename given to addbuttongfx");
    }
}

// addbuttonproc
// 0x4640DC
static void opAddButtonProc(Program* program)
{
    int v1 = programStackPopInteger(program);
    int v2 = programStackPopInteger(program);
    int v3 = programStackPopInteger(program);
    int v4 = programStackPopInteger(program);
    const char* buttonName = programStackPopString(program);
    _selectWindowID(program->windowId);

    if (!_windowAddButtonProc(buttonName, program, v4, v3, v2, v1)) {
        programFatalError("Error setting procedures to button %s\n", buttonName);
    }
}

// addbuttonrightproc
// 0x4642A8
static void opAddButtonRightProc(Program* program)
{
    int v1 = programStackPopInteger(program);
    int v2 = programStackPopInteger(program);
    const char* regionName = programStackPopString(program);
    _selectWindowID(program->windowId);

    if (!_windowAddRegionRightProc(regionName, program, v2, v1)) {
        programFatalError("Error setting right button procedures to button %s\n", regionName);
    }
}

// showwin
// 0x4643D4
static void opShowWin(Program* program)
{
    _selectWindowID(program->windowId);
    _windowDraw();
}

// deletebutton
// 0x4643E4
static void opDeleteButton(Program* program)
{
    ProgramValue value = programStackPopValue(program);

    switch (value.opcode & VALUE_TYPE_MASK) {
    case VALUE_TYPE_STRING:
        break;
    case VALUE_TYPE_INT:
        if (value.integerValue == -1) {
            break;
        }
        // FALLTHROUGH
    default:
        programFatalError("Invalid type given to delete button");
    }

    _selectWindowID(program->windowId);

    if ((value.opcode & 0xF7FF) == VALUE_TYPE_INT) {
        if (_windowDeleteButton(nullptr)) {
            return;
        }
    } else {
        const char* buttonName = programGetString(program, value.opcode, value.integerValue);
        if (_windowDeleteButton(buttonName)) {
            return;
        }
    }

    programFatalError("Error deleting button");
}

// fillwin
// 0x46449C
void opFillWin(Program* program)
{
    ProgramValue b = programStackPopValue(program);
    ProgramValue g = programStackPopValue(program);
    ProgramValue r = programStackPopValue(program);

    if ((r.opcode & VALUE_TYPE_MASK) != VALUE_TYPE_FLOAT) {
        if ((r.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_INT) {
            if (r.integerValue == 1) {
                r.floatValue = 1.0;
            } else if (r.integerValue != 0) {
                programFatalError("Invalid red value given to fillwin");
            }
        }
    }

    if ((g.opcode & VALUE_TYPE_MASK) != VALUE_TYPE_FLOAT) {
        if ((g.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_INT) {
            if (g.integerValue == 1) {
                g.floatValue = 1.0;
            } else if (g.integerValue != 0) {
                programFatalError("Invalid green value given to fillwin");
            }
        }
    }

    if ((b.opcode & VALUE_TYPE_MASK) != VALUE_TYPE_FLOAT) {
        if ((b.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_INT) {
            if (b.integerValue == 1) {
                b.floatValue = 1.0;
            } else if (b.integerValue != 0) {
                programFatalError("Invalid blue value given to fillwin");
            }
        }
    }

    _selectWindowID(program->windowId);

    _windowFill(r.floatValue, g.floatValue, b.floatValue);
}

// fillrect
// 0x4645FC
void opFillRect(Program* program)
{
    ProgramValue b = programStackPopValue(program);
    ProgramValue g = programStackPopValue(program);
    ProgramValue r = programStackPopValue(program);
    int height = programStackPopInteger(program);
    int width = programStackPopInteger(program);
    int y = programStackPopInteger(program);
    int x = programStackPopInteger(program);

    if ((r.opcode & VALUE_TYPE_MASK) != VALUE_TYPE_FLOAT) {
        if ((r.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_INT) {
            if (r.integerValue == 1) {
                r.floatValue = 1.0;
            } else if (r.integerValue != 0) {
                programFatalError("Invalid red value given to fillwin");
            }
        }
    }

    if ((g.opcode & VALUE_TYPE_MASK) != VALUE_TYPE_FLOAT) {
        if ((g.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_INT) {
            if (g.integerValue == 1) {
                g.floatValue = 1.0;
            } else if (g.integerValue != 0) {
                programFatalError("Invalid green value given to fillwin");
            }
        }
    }

    if ((b.opcode & VALUE_TYPE_MASK) != VALUE_TYPE_FLOAT) {
        if ((b.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_INT) {
            if (b.integerValue == 1) {
                b.floatValue = 1.0;
            } else if (b.integerValue != 0) {
                programFatalError("Invalid blue value given to fillwin");
            }
        }
    }

    _selectWindowID(program->windowId);

    _windowFillRect(x, y, width, height, r.floatValue, g.floatValue, b.floatValue);
}

// hidemouse
// 0x46489C
static void opHideMouse(Program* program)
{
    mouseHideCursor();
}

// showmouse
// 0x4648A4
static void opShowMouse(Program* program)
{
    mouseShowCursor();
}

// mouseshape
// 0x4648AC
void opMouseShape(Program* program)
{
    int v1 = programStackPopInteger(program);
    int v2 = programStackPopInteger(program);
    char* fileName = programStackPopString(program);

    if (!mouseManagerSetMouseShape(fileName, v2, v1)) {
        programFatalError("Error loading mouse shape.");
    }
}

// setglobalmousefunc
// 0x4649C4
static void opSetGlobalMouseFunc(Program* Program)
{
    programFatalError("setglobalmousefunc not defined");
}

// displaygfx
// 0x4649D4
void opDisplayGfx(Program* program)
{
    int height = programStackPopInteger(program);
    int width = programStackPopInteger(program);
    int y = programStackPopInteger(program);
    int x = programStackPopInteger(program);
    char* fileName = programStackPopString(program);

    char* mangledFileName = _interpretMangleName(fileName);
    _windowDisplay(mangledFileName, x, y, width, height);
}

// loadpalettetable
// 0x464ADC
static void opLoadPaletteTable(Program* program)
{
    char* path = programStackPopString(program);
    if (!colorPaletteLoad(path)) {
        programFatalError(_colorError());
    }
}

// addnamedevent
// 0x464B54
static void opAddNamedEvent(Program* program)
{
    int proc = programStackPopInteger(program);
    const char* name = programStackPopString(program);
    _nevs_addevent(name, program, proc, NEVS_TYPE_EVENT);
}

// addnamedhandler
// 0x464BE8
static void opAddNamedHandler(Program* program)
{
    int proc = programStackPopInteger(program);
    const char* name = programStackPopString(program);
    _nevs_addevent(name, program, proc, NEVS_TYPE_HANDLER);
}

// clearnamed
// 0x464C80
static void opClearNamed(Program* program)
{
    char* string = programStackPopString(program);
    _nevs_clearevent(string);
}

// signalnamed
// 0x464CE4
static void opSignalNamed(Program* program)
{
    char* str = programStackPopString(program);
    _nevs_signal(str);
}

// addkey
// 0x464D48
static void opAddKey(Program* program)
{
    int proc = programStackPopInteger(program);
    int key = programStackPopInteger(program);

    if (key == -1) {
        gIntLibGenericKeyHandlerProc = proc;
        gIntLibGenericKeyHandlerProgram = program;
    } else {
        if (key > INT_LIB_KEY_HANDLERS_CAPACITY - 1) {
            programFatalError("Key out of range");
        }

        gIntLibKeyHandlerEntries[key].program = program;
        gIntLibKeyHandlerEntries[key].proc = proc;
    }
}

// deletekey
// 0x464E24
static void opDeleteKey(Program* program)
{
    int key = programStackPopInteger(program);

    if (key == -1) {
        gIntLibGenericKeyHandlerProc = 0;
        gIntLibGenericKeyHandlerProgram = nullptr;
    } else {
        if (key > INT_LIB_KEY_HANDLERS_CAPACITY - 1) {
            programFatalError("Key out of range");
        }

        gIntLibKeyHandlerEntries[key].program = nullptr;
        gIntLibKeyHandlerEntries[key].proc = 0;
    }
}

// refreshmouse
// 0x464EB0
void opRefreshMouse(Program* program)
{
    int data = programStackPopInteger(program);

    if (!_windowRefreshRegions()) {
        _executeProc(program, data);
    }
}

// setfont
// 0x464F18
static void opSetFont(Program* program)
{
    int data = programStackPopInteger(program);

    if (!windowSetFont(data)) {
        programFatalError("Error setting font");
    }
}

// setflags
// 0x464F84
static void opSetTextFlags(Program* program)
{
    int data = programStackPopInteger(program);

    if (!windowSetTextFlags(data)) {
        programFatalError("Error setting text flags");
    }
}

// settextcolor
// 0x464FF0
static void opSetTextColor(Program* program)
{
    ProgramValue value[3];

    // NOTE: Original code does not use loops.
    for (int arg = 0; arg < 3; arg++) {
        value[arg] = programStackPopValue(program);
    }

    for (int arg = 0; arg < 3; arg++) {
        if ((value[arg].opcode & VALUE_TYPE_MASK) != VALUE_TYPE_FLOAT
            && (value[arg].opcode & VALUE_TYPE_MASK) == VALUE_TYPE_INT
            && value[arg].integerValue != 0) {
            programFatalError("Invalid type given to settextcolor");
        }
    }

    float r = value[2].floatValue;
    float g = value[1].floatValue;
    float b = value[0].floatValue;

    if (!windowSetTextColor(r, g, b)) {
        programFatalError("Error setting text color");
    }
}

// sayoptioncolor
// 0x465140
static void opSayOptionColor(Program* program)
{
    ProgramValue value[3];

    // NOTE: Original code does not use loops.
    for (int arg = 0; arg < 3; arg++) {
        value[arg] = programStackPopValue(program);
    }

    for (int arg = 0; arg < 3; arg++) {
        if ((value[arg].opcode & VALUE_TYPE_MASK) != VALUE_TYPE_FLOAT
            && (value[arg].opcode & VALUE_TYPE_MASK) == VALUE_TYPE_INT
            && value[arg].integerValue != 0) {
            programFatalError("Invalid type given to sayoptioncolor");
        }
    }

    float r = value[2].floatValue;
    float g = value[1].floatValue;
    float b = value[0].floatValue;

    if (dialogSetOptionColor(r, g, b)) {
        programFatalError("Error setting option color");
    }
}

// sayreplycolor
// 0x465290
static void opSayReplyColor(Program* program)
{
    ProgramValue value[3];

    // NOTE: Original code does not use loops.
    for (int arg = 0; arg < 3; arg++) {
        value[arg] = programStackPopValue(program);
    }

    for (int arg = 0; arg < 3; arg++) {
        if ((value[arg].opcode & VALUE_TYPE_MASK) != VALUE_TYPE_FLOAT
            && (value[arg].opcode & VALUE_TYPE_MASK) == VALUE_TYPE_INT
            && value[arg].integerValue != 0) {
            programFatalError("Invalid type given to sayreplycolor");
        }
    }

    float r = value[2].floatValue;
    float g = value[1].floatValue;
    float b = value[0].floatValue;

    if (dialogSetReplyColor(r, g, b) != 0) {
        programFatalError("Error setting reply color");
    }
}

// sethighlightcolor
// 0x4653E0
static void opSetHighlightColor(Program* program)
{
    ProgramValue value[3];

    // NOTE: Original code does not use loops.
    for (int arg = 0; arg < 3; arg++) {
        value[arg] = programStackPopValue(program);
    }

    for (int arg = 0; arg < 3; arg++) {
        if ((value[arg].opcode & VALUE_TYPE_MASK) != VALUE_TYPE_FLOAT
            && (value[arg].opcode & VALUE_TYPE_MASK) == VALUE_TYPE_INT
            && value[arg].integerValue != 0) {
            programFatalError("Invalid type given to sayreplycolor");
        }
    }

    float r = value[2].floatValue;
    float g = value[1].floatValue;
    float b = value[0].floatValue;

    if (!windowSetHighlightColor(r, g, b)) {
        programFatalError("Error setting text highlight color");
    }
}

// sayreplywindow
// 0x465530
void opSayReplyWindow(Program* program)
{
    ProgramValue v2 = programStackPopValue(program);
    int height = programStackPopInteger(program);
    int width = programStackPopInteger(program);
    int y = programStackPopInteger(program);
    int x = programStackPopInteger(program);

    char* v1;
    if ((v2.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
        v1 = programGetString(program, v2.opcode, v2.integerValue);
        v1 = _interpretMangleName(v1);
        v1 = strdup_safe(v1, __FILE__, __LINE__); // "..\\int\\INTLIB.C", 1510
    } else if ((v2.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_INT && v2.integerValue == 0) {
        v1 = nullptr;
    } else {
        programFatalError("Invalid arg 5 given to sayreplywindow");
    }

    if (dialogSetReplyWindow(x, y, width, height, v1) != 0) {
        programFatalError("Error setting reply window");
    }
}

// sayreplyflags
// 0x465688
static void opSayReplyFlags(Program* program)
{
    int data = programStackPopInteger(program);

    if (!_dialogSetOptionFlags(data)) {
        programFatalError("Error setting reply flags");
    }
}

// sayoptionflags
// 0x4656F4
static void opSayOptionFlags(Program* program)
{
    int data = programStackPopInteger(program);

    if (!_dialogSetOptionFlags(data)) {
        programFatalError("Error setting option flags");
    }
}

// sayoptionwindow
// 0x465760
void opSayOptionWindow(Program* program)
{
    ProgramValue v2 = programStackPopValue(program);
    int height = programStackPopInteger(program);
    int width = programStackPopInteger(program);
    int y = programStackPopInteger(program);
    int x = programStackPopInteger(program);

    char* v1;
    if ((v2.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
        v1 = programGetString(program, v2.opcode, v2.integerValue);
        v1 = _interpretMangleName(v1);
        v1 = strdup_safe(v1, __FILE__, __LINE__); // "..\\int\\INTLIB.C", 1556
    } else if ((v2.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_INT && v2.integerValue == 0) {
        v1 = nullptr;
    } else {
        programFatalError("Invalid arg 5 given to sayoptionwindow");
    }

    if (dialogSetOptionWindow(x, y, width, height, v1) != 0) {
        programFatalError("Error setting option window");
    }
}

// sayborder
// 0x4658B8
static void opSayBorder(Program* program)
{
    int y = programStackPopInteger(program);
    int x = programStackPopInteger(program);

    if (dialogSetBorder(x, y) != 0) {
        programFatalError("Error setting dialog border");
    }
}

// sayscrollup
// 0x465978
void opSayScrollUp(Program* program)
{
    ProgramValue v6 = programStackPopValue(program);
    ProgramValue v7 = programStackPopValue(program);
    ProgramValue v8 = programStackPopValue(program);
    ProgramValue v9 = programStackPopValue(program);
    int y = programStackPopInteger(program);
    int x = programStackPopInteger(program);

    char* v1 = nullptr;
    char* v2 = nullptr;
    char* v3 = nullptr;
    char* v4 = nullptr;
    int v5 = 0;

    if ((v6.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_INT) {
        if (v6.integerValue != -1 && v6.integerValue != 0) {
            programFatalError("Invalid arg 4 given to sayscrollup");
        }

        if (v6.integerValue == -1) {
            v5 = 1;
        }
    } else {
        if ((v6.opcode & VALUE_TYPE_MASK) != VALUE_TYPE_STRING) {
            programFatalError("Invalid arg 4 given to sayscrollup");
        }
    }

    if ((v7.opcode & VALUE_TYPE_MASK) != VALUE_TYPE_STRING && (v7.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_INT && v7.integerValue != 0) {
        programFatalError("Invalid arg 3 given to sayscrollup");
    }

    if ((v8.opcode & VALUE_TYPE_MASK) != VALUE_TYPE_STRING && (v8.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_INT && v8.integerValue != 0) {
        programFatalError("Invalid arg 2 given to sayscrollup");
    }

    if ((v9.opcode & VALUE_TYPE_MASK) != VALUE_TYPE_STRING && (v9.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_INT && v9.integerValue != 0) {
        programFatalError("Invalid arg 1 given to sayscrollup");
    }

    if ((v9.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
        v1 = programGetString(program, v9.opcode, v9.integerValue);
        v1 = _interpretMangleName(v1);
        v1 = strdup_safe(v1, __FILE__, __LINE__); // "..\\int\\INTLIB.C", 1611
    }

    if ((v8.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
        v2 = programGetString(program, v8.opcode, v8.integerValue);
        v2 = _interpretMangleName(v2);
        v2 = strdup_safe(v2, __FILE__, __LINE__); // "..\\int\\INTLIB.C", 1613
    }

    if ((v7.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
        v3 = programGetString(program, v7.opcode, v7.integerValue);
        v3 = _interpretMangleName(v3);
        v3 = strdup_safe(v3, __FILE__, __LINE__); // "..\\int\\INTLIB.C", 1615
    }

    if ((v6.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
        v4 = programGetString(program, v6.opcode, v6.integerValue);
        v4 = _interpretMangleName(v4);
        v4 = strdup_safe(v4, __FILE__, __LINE__); // "..\\int\\INTLIB.C", 1617
    }

    if (_dialogSetScrollUp(x, y, v1, v2, v3, v4, v5) != 0) {
        programFatalError("Error setting scroll up");
    }
}

// sayscrolldown
// 0x465CAC
void opSayScrollDown(Program* program)
{
    ProgramValue v6 = programStackPopValue(program);
    ProgramValue v7 = programStackPopValue(program);
    ProgramValue v8 = programStackPopValue(program);
    ProgramValue v9 = programStackPopValue(program);
    int y = programStackPopInteger(program);
    int x = programStackPopInteger(program);

    char* v1 = nullptr;
    char* v2 = nullptr;
    char* v3 = nullptr;
    char* v4 = nullptr;
    int v5 = 0;

    if ((v6.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_INT) {
        if (v6.integerValue != -1 && v6.integerValue != 0) {
            // FIXME: Wrong function name, should be sayscrolldown.
            programFatalError("Invalid arg 4 given to sayscrollup");
        }

        if (v6.integerValue == -1) {
            v5 = 1;
        }
    } else {
        if ((v6.opcode & VALUE_TYPE_MASK) != VALUE_TYPE_STRING) {
            // FIXME: Wrong function name, should be sayscrolldown.
            programFatalError("Invalid arg 4 given to sayscrollup");
        }
    }

    if ((v7.opcode & VALUE_TYPE_MASK) != VALUE_TYPE_STRING && (v7.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_INT && v7.integerValue != 0) {
        programFatalError("Invalid arg 3 given to sayscrolldown");
    }

    if ((v8.opcode & VALUE_TYPE_MASK) != VALUE_TYPE_STRING && (v8.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_INT && v8.integerValue != 0) {
        programFatalError("Invalid arg 2 given to sayscrolldown");
    }

    if ((v9.opcode & VALUE_TYPE_MASK) != VALUE_TYPE_STRING && (v9.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_INT && v9.integerValue != 0) {
        programFatalError("Invalid arg 1 given to sayscrolldown");
    }

    if ((v9.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
        v1 = programGetString(program, v9.opcode, v9.integerValue);
        v1 = _interpretMangleName(v1);
        v1 = strdup_safe(v1, __FILE__, __LINE__); // "..\\int\\INTLIB.C", 1652
    }

    if ((v8.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
        v2 = programGetString(program, v8.opcode, v8.integerValue);
        v2 = _interpretMangleName(v2);
        v2 = strdup_safe(v2, __FILE__, __LINE__); // "..\\int\\INTLIB.C", 1654
    }

    if ((v7.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
        v3 = programGetString(program, v7.opcode, v7.integerValue);
        v3 = _interpretMangleName(v3);
        v3 = strdup_safe(v3, __FILE__, __LINE__); // "..\\int\\INTLIB.C", 1656
    }

    if ((v6.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
        v4 = programGetString(program, v6.opcode, v6.integerValue);
        v4 = _interpretMangleName(v4);
        v4 = strdup_safe(v4, __FILE__, __LINE__); // "..\\int\\INTLIB.C", 1658
    }

    if (_dialogSetScrollDown(x, y, v1, v2, v3, v4, v5) != 0) {
        programFatalError("Error setting scroll down");
    }
}

// saysetspacing
// 0x465FE0
static void opSaySetSpacing(Program* program)
{
    int data = programStackPopInteger(program);

    if (dialogSetOptionSpacing(data) != 0) {
        programFatalError("Error setting option spacing");
    }
}

// sayrestart
// 0x46604C
static void opSayRestart(Program* program)
{
    if (_dialogRestart() != 0) {
        programFatalError("Error restarting option");
    }
}

// 0x466064
static void intLibSoundCallback(void* userData, int a2)
{
    if (a2 == 1) {
        Sound** sound = (Sound**)userData;
        *sound = nullptr;
    }
}

// 0x466070
static int intLibSoundDelete(int value)
{
    if (value == -1) {
        return 1;
    }

    if ((value & 0xA0000000) == 0) {
        return 0;
    }

    int index = value & ~0xA0000000;
    Sound* sound = gIntLibSounds[index];
    if (sound == nullptr) {
        return 0;
    }

    if (soundIsPlaying(sound)) {
        soundStop(sound);
    }

    soundDelete(sound);

    gIntLibSounds[index] = nullptr;

    return 1;
}

// 0x466110
static int intLibSoundPlay(char* fileName, int mode)
{
    int type = SOUND_TYPE_MEMORY;
    int soundFlags = 0;

    if (mode & 0x01) {
        soundFlags |= SOUND_LOOPING;
    } else {
        type |= SOUND_TYPE_FIRE_AND_FORGET;
    }

    if (mode & 0x02) {
        soundFlags |= SOUND_16BIT;
    } else {
        soundFlags |= SOUND_8BIT;
    }

    if (mode & 0x0100) {
        type &= ~(SOUND_TYPE_MEMORY | SOUND_TYPE_STREAMING);
        type |= SOUND_TYPE_MEMORY;
    }

    if (mode & 0x0200) {
        type &= ~(SOUND_TYPE_MEMORY | SOUND_TYPE_STREAMING);
        type |= SOUND_TYPE_STREAMING;
    }

    int index;
    for (index = 0; index < INT_LIB_SOUNDS_CAPACITY; index++) {
        if (gIntLibSounds[index] == nullptr) {
            break;
        }
    }

    if (index == INT_LIB_SOUNDS_CAPACITY) {
        return -1;
    }

    Sound* sound = gIntLibSounds[index] = soundAllocate(type, soundFlags);
    if (sound == nullptr) {
        return -1;
    }

    soundSetCallback(sound, intLibSoundCallback, &(gIntLibSounds[index]));

    if (mode & 0x01) {
        soundSetLooping(sound, 0xFFFF);
    }

    if (mode & 0x1000) {
        // mono
        soundSetChannels(sound, 2);
    }

    if (mode & 0x2000) {
        // stereo
        soundSetChannels(sound, 3);
    }

    int rc = soundLoad(sound, fileName);
    if (rc != SOUND_NO_ERROR) {
        goto err;
    }

    rc = soundPlay(sound);

    // TODO: Maybe wrong.
    switch (rc) {
    case SOUND_NO_DEVICE:
        debugPrint("soundPlay error: %s\n", "SOUND_NO_DEVICE");
        goto err;
    case SOUND_NOT_INITIALIZED:
        debugPrint("soundPlay error: %s\n", "SOUND_NOT_INITIALIZED");
        goto err;
    case SOUND_NO_SOUND:
        debugPrint("soundPlay error: %s\n", "SOUND_NO_SOUND");
        goto err;
    case SOUND_FUNCTION_NOT_SUPPORTED:
        debugPrint("soundPlay error: %s\n", "SOUND_FUNC_NOT_SUPPORTED");
        goto err;
    case SOUND_NO_BUFFERS_AVAILABLE:
        debugPrint("soundPlay error: %s\n", "SOUND_NO_BUFFERS_AVAILABLE");
        goto err;
    case SOUND_FILE_NOT_FOUND:
        debugPrint("soundPlay error: %s\n", "SOUND_FILE_NOT_FOUND");
        goto err;
    case SOUND_ALREADY_PLAYING:
        debugPrint("soundPlay error: %s\n", "SOUND_ALREADY_PLAYING");
        goto err;
    case SOUND_NOT_PLAYING:
        debugPrint("soundPlay error: %s\n", "SOUND_NOT_PLAYING");
        goto err;
    case SOUND_ALREADY_PAUSED:
        debugPrint("soundPlay error: %s\n", "SOUND_ALREADY_PAUSED");
        goto err;
    case SOUND_NOT_PAUSED:
        debugPrint("soundPlay error: %s\n", "SOUND_NOT_PAUSED");
        goto err;
    case SOUND_INVALID_HANDLE:
        debugPrint("soundPlay error: %s\n", "SOUND_INVALID_HANDLE");
        goto err;
    case SOUND_NO_MEMORY_AVAILABLE:
        debugPrint("soundPlay error: %s\n", "SOUND_NO_MEMORY");
        goto err;
    case SOUND_UNKNOWN_ERROR:
        debugPrint("soundPlay error: %s\n", "SOUND_ERROR");
        goto err;
    }

    return index | 0xA0000000;

err:

    soundDelete(sound);
    gIntLibSounds[index] = nullptr;
    return -1;
}

// 0x46655C
static int intLibSoundPause(int value)
{
    if (value == -1) {
        return 1;
    }

    if ((value & 0xA0000000) == 0) {
        return 0;
    }

    int index = value & ~0xA0000000;
    Sound* sound = gIntLibSounds[index];
    if (sound == nullptr) {
        return 0;
    }

    int rc;
    if (_soundType(sound, SOUND_TYPE_MEMORY)) {
        rc = soundStop(sound);
    } else {
        rc = soundPause(sound);
    }
    return rc == SOUND_NO_ERROR;
}

// 0x4665C8
static int intLibSoundRewind(int value)
{
    if (value == -1) {
        return 1;
    }

    if ((value & 0xA0000000) == 0) {
        return 0;
    }

    int index = value & ~0xA0000000;
    Sound* sound = gIntLibSounds[index];
    if (sound == nullptr) {
        return 0;
    }

    if (!soundIsPlaying(sound)) {
        return 1;
    }

    soundStop(sound);

    return soundPlay(sound) == SOUND_NO_ERROR;
}

// 0x46662C
static int intLibSoundResume(int value)
{
    if (value == -1) {
        return 1;
    }

    if ((value & 0xA0000000) == 0) {
        return 0;
    }

    int index = value & ~0xA0000000;
    Sound* sound = gIntLibSounds[index];
    if (sound == nullptr) {
        return 0;
    }

    int rc;
    if (_soundType(sound, SOUND_TYPE_MEMORY)) {
        rc = soundPlay(sound);
    } else {
        rc = soundResume(sound);
    }
    return rc == SOUND_NO_ERROR;
}

// soundplay
// 0x466698
static void opSoundPlay(Program* program)
{
    int flags = programStackPopInteger(program);
    char* fileName = programStackPopString(program);

    char* mangledFileName = _interpretMangleName(fileName);
    int rc = intLibSoundPlay(mangledFileName, flags);

    programStackPushInteger(program, rc);
}

// soundpause
// 0x466768
static void opSoundPause(Program* program)
{
    int data = programStackPopInteger(program);
    intLibSoundPause(data);
}

// soundresume
// 0x4667C0
static void opSoundResume(Program* program)
{
    int data = programStackPopInteger(program);
    intLibSoundResume(data);
}

// soundstop
// 0x466818
static void opSoundStop(Program* program)
{
    int data = programStackPopInteger(program);
    intLibSoundPause(data);
}

// soundrewind
// 0x466870
static void opSoundRewind(Program* program)
{
    int data = programStackPopInteger(program);
    intLibSoundRewind(data);
}

// sounddelete
// 0x4668C8
static void opSoundDelete(Program* program)
{
    int data = programStackPopInteger(program);
    intLibSoundDelete(data);
}

// SetOneOptPause
// 0x466920
static void opSetOneOptPause(Program* program)
{
    int data = programStackPopInteger(program);

    if (data) {
        if ((_dialogGetMediaFlag() & 8) == 0) {
            return;
        }
    } else {
        if ((_dialogGetMediaFlag() & 8) != 0) {
            return;
        }
    }

    _dialogToggleMediaFlag(8);
}

// 0x466994
void intLibUpdate()
{
    _nevs_update();
    intExtraUpdate();
}

// 0x4669A0
void intLibExit()
{
    _dialogClose();
    _intExtraClose_();

    for (int index = 0; index < INT_LIB_SOUNDS_CAPACITY; index++) {
        if (gIntLibSounds[index] != nullptr) {
            intLibSoundDelete(index | 0xA0000000);
        }
    }

    _nevs_close();

    if (gIntLibProgramDeleteCallbacks != nullptr) {
        internal_free_safe(gIntLibProgramDeleteCallbacks, __FILE__, __LINE__); // "..\\int\\INTLIB.C", 1976
        gIntLibProgramDeleteCallbacks = nullptr;
        gIntLibProgramDeleteCallbacksLength = 0;
    }
}

// 0x466A04
static bool intLibDoInput(int key)
{
    if (key < 0 || key >= INT_LIB_KEY_HANDLERS_CAPACITY) {
        return false;
    }

    if (gIntLibGenericKeyHandlerProgram != nullptr) {
        if (gIntLibGenericKeyHandlerProc != 0) {
            _executeProc(gIntLibGenericKeyHandlerProgram, gIntLibGenericKeyHandlerProc);
        }
        return true;
    }

    IntLibKeyHandlerEntry* entry = &(gIntLibKeyHandlerEntries[key]);
    if (entry->program == nullptr) {
        return false;
    }

    if (entry->proc != 0) {
        _executeProc(entry->program, entry->proc);
    }

    return true;
}

// 0x466A70
void intLibInit()
{
    _windowAddInputFunc(intLibDoInput);

    interpreterRegisterOpcode(0x806A, opFillWin3x3);
    interpreterRegisterOpcode(0x808C, opDeleteButton);
    interpreterRegisterOpcode(0x8086, opAddButton);
    interpreterRegisterOpcode(0x8088, opAddButtonFlag);
    interpreterRegisterOpcode(0x8087, opAddButtonText);
    interpreterRegisterOpcode(0x8089, opAddButtonGfx);
    interpreterRegisterOpcode(0x808A, opAddButtonProc);
    interpreterRegisterOpcode(0x808B, opAddButtonRightProc);
    interpreterRegisterOpcode(0x8067, opShowWin);
    interpreterRegisterOpcode(0x8068, opFillWin);
    interpreterRegisterOpcode(0x8069, opFillRect);
    interpreterRegisterOpcode(0x8072, opPrint);
    interpreterRegisterOpcode(0x8073, opFormat);
    interpreterRegisterOpcode(0x8074, opPrintRect);
    interpreterRegisterOpcode(0x8075, opSetFont);
    interpreterRegisterOpcode(0x8076, opSetTextFlags);
    interpreterRegisterOpcode(0x8077, opSetTextColor);
    interpreterRegisterOpcode(0x8078, opSetHighlightColor);
    interpreterRegisterOpcode(0x8064, opSelect);
    interpreterRegisterOpcode(0x806B, opDisplay);
    interpreterRegisterOpcode(0x806D, opDisplayRaw);
    interpreterRegisterOpcode(0x806C, opDisplayGfx);
    interpreterRegisterOpcode(0x806F, opFadeIn);
    interpreterRegisterOpcode(0x8070, opFadeOut);
    interpreterRegisterOpcode(0x807A, opPlayMovie);
    interpreterRegisterOpcode(0x807B, opSetMovieFlags);
    interpreterRegisterOpcode(0x807C, opPlayMovieRect);
    interpreterRegisterOpcode(0x8079, opStopMovie);
    interpreterRegisterOpcode(0x807F, opAddRegion);
    interpreterRegisterOpcode(0x8080, opAddRegionFlag);
    interpreterRegisterOpcode(0x8081, opAddRegionProc);
    interpreterRegisterOpcode(0x8082, opAddRegionRightProc);
    interpreterRegisterOpcode(0x8083, opDeleteRegion);
    interpreterRegisterOpcode(0x8084, opActivateRegion);
    interpreterRegisterOpcode(0x8085, opCheckRegion);
    interpreterRegisterOpcode(0x8062, opCreateWin);
    interpreterRegisterOpcode(0x8063, opDeleteWin);
    interpreterRegisterOpcode(0x8065, opResizeWin);
    interpreterRegisterOpcode(0x8066, opScaleWin);
    interpreterRegisterOpcode(0x804E, opSayStart);
    interpreterRegisterOpcode(0x804F, opSayStartPos);
    interpreterRegisterOpcode(0x8050, opSayReplyTitle);
    interpreterRegisterOpcode(0x8051, opSayGoToReply);
    interpreterRegisterOpcode(0x8053, opSayReply);
    interpreterRegisterOpcode(0x8052, opSayOption);
    interpreterRegisterOpcode(0x804D, opSayEnd);
    interpreterRegisterOpcode(0x804C, opSayQuit);
    interpreterRegisterOpcode(0x8054, opSayMessage);
    interpreterRegisterOpcode(0x8055, opSayReplyWindow);
    interpreterRegisterOpcode(0x8056, opSayOptionWindow);
    interpreterRegisterOpcode(0x805F, opSayReplyFlags);
    interpreterRegisterOpcode(0x8060, opSayOptionFlags);
    interpreterRegisterOpcode(0x8057, opSayBorder);
    interpreterRegisterOpcode(0x8058, opSayScrollUp);
    interpreterRegisterOpcode(0x8059, opSayScrollDown);
    interpreterRegisterOpcode(0x805A, opSaySetSpacing);
    interpreterRegisterOpcode(0x805B, opSayOptionColor);
    interpreterRegisterOpcode(0x805C, opSayReplyColor);
    interpreterRegisterOpcode(0x805D, opSayRestart);
    interpreterRegisterOpcode(0x805E, opSayGetLastPos);
    interpreterRegisterOpcode(0x8061, opSayMessageTimeout);
    interpreterRegisterOpcode(0x8071, opGotoXY);
    interpreterRegisterOpcode(0x808D, opHideMouse);
    interpreterRegisterOpcode(0x808E, opShowMouse);
    interpreterRegisterOpcode(0x8090, opRefreshMouse);
    interpreterRegisterOpcode(0x808F, opMouseShape);
    interpreterRegisterOpcode(0x8091, opSetGlobalMouseFunc);
    interpreterRegisterOpcode(0x806E, opLoadPaletteTable);
    interpreterRegisterOpcode(0x8092, opAddNamedEvent);
    interpreterRegisterOpcode(0x8093, opAddNamedHandler);
    interpreterRegisterOpcode(0x8094, opClearNamed);
    interpreterRegisterOpcode(0x8095, opSignalNamed);
    interpreterRegisterOpcode(0x8096, opAddKey);
    interpreterRegisterOpcode(0x8097, opDeleteKey);
    interpreterRegisterOpcode(0x8098, opSoundPlay);
    interpreterRegisterOpcode(0x8099, opSoundPause);
    interpreterRegisterOpcode(0x809A, opSoundResume);
    interpreterRegisterOpcode(0x809B, opSoundStop);
    interpreterRegisterOpcode(0x809C, opSoundRewind);
    interpreterRegisterOpcode(0x809D, opSoundDelete);
    interpreterRegisterOpcode(0x809E, opSetOneOptPause);
    interpreterRegisterOpcode(0x809F, opSelectFileList);
    interpreterRegisterOpcode(0x80A0, opTokenize);

    _nevs_initonce();
    _initIntExtra();
    dialogInit();
}

// 0x466F6C
void intLibRegisterProgramDeleteCallback(IntLibProgramDeleteCallback* callback)
{
    int index;
    for (index = 0; index < gIntLibProgramDeleteCallbacksLength; index++) {
        if (gIntLibProgramDeleteCallbacks[index] == nullptr) {
            break;
        }
    }

    if (index == gIntLibProgramDeleteCallbacksLength) {
        if (gIntLibProgramDeleteCallbacks != nullptr) {
            gIntLibProgramDeleteCallbacks = (IntLibProgramDeleteCallback**)internal_realloc_safe(gIntLibProgramDeleteCallbacks, sizeof(*gIntLibProgramDeleteCallbacks) * (gIntLibProgramDeleteCallbacksLength + 1), __FILE__, __LINE__); // ..\\int\\INTLIB.C, 2110
        } else {
            gIntLibProgramDeleteCallbacks = (IntLibProgramDeleteCallback**)internal_malloc_safe(sizeof(*gIntLibProgramDeleteCallbacks), __FILE__, __LINE__); // ..\\int\\INTLIB.C, 2112
        }
        gIntLibProgramDeleteCallbacksLength++;
    }

    gIntLibProgramDeleteCallbacks[index] = callback;
}

// 0x467040
void intLibRemoveProgramReferences(Program* program)
{
    for (int index = 0; index < INT_LIB_KEY_HANDLERS_CAPACITY; index++) {
        if (program == gIntLibKeyHandlerEntries[index].program) {
            gIntLibKeyHandlerEntries[index].program = nullptr;
        }
    }

    intExtraRemoveProgramReferences(program);

    for (int index = 0; index < gIntLibProgramDeleteCallbacksLength; index++) {
        IntLibProgramDeleteCallback* callback = gIntLibProgramDeleteCallbacks[index];
        if (callback != nullptr) {
            callback(program);
        }
    }
}

} // namespace fallout
