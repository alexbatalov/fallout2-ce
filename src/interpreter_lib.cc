#include "interpreter_lib.h"

#include "color.h"
#include "core.h"
#include "debug.h"
#include "dialog.h"
#include "interpreter_extra.h"
#include "memory_manager.h"
#include "nevs.h"
#include "sound.h"
#include "widget.h"

#define INTERPRETER_SOUNDS_LENGTH (32)
#define INTERPRETER_KEY_HANDLER_ENTRIES_LENGTH (256)

typedef struct InterpreterKeyHandlerEntry {
    Program* program;
    int proc;
} InterpreterKeyHandlerEntry;

static void opFormat(Program* program);
static void opPrint(Program* program);
static void opPrintRect(Program* program);
static void opSetMovieFlags(Program* program);
static void opStopMovie(Program* program);
static void opAddRegionProc(Program* program);
static void opAddRegionRightProc(Program* program);
static void opSayStart(Program* program);
static void opDeleteRegion(Program* program);
static void opCheckRegion(Program* program);
static void opSayStartPos(Program* program);
static void opSayReplyTitle(Program* program);
static void opSayGoToReply(Program* program);
static void opSayGetLastPos(Program* program);
static void opSayQuit(Program* program);
static void opSayMessageTimeout(Program* program);
static void opAddButtonFlag(Program* program);
static void opAddRegionFlag(Program* program);
static void opAddButtonProc(Program* program);
static void opAddButtonRightProc(Program* program);
static void opShowWin(Program* program);
static void opDeleteButton(Program* program);
static void opHideMouse(Program* program);
static void opShowMouse(Program* program);
static void opSetGlobalMouseFunc(Program* Program);
static void opLoadPaletteTable(Program* program);
static void opAddNamedEvent(Program* program);
static void opAddNamedHandler(Program* program);
static void opClearNamed(Program* program);
static void opSignalNamed(Program* program);
static void opAddKey(Program* program);
static void opDeleteKey(Program* program);
static void opSetFont(Program* program);
static void opSetTextFlags(Program* program);
static void opSetTextColor(Program* program);
static void opSayOptionColor(Program* program);
static void opSayReplyColor(Program* program);
static void opSetHighlightColor(Program* program);
static void opSayReplyFlags(Program* program);
static void opSayOptionFlags(Program* program);
static void opSayBorder(Program* program);
static void opSaySetSpacing(Program* program);
static void opSayRestart(Program* program);
static void interpreterSoundCallback(void* userData, int a2);
static int interpreterSoundDelete(int a1);
static int interpreterSoundPlay(char* fileName, int mode);
static int interpreterSoundPause(int value);
static int interpreterSoundRewind(int value);
static int interpreterSoundResume(int value);
static void opSoundPlay(Program* program);
static void opSoundPause(Program* program);
static void opSoundResume(Program* program);
static void opSoundStop(Program* program);
static void opSoundRewind(Program* program);
static void opSoundDelete(Program* program);
static void opSetOneOptPause(Program* program);
static bool _intLibDoInput(int key);

// 0x59D5D0
static Sound* gInterpreterSounds[INTERPRETER_SOUNDS_LENGTH];

// 0x59D950
static InterpreterKeyHandlerEntry gInterpreterKeyHandlerEntries[INTERPRETER_KEY_HANDLER_ENTRIES_LENGTH];

// 0x59E154
static int gIntepreterAnyKeyHandlerProc;

// Number of entries in _callbacks.
//
// 0x59E158
static int _numCallbacks;

// 0x59E15C
static Program* gInterpreterAnyKeyHandlerProgram;

// 0x59E160
static OFF_59E160* _callbacks;

// 0x59E164
static int _sayStartingPosition;

// format
// 0x461850
static void opFormat(Program* program)
{
    opcode_t opcode[6];
    int data[6];

    // NOTE: Original code does not use loop.
    for (int arg = 0; arg < 6; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }
    }

    if ((opcode[0] & 0xF7FF) != VALUE_TYPE_INT) {
        programFatalError("Invalid arg 6 given to format\n");
    }

    if ((opcode[1] & 0xF7FF) != VALUE_TYPE_INT) {
        programFatalError("Invalid arg 5 given to format\n");
    }

    if ((opcode[2] & 0xF7FF) != VALUE_TYPE_INT) {
        programFatalError("Invalid arg 4 given to format\n");
    }

    if ((opcode[3] & 0xF7FF) != VALUE_TYPE_INT) {
        programFatalError("Invalid arg 3 given to format\n");
    }

    if ((opcode[4] & 0xF7FF) != VALUE_TYPE_INT) {
        programFatalError("Invalid arg 2 given to format\n");
    }

    if ((opcode[5] & 0xF7FF) != VALUE_TYPE_STRING) {
        programFatalError("Invalid arg 1 given to format\n");
    }

    char* string = programGetString(program, opcode[5], data[5]);
    int x = data[4];
    int y = data[3];
    int width = data[2];
    int height = data[1];
    int textAlignment = data[0];

    if (!_windowFormatMessage(string, x, y, width, height, textAlignment)) {
        programFatalError("Error formatting message\n");
    }
}

// print
// 0x461A5C
static void opPrint(Program* program)
{
    _selectWindowID(program->field_84);

    opcode_t opcode = programStackPopInt16(program);
    int data = programStackPopInt32(program);

    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    switch (opcode & 0xF7FF) {
    case VALUE_TYPE_STRING:
        _interpretOutput("%s", programGetString(program, opcode, data));
        break;
    case VALUE_TYPE_FLOAT:
        _interpretOutput("%.5f", *((float*)&data));
        break;
    case VALUE_TYPE_INT:
        _interpretOutput("%d", data);
        break;
    }
}

// printrect
// 0x461F1C
static void opPrintRect(Program* program)
{
    _selectWindowID(program->field_84);

    opcode_t opcode[3];
    int data[3];

    // NOTE: Original code does not use loop.
    for (int arg = 0; arg < 3; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }
    }

    if ((opcode[0] & 0xF7FF) != VALUE_TYPE_INT || data[0] > 2) {
        programFatalError("Invalid arg 3 given to printrect, expecting int");
    }

    if ((opcode[1] & 0xF7FF) != VALUE_TYPE_INT) {
        programFatalError("Invalid arg 2 given to printrect, expecting int");
    }

    char string[80];
    switch (opcode[2] & 0xF7FF) {
    case VALUE_TYPE_STRING:
        sprintf(string, "%s", programGetString(program, opcode[2], data[2]));
        break;
    case VALUE_TYPE_FLOAT:
        sprintf(string, "%.5f", *((float*)&data[2]));
        break;
    case VALUE_TYPE_INT:
        sprintf(string, "%d", data[2]);
        break;
    }

    if (!_windowPrintRect(string, data[1], data[0])) {
        programFatalError("Error in printrect");
    }
}

// movieflags
// 0x462584
static void opSetMovieFlags(Program* program)
{
    opcode_t opcode = programStackPopInt16(program);
    int data = programStackPopInt32(program);

    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    if (!_windowSetMovieFlags(data)) {
        programFatalError("Error setting movie flags\n");
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
    opcode_t opcode = programStackPopInt16(program);
    int data = programStackPopInt32(program);

    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    if ((opcode & 0xF7FF) != VALUE_TYPE_STRING) {
        if ((opcode & 0xF7FF) == VALUE_TYPE_INT && data != -1) {
            programFatalError("Invalid type given to deleteregion");
        }
    }

    _selectWindowID(program->field_84);

    const char* regionName = data != -1 ? programGetString(program, opcode, data) : NULL;
    _windowDeleteRegion(regionName);
}

// checkregion
// 0x4629A0
static void opCheckRegion(Program* program)
{
    opcode_t opcode = programStackPopInt16(program);
    int data = programStackPopInt32(program);

    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    if ((opcode & 0xF7FF) != VALUE_TYPE_STRING) {
        programFatalError("Invalid arg 1 given to checkregion();\n");
    }

    const char* regionName = programGetString(program, opcode, data);

    bool regionExists = _windowCheckRegionExists(regionName);
    programStackPushInt32(program, regionExists);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// addregionproc
// 0x462C10
static void opAddRegionProc(Program* program)
{
    opcode_t opcode[5];
    int data[5];

    // NOTE: Original code does not use loop.
    for (int arg = 0; arg < 5; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }
    }

    if ((opcode[0] & 0xF7FF) != VALUE_TYPE_INT) {
        programFatalError("Invalid procedure 4 name given to addregionproc");
    }

    if ((opcode[1] & 0xF7FF) != VALUE_TYPE_INT) {
        programFatalError("Invalid procedure 3 name given to addregionproc");
    }

    if ((opcode[2] & 0xF7FF) != VALUE_TYPE_INT) {
        programFatalError("Invalid procedure 2 name given to addregionproc");
    }

    if ((opcode[3] & 0xF7FF) != VALUE_TYPE_INT) {
        programFatalError("Invalid procedure 1 name given to addregionproc");
    }

    if ((opcode[4] & 0xF7FF) != VALUE_TYPE_STRING) {
        programFatalError("Invalid name given to addregionproc");
    }

    const char* regionName = programGetString(program, opcode[4], data[4]);
    _selectWindowID(program->field_84);

    if (!_windowAddRegionProc(regionName, program, data[3], data[2], data[1], data[0])) {
        programFatalError("Error setting procedures to region %s\n", regionName);
    }
}

// addregionrightproc
// 0x462DDC
static void opAddRegionRightProc(Program* program)
{
    opcode_t opcode[3];
    int data[3];

    // NOTE: Original code does not use loop.
    for (int arg = 0; arg < 3; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }
    }

    if ((opcode[0] & 0xF7FF) != VALUE_TYPE_INT) {
        programFatalError("Invalid procedure 2 name given to addregionrightproc");
    }

    if ((opcode[1] & 0xF7FF) != VALUE_TYPE_INT) {
        programFatalError("Invalid procedure 1 name given to addregionrightproc");
    }

    if ((opcode[2] & 0xF7FF) != VALUE_TYPE_STRING) {
        programFatalError("Invalid name given to addregionrightproc");
    }

    const char* regionName = programGetString(program, opcode[2], data[2]);
    _selectWindowID(program->field_84);

    if (!_windowAddRegionRightProc(regionName, program, data[1], data[0])) {
        programFatalError("ErrorError setting right button procedures to region %s\n", regionName);
    }
}

// saystart
// 0x4633E4
static void opSayStart(Program* program)
{
    _sayStartingPosition = 0;

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
    opcode_t opcode = programStackPopInt16(program);
    int data = programStackPopInt32(program);

    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    _sayStartingPosition = data;

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
    opcode_t opcode = programStackPopInt16(program);
    int data = programStackPopInt32(program);

    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    char* string = NULL;
    if ((opcode & 0xF7FF) == VALUE_TYPE_STRING) {
        string = programGetString(program, opcode, data);
    }

    if (dialogSetReplyTitle(string) != 0) {
        programFatalError("Error setting title.");
    }
}

// saygotoreply
// 0x463510
static void opSayGoToReply(Program* program)
{
    opcode_t opcode = programStackPopInt16(program);
    int data = programStackPopInt32(program);

    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    char* string = NULL;
    if ((opcode & 0xF7FF) == VALUE_TYPE_STRING) {
        string = programGetString(program, opcode, data);
    }

    if (_dialogGotoReply(string) != 0) {
        programFatalError("Error during goto, couldn't find reply target %s", string);
    }
}

// saygetlastpos
// 0x4637EC
static void opSayGetLastPos(Program* program)
{
    int value = _dialogGetExitPoint();
    programStackPushInt32(program, value);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// sayquit
// 0x463810
static void opSayQuit(Program* program)
{
    if (_dialogQuit() != 0) {
        programFatalError("Error quitting option.");
    }
}

// saymessagetimeout
// 0x463838
static void opSayMessageTimeout(Program* program)
{
    opcode_t opcode = programStackPopInt16(program);
    int data = programStackPopInt32(program);

    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    // TODO: What the hell is this?
    if ((opcode & 0xF7FF) == 0x4000) {
        programFatalError("sayMsgTimeout:  invalid var type passed.");
    }

    _TimeOut = data;
}

// addbuttonflag
// 0x463A38
static void opAddButtonFlag(Program* program)
{
    opcode_t opcode[2];
    int data[2];

    // NOTE: Original code does not use loop.
    for (int arg = 0; arg < 2; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }
    }

    if ((opcode[0] & 0xF7FF) != VALUE_TYPE_INT) {
        programFatalError("Invalid arg 2 given to addbuttonflag");
    }

    if ((opcode[1] & 0xF7FF) != VALUE_TYPE_STRING) {
        programFatalError("Invalid arg 1 given to addbuttonflag");
    }

    const char* buttonName = programGetString(program, opcode[1], data[1]);
    if (!_windowSetButtonFlag(buttonName, data[0])) {
        // NOTE: Original code calls programGetString one more time with the
        // same params.
        programFatalError("Error setting flag on button %s", buttonName);
    }
}

// addregionflag
// 0x463B10
static void opAddRegionFlag(Program* program)
{
    opcode_t opcode[2];
    int data[2];

    // NOTE: Original code does not use loop.
    for (int arg = 0; arg < 2; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }
    }

    if ((opcode[0] & 0xF7FF) != VALUE_TYPE_INT) {
        programFatalError("Invalid arg 2 given to addregionflag");
    }

    if ((opcode[1] & 0xF7FF) != VALUE_TYPE_STRING) {
        programFatalError("Invalid arg 1 given to addregionflag");
    }

    const char* regionName = programGetString(program, opcode[1], data[1]);
    if (!_windowSetRegionFlag(regionName, data[0])) {
        // NOTE: Original code calls programGetString one more time with the
        // same params.
        programFatalError("Error setting flag on region %s", regionName);
    }
}

// addbuttonproc
// 0x4640DC
static void opAddButtonProc(Program* program)
{
    opcode_t opcode[5];
    int data[5];

    // NOTE: Original code does not use loop.
    for (int arg = 0; arg < 5; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }
    }

    if ((opcode[0] & 0xF7FF) != VALUE_TYPE_INT) {
        programFatalError("Invalid procedure 4 name given to addbuttonproc");
    }

    if ((opcode[1] & 0xF7FF) != VALUE_TYPE_INT) {
        programFatalError("Invalid procedure 3 name given to addbuttonproc");
    }

    if ((opcode[2] & 0xF7FF) != VALUE_TYPE_INT) {
        programFatalError("Invalid procedure 2 name given to addbuttonproc");
    }

    if ((opcode[3] & 0xF7FF) != VALUE_TYPE_INT) {
        programFatalError("Invalid procedure 1 name given to addbuttonproc");
    }

    if ((opcode[4] & 0xF7FF) != VALUE_TYPE_STRING) {
        programFatalError("Invalid name given to addbuttonproc");
    }

    const char* buttonName = programGetString(program, opcode[4], data[4]);
    _selectWindowID(program->field_84);

    if (!_windowAddButtonProc(buttonName, program, data[3], data[2], data[1], data[0])) {
        programFatalError("Error setting procedures to button %s\n", buttonName);
    }
}

// addbuttonrightproc
// 0x4642A8
static void opAddButtonRightProc(Program* program)
{
    opcode_t opcode[3];
    int data[3];

    // NOTE: Original code does not use loop.
    for (int arg = 0; arg < 3; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }
    }

    if ((opcode[0] & 0xF7FF) != VALUE_TYPE_INT) {
        programFatalError("Invalid procedure 2 name given to addbuttonrightproc");
    }

    if ((opcode[1] & 0xF7FF) != VALUE_TYPE_INT) {
        programFatalError("Invalid procedure 1 name given to addbuttonrightproc");
    }

    if ((opcode[2] & 0xF7FF) != VALUE_TYPE_STRING) {
        programFatalError("Invalid name given to addbuttonrightproc");
    }

    const char* regionName = programGetString(program, opcode[2], data[2]);
    _selectWindowID(program->field_84);

    if (!_windowAddRegionRightProc(regionName, program, data[1], data[0])) {
        programFatalError("Error setting right button procedures to button %s\n", regionName);
    }
}

// showwin
// 0x4643D4
static void opShowWin(Program* program)
{
    _selectWindowID(program->field_84);
    _windowDraw();
}

// deletebutton
// 0x4643E4
static void opDeleteButton(Program* program)
{
    opcode_t opcode = programStackPopInt16(program);
    int data = programStackPopInt32(program);

    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    if ((opcode & 0xF7FF) != VALUE_TYPE_STRING) {
        if ((opcode & 0xF7FF) == VALUE_TYPE_INT && data != -1) {
            programFatalError("Invalid type given to delete button");
        }
    }

    _selectWindowID(program->field_84);

    if ((opcode & 0xF7FF) == VALUE_TYPE_INT) {
        if (_windowDeleteButton(NULL)) {
            return;
        }
    } else {
        const char* buttonName = programGetString(program, opcode, data);
        if (_windowDeleteButton(buttonName)) {
            return;
        }
    }
    
    programFatalError("Error deleting button");
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

// setglobalmousefunc
// 0x4649C4
static void opSetGlobalMouseFunc(Program* Program)
{
    programFatalError("setglobalmousefunc not defined");
}

// loadpalettetable
// 0x464ADC
static void opLoadPaletteTable(Program* program)
{
    opcode_t opcode = programStackPopInt16(program);
    int data = programStackPopInt32(program);

    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    if ((opcode & 0xF7FF) != VALUE_TYPE_STRING) {
        if ((opcode & 0xF7FF) == VALUE_TYPE_INT && data != -1) {
            programFatalError("Invalid type given to loadpalettetable");
        }
    }

    char* path = programGetString(program, opcode, data);
    if (!colorPaletteLoad(path)) {
        programFatalError(_colorError());
    }
}

// addnamedevent
// 0x464B54
static void opAddNamedEvent(Program* program)
{
    opcode_t opcode[2];
    int data[2];

    // NOTE: Original code does not use loop.
    for (int arg = 0; arg < 2; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }
    }

    if ((opcode[1] & 0xF7FF) != VALUE_TYPE_STRING) {
        programFatalError("Invalid type given to addnamedevent");
    }

    const char* v1 = programGetString(program, opcode[1], data[1]);
    _nevs_addevent(v1, program, data[0], 0);
}

// addnamedhandler
// 0x464BE8
static void opAddNamedHandler(Program* program)
{
    opcode_t opcode[2];
    int data[2];

    // NOTE: Original code does not use loop.
    for (int arg = 0; arg < 2; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }
    }

    if ((opcode[1] & 0xF7FF) != VALUE_TYPE_STRING) {
        programFatalError("Invalid type given to addnamedhandler");
    }

    const char* v1 = programGetString(program, opcode[1], data[1]);
    _nevs_addevent(v1, program, data[0], 1);
}

// clearnamed
// 0x464C80
static void opClearNamed(Program* program)
{
    opcode_t opcode = programStackPopInt16(program);
    int data = programStackPopInt32(program);

    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    if ((opcode & 0xF7FF) != VALUE_TYPE_STRING) {
        programFatalError("Invalid type given to clearnamed");
    }

    char* string = programGetString(program, opcode, data);
    _nevs_clearevent(string);
}

// signalnamed
// 0x464CE4
static void opSignalNamed(Program* program)
{
    opcode_t opcode = programStackPopInt16(program);
    int data = programStackPopInt32(program);

    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    if ((opcode & 0xF7FF) != VALUE_TYPE_STRING) {
        programFatalError("Invalid type given to signalnamed");
    }

    char* str = programGetString(program, opcode, data);
    _nevs_signal(str);
}

// addkey
// 0x464D48
static void opAddKey(Program* program)
{
    opcode_t opcode[2];
    int data[2];

    // NOTE: Original code does not use loop.
    for (int arg = 0; arg < 2; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }
    }

    for (int arg = 0; arg < 2; arg++) {
        if ((opcode[arg] & 0xF7FF) != VALUE_TYPE_INT) {
            programFatalError("Invalid arg %d given to addkey", arg + 1);
        }
    }

    int key = data[1];
    int proc = data[0];

    if (key == -1) {
        gIntepreterAnyKeyHandlerProc = proc;
        gInterpreterAnyKeyHandlerProgram = program;
    } else {
        if (key > INTERPRETER_KEY_HANDLER_ENTRIES_LENGTH - 1) {
            programFatalError("Key out of range");
        }

        gInterpreterKeyHandlerEntries[key].program = program;
        gInterpreterKeyHandlerEntries[key].proc = proc;
    }
}

// deletekey
// 0x464E24
static void opDeleteKey(Program* program)
{
    opcode_t opcode = programStackPopInt16(program);
    int data = programStackPopInt32(program);

    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    if ((opcode & 0xF7FF) != VALUE_TYPE_INT) {
        programFatalError("Invalid arg 1 given to deletekey");
    }

    int key = data;

    if (key == -1) {
        gIntepreterAnyKeyHandlerProc = 0;
        gInterpreterAnyKeyHandlerProgram = NULL;
    } else {
        if (key > INTERPRETER_KEY_HANDLER_ENTRIES_LENGTH - 1) {
            programFatalError("Key out of range");
        }

        gInterpreterKeyHandlerEntries[key].program = NULL;
        gInterpreterKeyHandlerEntries[key].proc = 0;
    }
}

// setfont
// 0x464F18
static void opSetFont(Program* program)
{
    opcode_t opcode = programStackPopInt16(program);
    int data = programStackPopInt32(program);

    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    if ((opcode & 0xF7FF) != VALUE_TYPE_INT) {
        programFatalError("Invalid arg 1 given to setfont");
    }

    if (!widgetSetFont(data)) {
        programFatalError("Error setting font");
    }
}

// setflags
// 0x464F84
static void opSetTextFlags(Program* program)
{
    opcode_t opcode = programStackPopInt16(program);
    int data = programStackPopInt32(program);

    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    if ((opcode & 0xF7FF) != VALUE_TYPE_INT) {
        programFatalError("Invalid arg 1 given to setflags");
    }

    if (!widgetSetTextFlags(data)) {
        programFatalError("Error setting text flags");
    }
}

// settextcolor
// 0x464FF0
static void opSetTextColor(Program* program)
{
    opcode_t opcode[3];
    int data[3];
    float* floats = (float*)data;

    // NOTE: Original code does not use loops.
    for (int arg = 0; arg < 3; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }
    }

    for (int arg = 0; arg < 3; arg++) {
        if (((opcode[arg] & 0xF7FF) != VALUE_TYPE_FLOAT && (opcode[arg] & 0xF7FF) != VALUE_TYPE_INT)
            || floats[arg] == 0.0) {
            programFatalError("Invalid type given to settextcolor");
        }
    }

    float r = floats[2];
    float g = floats[1];
    float b = floats[0];

    if (!widgetSetTextColor(r, g, b)) {
        programFatalError("Error setting text color");
    }
}

// sayoptioncolor
// 0x465140
static void opSayOptionColor(Program* program)
{
    opcode_t opcode[3];
    int data[3];
    float* floats = (float*)data;

    // NOTE: Original code does not use loops.
    for (int arg = 0; arg < 3; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }
    }

    for (int arg = 0; arg < 3; arg++) {
        if (((opcode[arg] & 0xF7FF) != VALUE_TYPE_FLOAT && (opcode[arg] & 0xF7FF) != VALUE_TYPE_INT)
            || floats[arg] == 0.0) {
            programFatalError("Invalid type given to sayoptioncolor");
        }
    }

    float r = floats[2];
    float g = floats[1];
    float b = floats[0];

    if (dialogSetOptionColor(r, g, b)) {
        programFatalError("Error setting option color");
    }
}

// sayreplycolor
// 0x465290
static void opSayReplyColor(Program* program)
{
    opcode_t opcode[3];
    int data[3];
    float* floats = (float*)data;

    // NOTE: Original code does not use loops.
    for (int arg = 0; arg < 3; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);
        ;

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }
    }

    for (int arg = 0; arg < 3; arg++) {
        if (((opcode[arg] & 0xF7FF) != VALUE_TYPE_FLOAT && (opcode[arg] & 0xF7FF) != VALUE_TYPE_INT)
            || floats[arg] == 0.0) {
            programFatalError("Invalid type given to sayreplycolor");
        }
    }

    float r = floats[2];
    float g = floats[1];
    float b = floats[0];

    if (dialogSetReplyColor(r, g, b) != 0) {
        programFatalError("Error setting reply color");
    }
}

// sethighlightcolor
// 0x4653E0
static void opSetHighlightColor(Program* program)
{
    opcode_t opcode[3];
    int data[3];
    float* floats = (float*)data;

    // NOTE: Original code does not use loops.
    for (int arg = 0; arg < 3; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }
    }

    for (int arg = 0; arg < 3; arg++) {
        if (((opcode[arg] & 0xF7FF) != VALUE_TYPE_FLOAT && (opcode[arg] & 0xF7FF) != VALUE_TYPE_INT)
            || floats[arg] == 0.0) {
            programFatalError("Invalid type given to sethighlightcolor");
        }
    }

    float r = floats[2];
    float g = floats[1];
    float b = floats[0];

    if (!widgetSetHighlightColor(r, g, b)) {
        programFatalError("Error setting text highlight color");
    }
}

// sayreplyflags
// 0x465688
static void opSayReplyFlags(Program* program)
{
    opcode_t opcode = programStackPopInt16(program);
    int data = programStackPopInt32(program);

    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    if ((opcode & 0xF7FF) != VALUE_TYPE_INT) {
        programFatalError("Invalid arg 1 given to sayreplyflags");
    }

    if (!_dialogSetOptionFlags(data)) {
        programFatalError("Error setting reply flags");
    }
}

// sayoptionflags
// 0x4656F4
static void opSayOptionFlags(Program* program)
{
    opcode_t opcode = programStackPopInt16(program);
    int data = programStackPopInt32(program);

    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    if ((opcode & 0xF7FF) != VALUE_TYPE_INT) {
        programFatalError("Invalid arg 1 given to sayoptionflags");
    }

    if (!_dialogSetOptionFlags(data)) {
        programFatalError("Error setting option flags");
    }
}

// sayborder
// 0x4658B8
static void opSayBorder(Program* program)
{
    opcode_t opcode[2];
    int data[2];

    // NOTE: Original code does not use loops.
    for (int arg = 0; arg < 2; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }
    }

    for (int arg = 0; arg < 2; arg++) {
        if ((opcode[arg] & 0xF7FF) != VALUE_TYPE_INT) {
            programFatalError("Invalid arg %d given to sayborder", arg + 1);
        }
    }

    if (dialogSetBorder(data[1], data[0]) != 0) {
        programFatalError("Error setting dialog border");
    }
}

// saysetspacing
// 0x465FE0
static void opSaySetSpacing(Program* program)
{
    opcode_t opcode = programStackPopInt16(program);
    int data = programStackPopInt32(program);

    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    if ((opcode & 0xF7FF) != VALUE_TYPE_INT) {
        programFatalError("Invalid arg 1 given to saysetspacing");
    }

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
static void interpreterSoundCallback(void* userData, int a2)
{
    if (a2 == 1) {
        Sound** sound = (Sound**)userData;
        *sound = NULL;
    }
}

// 0x466070
static int interpreterSoundDelete(int value)
{
    if (value == -1) {
        return 1;
    }

    if ((value & 0xA0000000) == 0) {
        return 0;
    }

    int index = value & ~0xA0000000;
    Sound* sound = gInterpreterSounds[index];
    if (sound == NULL) {
        return 0;
    }

    if (soundIsPlaying(sound)) {
        soundStop(sound);
    }

    soundDelete(sound);

    gInterpreterSounds[index] = NULL;

    return 1;
}

// 0x466110
static int interpreterSoundPlay(char* fileName, int mode)
{
    int v3 = 1;
    int v5 = 0;

    if (mode & 0x01) {
        // looping
        v5 |= 0x20;
    } else {
        v3 = 5;
    }

    if (mode & 0x02) {
        v5 |= 0x08;
    } else {
        v5 |= 0x10;
    }

    if (mode & 0x0100) {
        // memory
        v3 &= ~0x03;
        v3 |= 0x01;
    }

    if (mode & 0x0200) {
        // streamed
        v3 &= ~0x03;
        v3 |= 0x02;
    }

    int index;
    for (index = 0; index < INTERPRETER_SOUNDS_LENGTH; index++) {
        if (gInterpreterSounds[index] == NULL) {
            break;
        }
    }

    if (index == INTERPRETER_SOUNDS_LENGTH) {
        return -1;
    }

    Sound* sound = gInterpreterSounds[index] = soundAllocate(v3, v5);
    if (sound == NULL) {
        return -1;
    }

    soundSetCallback(sound, interpreterSoundCallback, &(gInterpreterSounds[index]));

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
    gInterpreterSounds[index] = NULL;
    return -1;
}

// 0x46655C
static int interpreterSoundPause(int value)
{
    if (value == -1) {
        return 1;
    }

    if ((value & 0xA0000000) == 0) {
        return 0;
    }

    int index = value & ~0xA0000000;
    Sound* sound = gInterpreterSounds[index];
    if (sound == NULL) {
        return 0;
    }

    int rc;
    if (_soundType(sound, 0x01)) {
        rc = soundStop(sound);
    } else {
        rc = soundPause(sound);
    }
    return rc == SOUND_NO_ERROR;
}

// 0x4665C8
static int interpreterSoundRewind(int value)
{
    if (value == -1) {
        return 1;
    }

    if ((value & 0xA0000000) == 0) {
        return 0;
    }

    int index = value & ~0xA0000000;
    Sound* sound = gInterpreterSounds[index];
    if (sound == NULL) {
        return 0;
    }

    if (!soundIsPlaying(sound)) {
        return 1;
    }

    soundStop(sound);

    return soundPlay(sound) == SOUND_NO_ERROR;
}

// 0x46662C
static int interpreterSoundResume(int value)
{
    if (value == -1) {
        return 1;
    }

    if ((value & 0xA0000000) == 0) {
        return 0;
    }

    int index = value & ~0xA0000000;
    Sound* sound = gInterpreterSounds[index];
    if (sound == NULL) {
        return 0;
    }

    int rc;
    if (_soundType(sound, 0x01)) {
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
    // TODO: Incomplete.
}

// soundpause
// 0x466768
static void opSoundPause(Program* program)
{
    opcode_t opcode = programStackPopInt16(program);
    int data = programStackPopInt32(program);

    if (data == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    if ((opcode & 0xF7FF) != VALUE_TYPE_INT) {
        programFatalError("Invalid arg 1 given to soundpause");
    }

    interpreterSoundPause(data);
}

// soundresume
// 0x4667C0
static void opSoundResume(Program* program)
{
    opcode_t opcode = programStackPopInt16(program);
    int data = programStackPopInt32(program);

    if (data == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    if ((opcode & 0xF7FF) != VALUE_TYPE_INT) {
        programFatalError("Invalid arg 1 given to soundresume");
    }

    interpreterSoundResume(data);
}

// soundstop
// 0x466818
static void opSoundStop(Program* program)
{
    opcode_t opcode = programStackPopInt16(program);
    int data = programStackPopInt32(program);

    if (data == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    if ((opcode & 0xF7FF) != VALUE_TYPE_INT) {
        programFatalError("Invalid arg 1 given to soundstop");
    }

    interpreterSoundPause(data);
}

// soundrewind
// 0x466870
static void opSoundRewind(Program* program)
{
    opcode_t opcode = programStackPopInt16(program);
    int data = programStackPopInt32(program);

    if (data == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    if ((opcode & 0xF7FF) != VALUE_TYPE_INT) {
        programFatalError("Invalid arg 1 given to soundrewind");
    }

    interpreterSoundRewind(data);
}

// sounddelete
// 0x4668C8
static void opSoundDelete(Program* program)
{
    opcode_t opcode = programStackPopInt16(program);
    int data = programStackPopInt32(program);

    if (data == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    if ((opcode & 0xF7FF) != VALUE_TYPE_INT) {
        programFatalError("Invalid arg 1 given to sounddelete");
    }

    interpreterSoundDelete(data);
}

// SetOneOptPause
// 0x466920
static void opSetOneOptPause(Program* program)
{
    opcode_t opcode = programStackPopInt16(program);
    int data = programStackPopInt32(program);

    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    if ((opcode & 0xF7FF) != VALUE_TYPE_INT) {
        programFatalError("SetOneOptPause: invalid arg passed (non-integer).");
    }

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
void _updateIntLib()
{
    _nevs_update();
    _intExtraRemoveProgramReferences_();
}

// 0x4669A0
void _intlibClose()
{
    _dialogClose();
    _intExtraClose_();

    for (int index = 0; index < INTERPRETER_SOUNDS_LENGTH; index++) {
        if (gInterpreterSounds[index] != NULL) {
            interpreterSoundDelete(index | 0xA0000000);
        }
    }

    _nevs_close();

    if (_callbacks != NULL) {
        internal_free_safe(_callbacks, __FILE__, __LINE__); // "..\\int\\INTLIB.C", 1976
        _callbacks = NULL;
        _numCallbacks = 0;
    }
}

// 0x466A04
static bool _intLibDoInput(int key)
{
    if (key < 0 || key >= INTERPRETER_KEY_HANDLER_ENTRIES_LENGTH) {
        return false;
    }

    if (gInterpreterAnyKeyHandlerProgram != NULL) {
        if (gIntepreterAnyKeyHandlerProc != 0) {
            _executeProc(gInterpreterAnyKeyHandlerProgram, gIntepreterAnyKeyHandlerProc);
        }
        return true;
    }

    InterpreterKeyHandlerEntry* entry = &(gInterpreterKeyHandlerEntries[key]);
    if (entry->program == NULL) {
        return false;
    }

    if (entry->proc != 0) {
        _executeProc(entry->program, entry->proc);
    }

    return true;
}

// 0x466A70
void _initIntlib()
{
    // TODO: Incomplete.
    _nevs_initonce();
    _initIntExtra();
}

// 0x466F6C
void _interpretRegisterProgramDeleteCallback(OFF_59E160 fn)
{
    int index;
    for (index = 0; index < _numCallbacks; index++) {
        if (_callbacks[index] == NULL) {
            break;
        }
    }

    if (index == _numCallbacks) {
        if (_callbacks != NULL) {
            _callbacks = (OFF_59E160*)internal_realloc_safe(_callbacks, sizeof(*_callbacks) * (_numCallbacks + 1), __FILE__, __LINE__); // ..\\int\\INTLIB.C, 2110
        } else {
            _callbacks = (OFF_59E160*)internal_malloc_safe(sizeof(*_callbacks), __FILE__, __LINE__); // ..\\int\\INTLIB.C, 2112
        }
        _numCallbacks++;
    }

    _callbacks[index] = fn;
}

// 0x467040
void _removeProgramReferences_(Program* program)
{
    for (int index = 0; index < INTERPRETER_KEY_HANDLER_ENTRIES_LENGTH; index++) {
        if (program == gInterpreterKeyHandlerEntries[index].program) {
            gInterpreterKeyHandlerEntries[index].program = NULL;
        }
    }

    _intExtraRemoveProgramReferences_();

    for (int index = 0; index < _numCallbacks; index++) {
        OFF_59E160 fn = _callbacks[index];
        if (fn != NULL) {
            fn(program);
        }
    }
}
