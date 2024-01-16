#include "interpreter.h"

#include <assert.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "db.h"
#include "debug.h"
#include "export.h"
#include "input.h"
#include "interpreter_lib.h"
#include "memory_manager.h"
#include "platform_compat.h"
#include "sfall_global_scripts.h"
#include "svga.h"

namespace fallout {

typedef struct ProgramListNode {
    Program* program;
    struct ProgramListNode* next; // next
    struct ProgramListNode* prev; // prev
} ProgramListNode;

static unsigned int _defaultTimerFunc();
static char* _defaultFilename_(char* s);
static int _outputStr(char* a1);
static int _checkWait(Program* program);
static char* programGetCurrentProcedureName(Program* s);
static opcode_t stackReadInt16(unsigned char* data, int pos);
static int stackReadInt32(unsigned char* data, int pos);
static void stackWriteInt16(int value, unsigned char* data, int pos);
static void stackWriteInt32(int value, unsigned char* data, int pos);
static void stackPushInt16(unsigned char* data, int* pointer, int value);
static void stackPushInt32(unsigned char* data, int* pointer, int value);
static int stackPopInt32(unsigned char* data, int* pointer);
static opcode_t stackPopInt16(unsigned char* data, int* pointer);
static void _interpretIncStringRef(Program* program, opcode_t opcode, int value);
static void programReturnStackPushInt16(Program* program, int value);
static opcode_t programReturnStackPopInt16(Program* program);
static int programReturnStackPopInt32(Program* program);
static void _detachProgram(Program* program);
static void _purgeProgram(Program* program);
static opcode_t _getOp(Program* program);
static void programMarkHeap(Program* program);
static void opNoop(Program* program);
static void opPush(Program* program);
static void opPushBase(Program* program);
static void opPopBase(Program* program);
static void opPopToBase(Program* program);
static void opSetGlobal(Program* program);
static void opDump(Program* program);
static void opDelayedCall(Program* program);
static void opConditionalCall(Program* program);
static void opWait(Program* program);
static void opCancel(Program* program);
static void opCancelAll(Program* program);
static void opIf(Program* program);
static void opWhile(Program* program);
static void opStore(Program* program);
static void opFetch(Program* program);
static void opConditionalOperatorNotEqual(Program* program);
static void opConditionalOperatorEqual(Program* program);
static void opConditionalOperatorLessThanEquals(Program* program);
static void opConditionalOperatorGreaterThanEquals(Program* program);
static void opConditionalOperatorLessThan(Program* program);
static void opConditionalOperatorGreaterThan(Program* program);
static void opAdd(Program* program);
static void opSubtract(Program* program);
static void opMultiply(Program* program);
static void opDivide(Program* program);
static void opModulo(Program* program);
static void opLogicalOperatorAnd(Program* program);
static void opLogicalOperatorOr(Program* program);
static void opLogicalOperatorNot(Program* program);
static void opUnaryMinus(Program* program);
static void opBitwiseOperatorNot(Program* program);
static void opFloor(Program* program);
static void opBitwiseOperatorAnd(Program* program);
static void opBitwiseOperatorOr(Program* program);
static void opBitwiseOperatorXor(Program* program);
static void opSwapReturnStack(Program* program);
static void opLeaveCriticalSection(Program* program);
static void opEnterCriticalSection(Program* program);
static void opJump(Program* program);
static void opCall(Program* program);
static void opPopFlags(Program* program);
static void opPopReturn(Program* program);
static void opPopExit(Program* program);
static void opPopFlagsReturn(Program* program);
static void opPopFlagsExit(Program* program);
static void opPopFlagsReturnValExit(Program* program);
static void opPopFlagsReturnValExitExtern(Program* program);
static void opPopFlagsReturnExtern(Program* program);
static void opPopFlagsExitExtern(Program* program);
static void opPopFlagsReturnValExtern(Program* program);
static void opPopAddress(Program* program);
static void opAtoD(Program* program);
static void opDtoA(Program* program);
static void opExitProgram(Program* program);
static void opStopProgram(Program* program);
static void opFetchGlobalVariable(Program* program);
static void opStoreGlobalVariable(Program* program);
static void opSwapStack(Program* program);
static void opFetchProcedureAddress(Program* program);
static void opPop(Program* program);
static void opDuplicate(Program* program);
static void opStoreExternalVariable(Program* program);
static void opFetchExternalVariable(Program* program);
static void opExportProcedure(Program* program);
static void opExportVariable(Program* program);
static void opExit(Program* program);
static void opDetach(Program* program);
static void opCallStart(Program* program);
static void opSpawn(Program* program);
static Program* forkProgram(Program* program);
static void opFork(Program* program);
static void opExec(Program* program);
static void opCheckProcedureArgumentCount(Program* program);
static void opLookupStringProc(Program* program);
static void _setupCallWithReturnVal(Program* program, int address, int a3);
static void _setupCall(Program* program, int address, int returnAddress);
static void _setupExternalCallWithReturnVal(Program* program1, Program* program2, int address, int a4);
static void _setupExternalCall(Program* program1, Program* program2, int address, int a4);
static void _doEvents();
static void programListNodeFree(ProgramListNode* programListNode);
static void interpreterPrintStats();

// 0x50942C
static char _aCouldnTFindPro[] = "<couldn't find proc>";

// sayTimeoutMsg
// 0x519038
int _TimeOut = 0;

// 0x51903C
static int _Enabled = 1;

// 0x519040
InterpretTimerFunc* _timerFunc = _defaultTimerFunc;

// 0x519044
static unsigned int _timerTick = 1000;

// 0x519048
static char* (*_filenameFunc)(char*) = _defaultFilename_;

// 0x51904C
static int (*_outputFunc)(char*) = _outputStr;

// 0x519050
static int _cpuBurstSize = 10;

// 0x59E230
static OpcodeHandler* gInterpreterOpcodeHandlers[OPCODE_MAX_COUNT];

// 0x59E78C
static Program* gInterpreterCurrentProgram;

// 0x59E790
static ProgramListNode* gInterpreterProgramListHead;

// 0x59E794
static int _suspendEvents;

// 0x59E798
static int _busy;

// 0x4670A0
static unsigned int _defaultTimerFunc()
{
    return getTicks();
}

// 0x4670B4
static char* _defaultFilename_(char* s)
{
    return s;
}

// 0x4670B8
char* _interpretMangleName(char* s)
{
    return _filenameFunc(s);
}

// 0x4670C0
static int _outputStr(char* a1)
{
    return 1;
}

// 0x4670C8
static int _checkWait(Program* program)
{
    return 1000 * _timerFunc() / _timerTick <= program->waitEnd;
}

// 0x4670FC
void _interpretOutputFunc(int (*func)(char*))
{
    _outputFunc = func;
}

// 0x467104
int _interpretOutput(const char* format, ...)
{
    if (_outputFunc == nullptr) {
        return 0;
    }

    char string[260];

    va_list args;
    va_start(args, format);
    int rc = vsnprintf(string, sizeof(string), format, args);
    va_end(args);

    debugPrint(string);

    return rc;
}

// 0x467160
static char* programGetCurrentProcedureName(Program* program)
{
    int procedureCount = stackReadInt32(program->procedures, 0);
    unsigned char* ptr = program->procedures + 4;

    int procedureOffset = stackReadInt32(ptr, offsetof(Procedure, bodyOffset));
    int identifierOffset = stackReadInt32(ptr, offsetof(Procedure, nameOffset));

    for (int index = 0; index < procedureCount; index++) {
        int nextProcedureOffset = stackReadInt32(ptr + 24, offsetof(Procedure, bodyOffset));
        if (program->instructionPointer >= procedureOffset && program->instructionPointer < nextProcedureOffset) {
            return (char*)(program->identifiers + identifierOffset);
        }

        ptr += 24;
        identifierOffset = stackReadInt32(ptr, offsetof(Procedure, nameOffset));
    }

    return _aCouldnTFindPro;
}

// 0x4671F0
[[noreturn]] void programFatalError(const char* format, ...)
{
    char string[260];

    va_list argptr;
    va_start(argptr, format);
    vsnprintf(string, sizeof(string), format, argptr);
    va_end(argptr);

    debugPrint("\nError during execution: %s\n", string);

    if (gInterpreterCurrentProgram == nullptr) {
        debugPrint("No current script");
    } else {
        char* procedureName = programGetCurrentProcedureName(gInterpreterCurrentProgram);
        debugPrint("Current script: %s, procedure %s", gInterpreterCurrentProgram->name, procedureName);
    }

    if (gInterpreterCurrentProgram) {
        longjmp(gInterpreterCurrentProgram->env, 1);
    }
}

// 0x467290
static opcode_t stackReadInt16(unsigned char* data, int pos)
{
    // TODO: The return result is probably short.
    opcode_t value = 0;
    value |= data[pos++] << 8;
    value |= data[pos++];
    return value;
}

// 0x4672A4
static int stackReadInt32(unsigned char* data, int pos)
{
    int value = 0;
    value |= data[pos++] << 24;
    value |= data[pos++] << 16;
    value |= data[pos++] << 8;
    value |= data[pos++] & 0xFF;

    return value;
}

// 0x4672D4
static void stackWriteInt16(int value, unsigned char* stack, int pos)
{
    stack[pos++] = (value >> 8) & 0xFF;
    stack[pos] = value & 0xFF;
}

// NOTE: Inlined.
//
// 0x4672E8
static void stackWriteInt32(int value, unsigned char* stack, int pos)
{
    stack[pos++] = (value >> 24) & 0xFF;
    stack[pos++] = (value >> 16) & 0xFF;
    stack[pos++] = (value >> 8) & 0xFF;
    stack[pos] = value & 0xFF;
}

// pushShortStack
// 0x467324
static void stackPushInt16(unsigned char* data, int* pointer, int value)
{
    if (*pointer + 2 >= 0x1000) {
        programFatalError("pushShortStack: Stack overflow.");
    }

    stackWriteInt16(value, data, *pointer);

    *pointer += 2;
}

// pushLongStack
// 0x46736C
static void stackPushInt32(unsigned char* data, int* pointer, int value)
{
    int v1;

    if (*pointer + 4 >= 0x1000) {
        // FIXME: Should be pushLongStack.
        programFatalError("pushShortStack: Stack overflow.");
    }

    v1 = *pointer;
    stackWriteInt16(value >> 16, data, v1);
    stackWriteInt16(value & 0xFFFF, data, v1 + 2);
    *pointer = v1 + 4;
}

// popStackLong
// 0x4673C4
static int stackPopInt32(unsigned char* data, int* pointer)
{
    if (*pointer < 4) {
        programFatalError("\nStack underflow long.");
    }

    *pointer -= 4;

    return stackReadInt32(data, *pointer);
}

// popStackShort
// 0x4673F0
static opcode_t stackPopInt16(unsigned char* data, int* pointer)
{
    if (*pointer < 2) {
        programFatalError("\nStack underflow short.");
    }

    *pointer -= 2;

    // NOTE: uninline
    return stackReadInt16(data, *pointer);
}

// NOTE: Inlined.
//
// 0x467424
static void _interpretIncStringRef(Program* program, opcode_t opcode, int value)
{
    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        *(short*)(program->dynamicStrings + 4 + value - 2) += 1;
    }
}

// 0x467440
void _interpretDecStringRef(Program* program, opcode_t opcode, int value)
{
    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        char* string = (char*)(program->dynamicStrings + 4 + value);
        short* refcountPtr = (short*)(string - 2);

        if (*refcountPtr != 0) {
            *refcountPtr -= 1;
        } else {
            debugPrint("Reference count zero for %s!\n", string);
        }

        if (*refcountPtr < 0) {
            debugPrint("String ref went negative, this shouldn\'t ever happen\n");
        }
    }
}

// NOTE: Inlined.
//
// 0x4675C8
static void _detachProgram(Program* program)
{
    Program* parent = program->parent;
    if (parent != nullptr) {
        parent->flags &= ~PROGRAM_FLAG_0x20;
        parent->flags &= ~PROGRAM_FLAG_0x0100;
        if (program == parent->child) {
            parent->child = nullptr;
        }
    }
}

// 0x4675F4
static void _purgeProgram(Program* program)
{
    if (!program->exited) {
        intLibRemoveProgramReferences(program);
        program->exited = true;
    }
}

// 0x467614
void programFree(Program* program)
{
    // NOTE: Uninline.
    _detachProgram(program);

    Program* curr = program->child;
    while (curr != nullptr) {
        // NOTE: Uninline.
        _purgeProgram(curr);

        curr->parent = nullptr;

        Program* next = curr->child;
        curr->child = nullptr;

        curr = next;
    }

    // NOTE: Uninline.
    _purgeProgram(program);

    if (program->dynamicStrings != nullptr) {
        internal_free_safe(program->dynamicStrings, __FILE__, __LINE__); // "..\\int\\INTRPRET.C", 429
    }

    if (program->data != nullptr) {
        internal_free_safe(program->data, __FILE__, __LINE__); // "..\\int\\INTRPRET.C", 430
    }

    if (program->name != nullptr) {
        internal_free_safe(program->name, __FILE__, __LINE__); // "..\\int\\INTRPRET.C", 431
    }

    delete program->stackValues;
    delete program->returnStackValues;

    internal_free_safe(program, __FILE__, __LINE__); // "..\\int\\INTRPRET.C", 435
}

// 0x467734
Program* programCreateByPath(const char* path)
{
    File* stream = fileOpen(path, "rb");
    if (stream == nullptr) {
        char err[260];
        snprintf(err, sizeof(err), "Couldn't open %s for read\n", path);
        programFatalError(err);
        return nullptr;
    }

    int fileSize = fileGetSize(stream);
    unsigned char* data = (unsigned char*)internal_malloc_safe(fileSize, __FILE__, __LINE__); // ..\\int\\INTRPRET.C, 458

    fileRead(data, 1, fileSize, stream);
    fileClose(stream);

    Program* program = (Program*)internal_malloc_safe(sizeof(Program), __FILE__, __LINE__); // ..\\int\\INTRPRET.C, 463
    memset(program, 0, sizeof(Program));

    program->name = (char*)internal_malloc_safe(strlen(path) + 1, __FILE__, __LINE__); // ..\\int\\INTRPRET.C, 466
    strcpy(program->name, path);

    program->child = nullptr;
    program->parent = nullptr;
    program->field_78 = -1;
    program->exited = false;
    program->basePointer = -1;
    program->framePointer = -1;
    program->data = data;
    program->procedures = data + 42;
    program->identifiers = 24 * stackReadInt32(program->procedures, 0) + program->procedures + 4;
    program->staticStrings = program->identifiers + stackReadInt32(program->identifiers, 0) + 4;

    program->stackValues = new ProgramStack();
    program->returnStackValues = new ProgramStack();

    return program;
}

// NOTE: Inlined.
//
// 0x4678BC
opcode_t _getOp(Program* program)
{
    int instructionPointer;

    instructionPointer = program->instructionPointer;
    program->instructionPointer = instructionPointer + 2;

    // NOTE: Uninline.
    return stackReadInt16(program->data, instructionPointer);
}

// 0x4678E0
char* programGetString(Program* program, opcode_t opcode, int offset)
{
    // The order of checks is important, because dynamic string flag is
    // always used with static string flag.

    if ((opcode & RAW_VALUE_TYPE_DYNAMIC_STRING) != 0) {
        return (char*)(program->dynamicStrings + 4 + offset);
    }

    if ((opcode & RAW_VALUE_TYPE_STATIC_STRING) != 0) {
        return (char*)(program->staticStrings + 4 + offset);
    }

    return nullptr;
}

// 0x46790C
char* programGetIdentifier(Program* program, int offset)
{
    return (char*)(program->identifiers + offset);
}

// Loops thru heap:
// - mark unreferenced blocks as free.
// - merge consequtive free blocks as one large block.
//
// This is done by negating block length:
// - positive block length - check for ref count.
// - negative block length - block is free, attempt to merge with next block.
//
// 0x4679E0
static void programMarkHeap(Program* program)
{
    unsigned char* ptr;
    short len;
    unsigned char* next_ptr;
    short next_len;
    short diff;

    if (program->dynamicStrings == nullptr) {
        return;
    }

    ptr = program->dynamicStrings + 4;
    while (*(unsigned short*)ptr != 0x8000) {
        len = *(short*)ptr;
        if (len < 0) {
            len = -len;
            next_ptr = ptr + len + 4;

            if (*(unsigned short*)next_ptr != 0x8000) {
                next_len = *(short*)next_ptr;
                if (next_len < 0) {
                    diff = 4 - next_len;
                    if (diff + len < 32766) {
                        len += diff;
                        *(short*)ptr += next_len - 4;
                    } else {
                        debugPrint("merged string would be too long, size %d %d\n", diff, len);
                    }
                }
            }
        } else if (*(short*)(ptr + 2) == 0) {
            *(short*)ptr = -len;
            *(short*)(ptr + 2) = 0;
        }

        ptr += len + 4;
    }
}

// 0x467A80
int programPushString(Program* program, char* string)
{
    int v27;
    unsigned char* v20;
    unsigned char* v23;

    if (program == nullptr) {
        return 0;
    }

    v27 = strlen(string) + 1;

    // Align memory
    if (v27 & 1) {
        v27++;
    }

    if (program->dynamicStrings != nullptr) {
        // TODO: Needs testing, lots of pointer stuff.
        unsigned char* heap = program->dynamicStrings + 4;
        while (*(unsigned short*)heap != 0x8000) {
            short v2 = *(short*)heap;
            if (v2 >= 0) {
                if (v2 == v27) {
                    if (strcmp(string, (char*)(heap + 4)) == 0) {
                        return (heap + 4) - (program->dynamicStrings + 4);
                    }
                }
            } else {
                v2 = -v2;
                if (v2 > v27) {
                    if (v2 - v27 <= 4) {
                        *(short*)heap = v2;
                    } else {
                        *(short*)(heap + v27 + 6) = 0;
                        *(short*)(heap + v27 + 4) = -(v2 - v27 - 4);
                        *(short*)(heap) = v27;
                    }

                    *(short*)(heap + 2) = 0;
                    strcpy((char*)(heap + 4), string);

                    *(heap + v27 + 3) = '\0';
                    return (heap + 4) - (program->dynamicStrings + 4);
                }
            }
            heap += v2 + 4;
        }
    } else {
        program->dynamicStrings = (unsigned char*)internal_malloc_safe(8, __FILE__, __LINE__); // "..\\int\\INTRPRET.C", 631
        *(int*)(program->dynamicStrings) = 0;
        *(unsigned short*)(program->dynamicStrings + 4) = 0x8000;
        *(short*)(program->dynamicStrings + 6) = 1;
    }

    program->dynamicStrings = (unsigned char*)internal_realloc_safe(program->dynamicStrings, *(int*)(program->dynamicStrings) + 8 + 4 + v27, __FILE__, __LINE__); // "..\\int\\INTRPRET.C", 640

    v20 = program->dynamicStrings + *(int*)(program->dynamicStrings) + 4;
    if ((*(short*)v20 & 0xFFFF) != 0x8000) {
        programFatalError("Internal consistancy error, string table mangled");
    }

    *(int*)(program->dynamicStrings) += v27 + 4;

    *(short*)(v20) = v27;
    *(short*)(v20 + 2) = 0;

    strcpy((char*)(v20 + 4), string);

    v23 = v20 + v27;
    *(v23 + 3) = '\0';
    *(unsigned short*)(v23 + 4) = 0x8000;
    *(short*)(v23 + 6) = 1;

    return v20 + 4 - (program->dynamicStrings + 4);
}

// 0x467C90
static void opNoop(Program* program)
{
}

// 0x467C94
static void opPush(Program* program)
{
    int pos = program->instructionPointer;
    program->instructionPointer = pos + 4;

    int value = stackReadInt32(program->data, pos);

    ProgramValue result;
    result.opcode = (program->flags >> 16) & 0xFFFF;
    result.integerValue = value;
    programStackPushValue(program, result);
}

// - Pops value from stack, which is a number of arguments in the procedure.
// - Saves current frame pointer in return stack.
// - Sets frame pointer to the stack pointer minus number of arguments.
//
// 0x467CD0
static void opPushBase(Program* program)
{
    int argumentCount = programStackPopInteger(program);
    programReturnStackPushInteger(program, program->framePointer);
    program->framePointer = program->stackValues->size() - argumentCount;
}

// pop_base
// 0x467D3C
static void opPopBase(Program* program)
{
    int data = programReturnStackPopInteger(program);
    program->framePointer = data;
}

// 0x467D94
static void opPopToBase(Program* program)
{
    while (program->stackValues->size() != program->framePointer) {
        programStackPopValue(program);
    }
}

// 0x467DE0
static void opSetGlobal(Program* program)
{
    program->basePointer = program->stackValues->size();
}

// 0x467DEC
static void opDump(Program* program)
{
    int data = programStackPopInteger(program);

    // NOTE: Original code is slightly different - it goes backwards to -1.
    for (int index = 0; index < data; index++) {
        programStackPopValue(program);
    }
}

// 0x467EA4
static void opDelayedCall(Program* program)
{
    int data[2];

    for (int arg = 0; arg < 2; arg++) {
        data[arg] = programStackPopInteger(program);
    }

    unsigned char* procedure_ptr = program->procedures + 4 + 24 * data[0];

    int delay = 1000 * data[1];

    if (!_suspendEvents) {
        delay += 1000 * _timerFunc() / _timerTick;
    }

    int flags = stackReadInt32(procedure_ptr, offsetof(Procedure, flags));

    stackWriteInt32(delay, procedure_ptr, offsetof(Procedure, time));
    stackWriteInt32(flags | PROCEDURE_FLAG_TIMED, procedure_ptr, offsetof(Procedure, flags));
}

// 0x468034
static void opConditionalCall(Program* program)
{
    int data[2];

    for (int arg = 0; arg < 2; arg++) {
        data[arg] = programStackPopInteger(program);
    }

    unsigned char* procedure_ptr = program->procedures + 4 + 24 * data[0];
    int flags = stackReadInt32(procedure_ptr, offsetof(Procedure, flags));

    stackWriteInt32(flags | PROCEDURE_FLAG_CONDITIONAL, procedure_ptr, offsetof(Procedure, flags));
    stackWriteInt32(data[1], procedure_ptr, offsetof(Procedure, conditionOffset));
}

// 0x46817C
static void opWait(Program* program)
{
    int data = programStackPopInteger(program);

    program->waitStart = 1000 * _timerFunc() / _timerTick;
    program->waitEnd = program->waitStart + data;
    program->checkWaitFunc = _checkWait;
    program->flags |= PROGRAM_IS_WAITING;
}

// 0x468218
static void opCancel(Program* program)
{
    int data = programStackPopInteger(program);

    if (data >= stackReadInt32(program->procedures, 0)) {
        programFatalError("Invalid procedure offset given to cancel");
    }

    Procedure* proc = (Procedure*)(program->procedures + 4 + data * sizeof(*proc));
    proc->flags = 0;
    proc->time = 0;
    proc->conditionOffset = 0;
}

// 0x468330
static void opCancelAll(Program* program)
{
    int procedureCount = stackReadInt32(program->procedures, 0);

    for (int index = 0; index < procedureCount; index++) {
        // TODO: Original code uses different approach, check.
        Procedure* proc = (Procedure*)(program->procedures + 4 + index * sizeof(*proc));

        proc->flags = 0;
        proc->time = 0;
        proc->conditionOffset = 0;
    }
}

// 0x468400
static void opIf(Program* program)
{
    ProgramValue value = programStackPopValue(program);

    if (!value.isEmpty()) {
        programStackPopValue(program);
    } else {
        program->instructionPointer = programStackPopInteger(program);
    }
}

// 0x4684A4
static void opWhile(Program* program)
{
    ProgramValue value = programStackPopValue(program);

    if (value.isEmpty()) {
        program->instructionPointer = programStackPopInteger(program);
    }
}

// 0x468518
static void opStore(Program* program)
{
    int addr = programStackPopInteger(program);
    ProgramValue value = programStackPopValue(program);
    size_t pos = program->framePointer + addr;

    ProgramValue oldValue = program->stackValues->at(pos);

    if (oldValue.opcode == VALUE_TYPE_DYNAMIC_STRING) {
        _interpretDecStringRef(program, oldValue.opcode, oldValue.integerValue);
    }

    program->stackValues->at(pos) = value;

    if (value.opcode == VALUE_TYPE_DYNAMIC_STRING) {
        // NOTE: Uninline.
        _interpretIncStringRef(program, VALUE_TYPE_DYNAMIC_STRING, value.integerValue);
    }
}

// fetch
// 0x468678
static void opFetch(Program* program)
{
    int addr = programStackPopInteger(program);

    ProgramValue value = program->stackValues->at(program->framePointer + addr);
    programStackPushValue(program, value);
}

// 0x46873C
static void opConditionalOperatorNotEqual(Program* program)
{
    ProgramValue value[2];
    char stringBuffers[2][80];
    char* strings[2];
    int result;

    for (int arg = 0; arg < 2; arg++) {
        value[arg] = programStackPopValue(program);
    }

    switch (value[1].opcode) {
    case VALUE_TYPE_STRING:
    case VALUE_TYPE_DYNAMIC_STRING:
        strings[1] = programGetString(program, value[1].opcode, value[1].integerValue);

        switch (value[0].opcode) {
        case VALUE_TYPE_STRING:
        case VALUE_TYPE_DYNAMIC_STRING:
            strings[0] = programGetString(program, value[0].opcode, value[0].integerValue);
            break;
        case VALUE_TYPE_FLOAT:
            snprintf(stringBuffers[0], sizeof(stringBuffers[0]), "%.5f", value[0].floatValue);
            strings[0] = stringBuffers[0];
            break;
        case VALUE_TYPE_INT:
            snprintf(stringBuffers[0], sizeof(stringBuffers[0]), "%d", value[0].integerValue);
            strings[0] = stringBuffers[0];
            break;
        default:
            assert(false && "Should be unreachable");
        }

        result = strcmp(strings[1], strings[0]) != 0;
        break;
    case VALUE_TYPE_FLOAT:
        switch (value[0].opcode) {
        case VALUE_TYPE_STRING:
        case VALUE_TYPE_DYNAMIC_STRING:
            snprintf(stringBuffers[1], sizeof(stringBuffers[1]), "%.5f", value[1].floatValue);
            strings[1] = stringBuffers[1];
            strings[0] = programGetString(program, value[0].opcode, value[0].integerValue);
            result = strcmp(strings[1], strings[0]) != 0;
            break;
        case VALUE_TYPE_FLOAT:
            result = value[1].floatValue != value[0].floatValue;
            break;
        case VALUE_TYPE_INT:
            result = value[1].floatValue != (float)value[0].integerValue;
            break;
        default:
            assert(false && "Should be unreachable");
        }
        break;
    case VALUE_TYPE_INT:
        switch (value[0].opcode) {
        case VALUE_TYPE_STRING:
        case VALUE_TYPE_DYNAMIC_STRING:
            snprintf(stringBuffers[1], sizeof(stringBuffers[1]), "%d", value[1].integerValue);
            strings[1] = stringBuffers[1];
            strings[0] = programGetString(program, value[0].opcode, value[0].integerValue);
            result = strcmp(strings[1], strings[0]) != 0;
            break;
        case VALUE_TYPE_FLOAT:
            result = (float)value[1].integerValue != value[0].floatValue;
            break;
        case VALUE_TYPE_INT:
            result = value[1].integerValue != value[0].integerValue;
            break;
        case VALUE_TYPE_PTR:
            result = (uintptr_t)(value[1].integerValue) != (uintptr_t)(value[0].pointerValue);
            break;
        default:
            assert(false && "Should be unreachable");
        }
        break;
    case VALUE_TYPE_PTR:
        switch (value[0].opcode) {
        case VALUE_TYPE_INT:
            result = (uintptr_t)(value[1].pointerValue) != (uintptr_t)(value[0].integerValue);
            break;
        case VALUE_TYPE_PTR:
            result = value[1].pointerValue != value[0].pointerValue;
            break;
        default:
            assert(false && "Should be unreachable");
        }
        break;
    default:
        assert(false && "Should be unreachable");
    }

    programStackPushInteger(program, result);
}

// 0x468AA8
static void opConditionalOperatorEqual(Program* program)
{
    ProgramValue value[2];
    char stringBuffers[2][80];
    char* strings[2];
    int result;

    for (int arg = 0; arg < 2; arg++) {
        value[arg] = programStackPopValue(program);
    }

    switch (value[1].opcode) {
    case VALUE_TYPE_STRING:
    case VALUE_TYPE_DYNAMIC_STRING:
        strings[1] = programGetString(program, value[1].opcode, value[1].integerValue);

        switch (value[0].opcode) {
        case VALUE_TYPE_STRING:
        case VALUE_TYPE_DYNAMIC_STRING:
            strings[0] = programGetString(program, value[0].opcode, value[0].integerValue);
            break;
        case VALUE_TYPE_FLOAT:
            snprintf(stringBuffers[0], sizeof(stringBuffers[0]), "%.5f", value[0].floatValue);
            strings[0] = stringBuffers[0];
            break;
        case VALUE_TYPE_INT:
            snprintf(stringBuffers[0], sizeof(stringBuffers[0]), "%d", value[0].integerValue);
            strings[0] = stringBuffers[0];
            break;
        default:
            assert(false && "Should be unreachable");
        }

        result = strcmp(strings[1], strings[0]) == 0;
        break;
    case VALUE_TYPE_FLOAT:
        switch (value[0].opcode) {
        case VALUE_TYPE_STRING:
        case VALUE_TYPE_DYNAMIC_STRING:
            snprintf(stringBuffers[1], sizeof(stringBuffers[1]), "%.5f", value[1].floatValue);
            strings[1] = stringBuffers[1];
            strings[0] = programGetString(program, value[0].opcode, value[0].integerValue);
            result = strcmp(strings[1], strings[0]) == 0;
            break;
        case VALUE_TYPE_FLOAT:
            result = value[1].floatValue == value[0].floatValue;
            break;
        case VALUE_TYPE_INT:
            result = value[1].floatValue == (float)value[0].integerValue;
            break;
        default:
            assert(false && "Should be unreachable");
        }
        break;
    case VALUE_TYPE_INT:
        switch (value[0].opcode) {
        case VALUE_TYPE_STRING:
        case VALUE_TYPE_DYNAMIC_STRING:
            snprintf(stringBuffers[1], sizeof(stringBuffers[1]), "%d", value[1].integerValue);
            strings[1] = stringBuffers[1];
            strings[0] = programGetString(program, value[0].opcode, value[0].integerValue);
            result = strcmp(strings[1], strings[0]) == 0;
            break;
        case VALUE_TYPE_FLOAT:
            result = (float)value[1].integerValue == value[0].floatValue;
            break;
        case VALUE_TYPE_INT:
            result = value[1].integerValue == value[0].integerValue;
            break;
        case VALUE_TYPE_PTR:
            result = (uintptr_t)(value[1].integerValue) == (uintptr_t)(value[0].pointerValue);
            break;
        default:
            assert(false && "Should be unreachable");
        }
        break;
    case VALUE_TYPE_PTR:
        switch (value[0].opcode) {
        case VALUE_TYPE_INT:
            result = (uintptr_t)(value[1].pointerValue) == (uintptr_t)(value[0].integerValue);
            break;
        case VALUE_TYPE_PTR:
            result = value[1].pointerValue == value[0].pointerValue;
            break;
        default:
            assert(false && "Should be unreachable");
        }
        break;
    default:
        assert(false && "Should be unreachable");
    }

    programStackPushInteger(program, result);
}

// 0x468E14
static void opConditionalOperatorLessThanEquals(Program* program)
{
    ProgramValue value[2];
    char stringBuffers[2][80];
    char* strings[2];
    int result;

    for (int arg = 0; arg < 2; arg++) {
        value[arg] = programStackPopValue(program);
    }

    switch (value[1].opcode) {
    case VALUE_TYPE_STRING:
    case VALUE_TYPE_DYNAMIC_STRING:
        strings[1] = programGetString(program, value[1].opcode, value[1].integerValue);

        switch (value[0].opcode) {
        case VALUE_TYPE_STRING:
        case VALUE_TYPE_DYNAMIC_STRING:
            strings[0] = programGetString(program, value[0].opcode, value[0].integerValue);
            break;
        case VALUE_TYPE_FLOAT:
            snprintf(stringBuffers[0], sizeof(stringBuffers[0]), "%.5f", value[0].floatValue);
            strings[0] = stringBuffers[0];
            break;
        case VALUE_TYPE_INT:
            snprintf(stringBuffers[0], sizeof(stringBuffers[0]), "%d", value[0].integerValue);
            strings[0] = stringBuffers[0];
            break;
        default:
            assert(false && "Should be unreachable");
        }

        result = strcmp(strings[1], strings[0]) <= 0;
        break;
    case VALUE_TYPE_FLOAT:
        switch (value[0].opcode) {
        case VALUE_TYPE_STRING:
        case VALUE_TYPE_DYNAMIC_STRING:
            snprintf(stringBuffers[1], sizeof(stringBuffers[1]), "%.5f", value[1].floatValue);
            strings[1] = stringBuffers[1];
            strings[0] = programGetString(program, value[0].opcode, value[0].integerValue);
            result = strcmp(strings[1], strings[0]) <= 0;
            break;
        case VALUE_TYPE_FLOAT:
            result = value[1].floatValue <= value[0].floatValue;
            break;
        case VALUE_TYPE_INT:
            result = value[1].floatValue <= (float)value[0].integerValue;
            break;
        default:
            assert(false && "Should be unreachable");
        }
        break;
    case VALUE_TYPE_INT:
        switch (value[0].opcode) {
        case VALUE_TYPE_STRING:
        case VALUE_TYPE_DYNAMIC_STRING:
            snprintf(stringBuffers[1], sizeof(stringBuffers[1]), "%d", value[1].integerValue);
            strings[1] = stringBuffers[1];
            strings[0] = programGetString(program, value[0].opcode, value[0].integerValue);
            result = strcmp(strings[1], strings[0]) <= 0;
            break;
        case VALUE_TYPE_FLOAT:
            result = (float)value[1].integerValue <= value[0].floatValue;
            break;
        case VALUE_TYPE_INT:
            result = value[1].integerValue <= value[0].integerValue;
            break;
        default:
            assert(false && "Should be unreachable");
        }
        break;
    // Nevada folks tend to use "object <= 0" to test objects for nulls.
    case VALUE_TYPE_PTR:
        switch (value[0].opcode) {
        case VALUE_TYPE_INT:
            if (value[0].integerValue > 0) {
                result = (uintptr_t)value[1].pointerValue <= (uintptr_t)value[0].integerValue;
            } else {
                // (ptr <= int{0 or negative}) means (ptr == nullptr)
                result = nullptr == value[1].pointerValue;
            }
            break;
        default:
            assert(false && "Should be unreachable");
        }
        break;
    default:
        assert(false && "Should be unreachable");
    }

    programStackPushInteger(program, result);
}

// 0x469180
static void opConditionalOperatorGreaterThanEquals(Program* program)
{
    ProgramValue value[2];
    char stringBuffers[2][80];
    char* strings[2];
    int result;

    // NOTE: original code does not use loop
    for (int arg = 0; arg < 2; arg++) {
        value[arg] = programStackPopValue(program);
    }

    switch (value[1].opcode) {
    case VALUE_TYPE_STRING:
    case VALUE_TYPE_DYNAMIC_STRING:
        strings[1] = programGetString(program, value[1].opcode, value[1].integerValue);

        switch (value[0].opcode) {
        case VALUE_TYPE_STRING:
        case VALUE_TYPE_DYNAMIC_STRING:
            strings[0] = programGetString(program, value[0].opcode, value[0].integerValue);
            break;
        case VALUE_TYPE_FLOAT:
            snprintf(stringBuffers[0], sizeof(stringBuffers[0]), "%.5f", value[0].floatValue);
            strings[0] = stringBuffers[0];
            break;
        case VALUE_TYPE_INT:
            snprintf(stringBuffers[0], sizeof(stringBuffers[0]), "%d", value[0].integerValue);
            strings[0] = stringBuffers[0];
            break;
        default:
            assert(false && "Should be unreachable");
        }

        result = strcmp(strings[1], strings[0]) >= 0;
        break;
    case VALUE_TYPE_FLOAT:
        switch (value[0].opcode) {
        case VALUE_TYPE_STRING:
        case VALUE_TYPE_DYNAMIC_STRING:
            snprintf(stringBuffers[1], sizeof(stringBuffers[1]), "%.5f", value[1].floatValue);
            strings[1] = stringBuffers[1];
            strings[0] = programGetString(program, value[0].opcode, value[0].integerValue);
            result = strcmp(strings[1], strings[0]) >= 0;
            break;
        case VALUE_TYPE_FLOAT:
            result = value[1].floatValue >= value[0].floatValue;
            break;
        case VALUE_TYPE_INT:
            result = value[1].floatValue >= (float)value[0].integerValue;
            break;
        default:
            assert(false && "Should be unreachable");
        }
        break;
    case VALUE_TYPE_INT:
        switch (value[0].opcode) {
        case VALUE_TYPE_STRING:
        case VALUE_TYPE_DYNAMIC_STRING:
            snprintf(stringBuffers[1], sizeof(stringBuffers[1]), "%d", value[1].integerValue);
            strings[1] = stringBuffers[1];
            strings[0] = programGetString(program, value[0].opcode, value[0].integerValue);
            result = strcmp(strings[1], strings[0]) >= 0;
            break;
        case VALUE_TYPE_FLOAT:
            result = (float)value[1].integerValue >= value[0].floatValue;
            break;
        case VALUE_TYPE_INT:
            result = value[1].integerValue >= value[0].integerValue;
            break;
        default:
            assert(false && "Should be unreachable");
        }
        break;
    default:
        assert(false && "Should be unreachable");
    }

    programStackPushInteger(program, result);
}

// 0x4694EC
static void opConditionalOperatorLessThan(Program* program)
{
    ProgramValue value[2];
    char text[2][80];
    char* str_ptr[2];
    int result;

    for (int arg = 0; arg < 2; arg++) {
        value[arg] = programStackPopValue(program);
    }

    switch (value[1].opcode) {
    case VALUE_TYPE_STRING:
    case VALUE_TYPE_DYNAMIC_STRING:
        str_ptr[1] = programGetString(program, value[1].opcode, value[1].integerValue);

        switch (value[0].opcode) {
        case VALUE_TYPE_STRING:
        case VALUE_TYPE_DYNAMIC_STRING:
            str_ptr[0] = programGetString(program, value[0].opcode, value[0].integerValue);
            break;
        case VALUE_TYPE_FLOAT:
            snprintf(text[0], sizeof(text[0]), "%.5f", value[0].floatValue);
            str_ptr[0] = text[0];
            break;
        case VALUE_TYPE_INT:
            snprintf(text[0], sizeof(text[0]), "%d", value[0].integerValue);
            str_ptr[0] = text[0];
            break;
        default:
            assert(false && "Should be unreachable");
        }

        result = strcmp(str_ptr[1], str_ptr[0]) < 0;
        break;
    case VALUE_TYPE_FLOAT:
        switch (value[0].opcode) {
        case VALUE_TYPE_STRING:
        case VALUE_TYPE_DYNAMIC_STRING:
            snprintf(text[1], sizeof(text[1]), "%.5f", value[1].floatValue);
            str_ptr[1] = text[1];
            str_ptr[0] = programGetString(program, value[0].opcode, value[0].integerValue);
            result = strcmp(str_ptr[1], str_ptr[0]) < 0;
            break;
        case VALUE_TYPE_FLOAT:
            result = value[1].floatValue < value[0].floatValue;
            break;
        case VALUE_TYPE_INT:
            result = value[1].floatValue < (float)value[0].integerValue;
            break;
        default:
            assert(false && "Should be unreachable");
        }
        break;
    case VALUE_TYPE_INT:
        switch (value[0].opcode) {
        case VALUE_TYPE_STRING:
        case VALUE_TYPE_DYNAMIC_STRING:
            snprintf(text[1], sizeof(text[1]), "%d", value[1].integerValue);
            str_ptr[1] = text[1];
            str_ptr[0] = programGetString(program, value[0].opcode, value[0].integerValue);
            result = strcmp(str_ptr[1], str_ptr[0]) < 0;
            break;
        case VALUE_TYPE_FLOAT:
            result = (float)value[1].integerValue < value[0].floatValue;
            break;
        case VALUE_TYPE_INT:
            result = value[1].integerValue < value[0].integerValue;
            break;
        default:
            assert(false && "Should be unreachable");
        }
        break;
    default:
        assert(false && "Should be unreachable");
    }

    programStackPushInteger(program, result);
}

// 0x469858
static void opConditionalOperatorGreaterThan(Program* program)
{
    ProgramValue value[2];
    char stringBuffers[2][80];
    char* strings[2];
    int result;

    for (int arg = 0; arg < 2; arg++) {
        value[arg] = programStackPopValue(program);
    }

    switch (value[1].opcode) {
    case VALUE_TYPE_STRING:
    case VALUE_TYPE_DYNAMIC_STRING:
        strings[1] = programGetString(program, value[1].opcode, value[1].integerValue);

        switch (value[0].opcode) {
        case VALUE_TYPE_STRING:
        case VALUE_TYPE_DYNAMIC_STRING:
            strings[0] = programGetString(program, value[0].opcode, value[0].integerValue);
            break;
        case VALUE_TYPE_FLOAT:
            snprintf(stringBuffers[0], sizeof(stringBuffers[0]), "%.5f", value[0].floatValue);
            strings[0] = stringBuffers[0];
            break;
        case VALUE_TYPE_INT:
            snprintf(stringBuffers[0], sizeof(stringBuffers[0]), "%d", value[0].integerValue);
            strings[0] = stringBuffers[0];
            break;
        default:
            assert(false && "Should be unreachable");
        }

        result = strcmp(strings[1], strings[0]) > 0;
        break;
    case VALUE_TYPE_FLOAT:
        switch (value[0].opcode) {
        case VALUE_TYPE_STRING:
        case VALUE_TYPE_DYNAMIC_STRING:
            snprintf(stringBuffers[1], sizeof(stringBuffers[1]), "%.5f", value[1].floatValue);
            strings[1] = stringBuffers[1];
            strings[0] = programGetString(program, value[0].opcode, value[0].integerValue);
            result = strcmp(strings[1], strings[0]) > 0;
            break;
        case VALUE_TYPE_FLOAT:
            result = value[1].floatValue > value[0].floatValue;
            break;
        case VALUE_TYPE_INT:
            result = value[1].floatValue > (float)value[0].integerValue;
            break;
        default:
            assert(false && "Should be unreachable");
        }
        break;
    case VALUE_TYPE_INT:
        switch (value[0].opcode) {
        case VALUE_TYPE_STRING:
        case VALUE_TYPE_DYNAMIC_STRING:
            snprintf(stringBuffers[1], sizeof(stringBuffers[1]), "%d", value[1].integerValue);
            strings[1] = stringBuffers[1];
            strings[0] = programGetString(program, value[0].opcode, value[0].integerValue);
            result = strcmp(strings[1], strings[0]) > 0;
            break;
        case VALUE_TYPE_FLOAT:
            result = (float)value[1].integerValue > value[0].floatValue;
            break;
        case VALUE_TYPE_INT:
            result = value[1].integerValue > value[0].integerValue;
            break;
        default:
            assert(false && "Should be unreachable");
        }
        break;
    // Sonora folks tend to use "object > 0" to test objects for nulls.
    case VALUE_TYPE_PTR:
        switch (value[0].opcode) {
        case VALUE_TYPE_INT:
            if (value[0].integerValue > 0) {
                result = (uintptr_t)value[1].pointerValue > (uintptr_t)value[0].integerValue;
            } else {
                // (ptr > int{0 or negative}) means (ptr != nullptr)
                result = nullptr != value[1].pointerValue;
            }
            break;
        default:
            assert(false && "Should be unreachable");
        }
        break;
    default:
        assert(false && "Should be unreachable");
    }

    programStackPushInteger(program, result);
}

// 0x469BC4
static void opAdd(Program* program)
{
    ProgramValue value[2];
    char* strings[2];
    char* tempString;

    for (int arg = 0; arg < 2; arg++) {
        value[arg] = programStackPopValue(program);
    }

    switch (value[1].opcode) {
    case VALUE_TYPE_STRING:
    case VALUE_TYPE_DYNAMIC_STRING:
        strings[1] = programGetString(program, value[1].opcode, value[1].integerValue);

        switch (value[0].opcode) {
        case VALUE_TYPE_STRING:
        case VALUE_TYPE_DYNAMIC_STRING:
            tempString = programGetString(program, value[0].opcode, value[0].integerValue);
            strings[0] = (char*)internal_malloc_safe(strlen(tempString) + 1, __FILE__, __LINE__); // "..\\int\\INTRPRET.C", 1002
            strcpy(strings[0], tempString);
            break;
        case VALUE_TYPE_FLOAT:
            strings[0] = (char*)internal_malloc_safe(80, __FILE__, __LINE__); // "..\\int\\INTRPRET.C", 1011
            snprintf(strings[0], 80, "%.5f", value[0].floatValue);
            break;
        case VALUE_TYPE_INT:
            strings[0] = (char*)internal_malloc_safe(80, __FILE__, __LINE__); // "..\\int\\INTRPRET.C", 1007
            snprintf(strings[0], 80, "%d", value[0].integerValue);
            break;
        case VALUE_TYPE_PTR:
            strings[0] = (char*)internal_malloc_safe(80, __FILE__, __LINE__);
            snprintf(strings[0], 80, "%p", value[0].pointerValue);
            break;
        }

        tempString = (char*)internal_malloc_safe(strlen(strings[1]) + strlen(strings[0]) + 1, __FILE__, __LINE__); // "..\\int\\INTRPRET.C", 1015
        strcpy(tempString, strings[1]);
        strcat(tempString, strings[0]);

        programStackPushString(program, tempString);

        internal_free_safe(strings[0], __FILE__, __LINE__); // "..\\int\\INTRPRET.C", 1019
        internal_free_safe(tempString, __FILE__, __LINE__); // "..\\int\\INTRPRET.C", 1020
        break;
    case VALUE_TYPE_FLOAT:
        switch (value[0].opcode) {
        case VALUE_TYPE_STRING:
        case VALUE_TYPE_DYNAMIC_STRING:
            strings[0] = programGetString(program, value[0].opcode, value[0].integerValue);
            tempString = (char*)internal_malloc_safe(strlen(strings[0]) + 80, __FILE__, __LINE__); // "..\\int\\INTRPRET.C", 1039
            snprintf(tempString, strlen(strings[0]) + 80, "%.5f", value[1].floatValue);
            strcat(tempString, strings[0]);

            programStackPushString(program, tempString);

            internal_free_safe(tempString, __FILE__, __LINE__); // "..\\int\\INTRPRET.C", 1044
            break;
        case VALUE_TYPE_FLOAT:
            programStackPushFloat(program, value[1].floatValue + value[0].floatValue);
            break;
        case VALUE_TYPE_INT:
            programStackPushFloat(program, value[1].floatValue + (float)value[0].integerValue);
            break;
        }
        break;
    case VALUE_TYPE_INT:
        switch (value[0].opcode) {
        case VALUE_TYPE_STRING:
        case VALUE_TYPE_DYNAMIC_STRING:
            strings[0] = programGetString(program, value[0].opcode, value[0].integerValue);
            tempString = (char*)internal_malloc_safe(strlen(strings[0]) + 80, __FILE__, __LINE__); // "..\\int\\INTRPRET.C", 1070
            snprintf(tempString, strlen(strings[0]) + 80, "%d", value[1].integerValue);
            strcat(tempString, strings[0]);

            programStackPushString(program, tempString);

            internal_free_safe(tempString, __FILE__, __LINE__); // "..\\int\\INTRPRET.C", 1075
            break;
        case VALUE_TYPE_FLOAT:
            programStackPushFloat(program, (float)value[1].integerValue + value[0].floatValue);
            break;
        case VALUE_TYPE_INT:
            if ((value[0].integerValue <= 0 || (INT_MAX - value[0].integerValue) > value[1].integerValue)
                && (value[0].integerValue >= 0 || (INT_MIN - value[0].integerValue) <= value[1].integerValue)) {
                programStackPushInteger(program, value[1].integerValue + value[0].integerValue);
            } else {
                programStackPushFloat(program, (float)value[1].integerValue + (float)value[0].integerValue);
            }
            break;
        }
        break;
    // Sonora folks use "object + string" concatenation for debug purposes.
    case VALUE_TYPE_PTR:
        switch (value[0].opcode) {
        case VALUE_TYPE_STRING:
        case VALUE_TYPE_DYNAMIC_STRING:
            strings[0] = programGetString(program, value[0].opcode, value[0].integerValue);
            tempString = (char*)internal_malloc_safe(strlen(strings[0]) + 80, __FILE__, __LINE__);
            snprintf(tempString, strlen(strings[0]) + 80, "%p", value[1].pointerValue);
            strcat(tempString, strings[0]);

            programStackPushString(program, tempString);

            internal_free_safe(tempString, __FILE__, __LINE__);
            break;
        }
    }
}

// 0x46A1D8
static void opSubtract(Program* program)
{
    ProgramValue value[2];

    for (int arg = 0; arg < 2; arg++) {
        value[arg] = programStackPopValue(program);
    }

    switch (value[1].opcode) {
    case VALUE_TYPE_FLOAT:
        switch (value[0].opcode) {
        case VALUE_TYPE_FLOAT:
            programStackPushFloat(program, value[1].floatValue - value[0].floatValue);
            break;
        default:
            programStackPushFloat(program, value[1].floatValue - (float)value[0].integerValue);
            break;
        }
        break;
    case VALUE_TYPE_INT:
        switch (value[0].opcode) {
        case VALUE_TYPE_FLOAT:
            programStackPushFloat(program, value[1].integerValue - value[0].floatValue);
            break;
        default:
            programStackPushInteger(program, value[1].integerValue - value[0].integerValue);
            break;
        }
        break;
    }
}

// 0x46A300
static void opMultiply(Program* program)
{
    ProgramValue value[2];

    for (int arg = 0; arg < 2; arg++) {
        value[arg] = programStackPopValue(program);
    }

    switch (value[1].opcode) {
    case VALUE_TYPE_FLOAT:
        switch (value[0].opcode) {
        case VALUE_TYPE_FLOAT:
            programStackPushFloat(program, value[1].floatValue * value[0].floatValue);
            break;
        default:
            programStackPushFloat(program, value[1].floatValue * value[0].integerValue);
            break;
        }
        break;
    case VALUE_TYPE_INT:
        switch (value[0].opcode) {
        case VALUE_TYPE_FLOAT:
            programStackPushFloat(program, value[1].integerValue * value[0].floatValue);
            break;
        default:
            programStackPushInteger(program, value[0].integerValue * value[1].integerValue);
            break;
        }
        break;
    }
}

// 0x46A424
static void opDivide(Program* program)
{
    ProgramValue value[2];
    float divisor;

    for (int arg = 0; arg < 2; arg++) {
        value[arg] = programStackPopValue(program);
    }

    switch (value[1].opcode) {
    case VALUE_TYPE_FLOAT:
        if (value[0].opcode == VALUE_TYPE_FLOAT) {
            divisor = value[0].floatValue;
        } else {
            divisor = (float)value[0].integerValue;
        }

        // NOTE: Original code is slightly different, it performs bitwise and
        // with 0x7FFFFFFF in order to determine if it's zero. Probably some
        // kind of compiler optimization.
        if (divisor == 0.0) {
            programFatalError("Division (DIV) by zero");
        }

        programStackPushFloat(program, value[1].floatValue / divisor);
        break;
    case VALUE_TYPE_INT:
        if (value[0].opcode == VALUE_TYPE_FLOAT) {
            divisor = value[0].floatValue;

            // NOTE: Same as above.
            if (divisor == 0.0) {
                programFatalError("Division (DIV) by zero");
            }

            programStackPushFloat(program, (float)value[1].integerValue / divisor);
        } else {
            if (value[0].integerValue == 0) {
                programFatalError("Division (DIV) by zero");
            }

            programStackPushInteger(program, value[1].integerValue / value[0].integerValue);
        }
        break;
    }
}

// 0x46A5B8
static void opModulo(Program* program)
{
    ProgramValue value[2];

    for (int arg = 0; arg < 2; arg++) {
        value[arg] = programStackPopValue(program);
    }

    if (value[1].opcode == VALUE_TYPE_FLOAT) {
        programFatalError("Trying to MOD a float");
    }

    if (value[1].opcode != VALUE_TYPE_INT) {
        return;
    }

    if (value[0].opcode == VALUE_TYPE_FLOAT) {
        programFatalError("Trying to MOD with a float");
    }

    if (value[0].integerValue == 0) {
        programFatalError("Division (MOD) by zero");
    }

    programStackPushInteger(program, value[1].integerValue % value[0].integerValue);
}

// 0x46A6B4
static void opLogicalOperatorAnd(Program* program)
{
    ProgramValue value[2];
    int result;

    for (int arg = 0; arg < 2; arg++) {
        value[arg] = programStackPopValue(program);
    }

    switch (value[1].opcode) {
    case VALUE_TYPE_STRING:
    case VALUE_TYPE_DYNAMIC_STRING:
        switch (value[0].opcode) {
        case VALUE_TYPE_STRING:
        case VALUE_TYPE_DYNAMIC_STRING:
            result = 1;
            break;
        case VALUE_TYPE_FLOAT:
            result = (value[0].integerValue & 0x7FFFFFFF) != 0;
            break;
        case VALUE_TYPE_INT:
            result = value[0].integerValue != 0;
            break;
        case VALUE_TYPE_PTR:
            result = value[0].pointerValue != nullptr;
            break;
        default:
            assert(false && "Should be unreachable");
        }
        break;
    case VALUE_TYPE_FLOAT:
        switch (value[0].opcode) {
        case VALUE_TYPE_STRING:
        case VALUE_TYPE_DYNAMIC_STRING:
            result = value[1].integerValue != 0;
            break;
        case VALUE_TYPE_FLOAT:
            result = (value[1].integerValue & 0x7FFFFFFF) && (value[0].integerValue & 0x7FFFFFFF);
            break;
        case VALUE_TYPE_INT:
            result = (value[1].integerValue & 0x7FFFFFFF) && (value[0].integerValue != 0);
            break;
        case VALUE_TYPE_PTR:
            result = (value[1].integerValue & 0x7FFFFFFF) && (value[0].pointerValue != nullptr);
            break;
        default:
            assert(false && "Should be unreachable");
        }
        break;
    case VALUE_TYPE_INT:
        switch (value[0].opcode) {
        case VALUE_TYPE_STRING:
        case VALUE_TYPE_DYNAMIC_STRING:
            result = value[1].integerValue != 0;
            break;
        case VALUE_TYPE_FLOAT:
            result = (value[1].integerValue != 0) && (value[0].integerValue & 0x7FFFFFFF);
            break;
        case VALUE_TYPE_INT:
            result = (value[1].integerValue != 0) && (value[0].integerValue != 0);
            break;
        case VALUE_TYPE_PTR:
            result = (value[1].integerValue != 0) && (value[0].pointerValue != nullptr);
            break;
        default:
            assert(false && "Should be unreachable");
        }
        break;
    case VALUE_TYPE_PTR:
        switch (value[0].opcode) {
        case VALUE_TYPE_STRING:
        case VALUE_TYPE_DYNAMIC_STRING:
            result = value[1].pointerValue != nullptr;
            break;
        case VALUE_TYPE_FLOAT:
            result = (value[1].pointerValue != nullptr) && (value[0].integerValue & 0x7FFFFFFF);
            break;
        case VALUE_TYPE_INT:
            result = (value[1].pointerValue != nullptr) && (value[0].integerValue != 0);
            break;
        case VALUE_TYPE_PTR:
            result = (value[1].pointerValue != nullptr) && (value[0].pointerValue != nullptr);
            break;
        default:
            assert(false && "Should be unreachable");
        }
        break;
    default:
        assert(false && "Should be unreachable");
    }

    programStackPushInteger(program, result);
}

// 0x46A8D8
static void opLogicalOperatorOr(Program* program)
{
    ProgramValue value[2];
    int result;

    for (int arg = 0; arg < 2; arg++) {
        value[arg] = programStackPopValue(program);
    }

    switch (value[1].opcode) {
    case VALUE_TYPE_STRING:
    case VALUE_TYPE_DYNAMIC_STRING:
        switch (value[0].opcode) {
        case VALUE_TYPE_STRING:
        case VALUE_TYPE_DYNAMIC_STRING:
        case VALUE_TYPE_FLOAT:
        case VALUE_TYPE_INT:
        case VALUE_TYPE_PTR:
            result = 1;
            break;
        default:
            assert(false && "Should be unreachable");
        }
        break;
    case VALUE_TYPE_FLOAT:
        switch (value[0].opcode) {
        case VALUE_TYPE_STRING:
        case VALUE_TYPE_DYNAMIC_STRING:
            result = 1;
            break;
        case VALUE_TYPE_FLOAT:
            result = (value[1].integerValue & 0x7FFFFFFF) || (value[0].integerValue & 0x7FFFFFFF);
            break;
        case VALUE_TYPE_INT:
            result = (value[1].integerValue & 0x7FFFFFFF) || (value[0].integerValue != 0);
            break;
        case VALUE_TYPE_PTR:
            result = (value[1].integerValue & 0x7FFFFFFF) || (value[0].pointerValue != nullptr);
            break;
        default:
            assert(false && "Should be unreachable");
        }
        break;
    case VALUE_TYPE_INT:
        switch (value[0].opcode) {
        case VALUE_TYPE_STRING:
        case VALUE_TYPE_DYNAMIC_STRING:
            result = 1;
            break;
        case VALUE_TYPE_FLOAT:
            result = (value[1].integerValue != 0) || (value[0].integerValue & 0x7FFFFFFF);
            break;
        case VALUE_TYPE_INT:
            result = (value[1].integerValue != 0) || (value[0].integerValue != 0);
            break;
        case VALUE_TYPE_PTR:
            result = (value[1].integerValue != 0) || (value[0].pointerValue != nullptr);
            break;
        default:
            assert(false && "Should be unreachable");
        }
        break;
    case VALUE_TYPE_PTR:
        switch (value[0].opcode) {
        case VALUE_TYPE_STRING:
        case VALUE_TYPE_DYNAMIC_STRING:
            result = 1;
            break;
        case VALUE_TYPE_FLOAT:
            result = (value[1].pointerValue != nullptr) || (value[0].integerValue & 0x7FFFFFFF);
            break;
        case VALUE_TYPE_INT:
            result = (value[1].pointerValue != nullptr) || (value[0].integerValue != 0);
            break;
        case VALUE_TYPE_PTR:
            result = (value[1].pointerValue != nullptr) || (value[0].pointerValue != nullptr);
            break;
        default:
            assert(false && "Should be unreachable");
        }
        break;
    default:
        assert(false && "Should be unreachable");
    }

    programStackPushInteger(program, result);
}

// 0x46AACC
static void opLogicalOperatorNot(Program* program)
{
    ProgramValue value = programStackPopValue(program);
    programStackPushInteger(program, value.integerValue == 0);
}

// 0x46AB2C
static void opUnaryMinus(Program* program)
{
    // SFALL: Fix vanilla negate operator for float values.
    ProgramValue programValue = programStackPopValue(program);
    switch (programValue.opcode) {
    case VALUE_TYPE_INT:
        programStackPushInteger(program, -programValue.integerValue);
        break;
    case VALUE_TYPE_FLOAT:
        programStackPushFloat(program, -programValue.floatValue);
        break;
    default:
        programFatalError("Invalid arg given to NEG");
    }
}

// 0x46AB84
static void opBitwiseOperatorNot(Program* program)
{
    int value = programStackPopInteger(program);
    programStackPushInteger(program, ~value);
}

// floor
// 0x46ABDC
static void opFloor(Program* program)
{
    ProgramValue value = programStackPopValue(program);

    if (value.opcode == VALUE_TYPE_STRING) {
        programFatalError("Invalid arg given to floor()");
    } else if (value.opcode == VALUE_TYPE_FLOAT) {
        value.opcode = VALUE_TYPE_INT;
        value.integerValue = (int)value.floatValue;
    }

    programStackPushValue(program, value);
}

// 0x46AC78
static void opBitwiseOperatorAnd(Program* program)
{
    ProgramValue value[2];
    int result;

    for (int arg = 0; arg < 2; arg++) {
        value[arg] = programStackPopValue(program);
    }

    switch (value[1].opcode) {
    case VALUE_TYPE_FLOAT:
        switch (value[0].opcode) {
        case VALUE_TYPE_FLOAT:
            result = (int)value[1].floatValue & (int)value[0].floatValue;
            break;
        default:
            result = (int)value[1].floatValue & value[0].integerValue;
            break;
        }
        break;
    case VALUE_TYPE_INT:
        switch (value[0].opcode) {
        case VALUE_TYPE_FLOAT:
            result = value[1].integerValue & (int)value[0].floatValue;
            break;
        default:
            result = value[1].integerValue & value[0].integerValue;
            break;
        }
        break;
    default:
        return;
    }

    programStackPushInteger(program, result);
}

// 0x46ADA4
static void opBitwiseOperatorOr(Program* program)
{
    ProgramValue value[2];
    int result;

    for (int arg = 0; arg < 2; arg++) {
        value[arg] = programStackPopValue(program);
    }

    switch (value[1].opcode) {
    case VALUE_TYPE_FLOAT:
        switch (value[0].opcode) {
        case VALUE_TYPE_FLOAT:
            result = (int)value[1].floatValue | (int)value[0].floatValue;
            break;
        default:
            result = (int)value[1].floatValue | value[0].integerValue;
            break;
        }
        break;
    case VALUE_TYPE_INT:
        switch (value[0].opcode) {
        case VALUE_TYPE_FLOAT:
            result = value[1].integerValue | (int)value[0].floatValue;
            break;
        default:
            result = value[1].integerValue | value[0].integerValue;
            break;
        }
        break;
    default:
        return;
    }

    programStackPushInteger(program, result);
}

// 0x46AED0
static void opBitwiseOperatorXor(Program* program)
{
    ProgramValue value[2];
    int result;

    for (int arg = 0; arg < 2; arg++) {
        value[arg] = programStackPopValue(program);
    }

    switch (value[1].opcode) {
    case VALUE_TYPE_FLOAT:
        switch (value[0].opcode) {
        case VALUE_TYPE_FLOAT:
            result = (int)value[1].floatValue ^ (int)value[0].floatValue;
            break;
        default:
            result = (int)value[1].floatValue ^ value[0].integerValue;
            break;
        }
        break;
    case VALUE_TYPE_INT:
        switch (value[0].opcode) {
        case VALUE_TYPE_FLOAT:
            result = value[1].integerValue ^ (int)value[0].floatValue;
            break;
        default:
            result = value[1].integerValue ^ value[0].integerValue;
            break;
        }
        break;
    default:
        return;
    }

    programStackPushInteger(program, result);
}

// 0x46AFFC
static void opSwapReturnStack(Program* program)
{
    ProgramValue v1 = programReturnStackPopValue(program);
    ProgramValue v2 = programReturnStackPopValue(program);
    programReturnStackPushValue(program, v1);
    programReturnStackPushValue(program, v2);
}

// 0x46B070
static void opLeaveCriticalSection(Program* program)
{
    program->flags &= ~PROGRAM_FLAG_CRITICAL_SECTION;
}

// 0x46B078
static void opEnterCriticalSection(Program* program)
{
    program->flags |= PROGRAM_FLAG_CRITICAL_SECTION;
}

// 0x46B080
static void opJump(Program* program)
{
    program->instructionPointer = programStackPopInteger(program);
}

// 0x46B108
static void opCall(Program* program)
{
    int value = programStackPopInteger(program);

    unsigned char* ptr = program->procedures + 4 + 24 * value;

    int flags = stackReadInt32(ptr, offsetof(Procedure, flags));
    if ((flags & PROCEDURE_FLAG_IMPORTED) != 0) {
        // TODO: Incomplete.
    } else {
        program->instructionPointer = stackReadInt32(ptr, offsetof(Procedure, bodyOffset));
        if ((flags & PROCEDURE_FLAG_CRITICAL) != 0) {
            program->flags |= PROGRAM_FLAG_CRITICAL_SECTION;
        }
    }
}

// 0x46B590
static void opPopFlags(Program* program)
{
    program->windowId = programStackPopInteger(program);
    program->checkWaitFunc = (InterpretCheckWaitFunc*)programStackPopPointer(program);
    program->flags = programStackPopInteger(program) & 0xFFFF;
}

// pop stack 2 -> set program address
// 0x46B63C
static void opPopReturn(Program* program)
{
    program->instructionPointer = programReturnStackPopInteger(program);
}

// 0x46B658
static void opPopExit(Program* program)
{
    program->instructionPointer = programReturnStackPopInteger(program);

    program->flags |= PROGRAM_FLAG_0x40;
}

// 0x46B67C
static void opPopFlagsReturn(Program* program)
{
    opPopFlags(program);
    program->instructionPointer = programReturnStackPopInteger(program);
}

// 0x46B698
static void opPopFlagsExit(Program* program)
{
    opPopFlags(program);
    program->instructionPointer = programReturnStackPopInteger(program);
    program->flags |= PROGRAM_FLAG_0x40;
}

// 0x46B6BC
static void opPopFlagsReturnValExit(Program* program)
{
    ProgramValue value = programStackPopValue(program);

    opPopFlags(program);
    program->instructionPointer = programReturnStackPopInteger(program);
    program->flags |= PROGRAM_FLAG_0x40;
    programStackPushValue(program, value);
}

// 0x46B73C
static void opPopFlagsReturnValExitExtern(Program* program)
{
    ProgramValue value = programStackPopValue(program);

    opPopFlags(program);

    Program* v1 = (Program*)programReturnStackPopPointer(program);
    v1->checkWaitFunc = (InterpretCheckWaitFunc*)programReturnStackPopPointer(program);
    v1->flags = programReturnStackPopInteger(program);

    program->instructionPointer = programReturnStackPopInteger(program);

    program->flags |= PROGRAM_FLAG_0x40;

    programStackPushValue(program, value);
}

// 0x46B808
static void opPopFlagsReturnExtern(Program* program)
{
    opPopFlags(program);

    Program* v1 = (Program*)programReturnStackPopPointer(program);
    v1->checkWaitFunc = (InterpretCheckWaitFunc*)programReturnStackPopPointer(program);
    v1->flags = programReturnStackPopInteger(program);

    program->instructionPointer = programReturnStackPopInteger(program);
}

// 0x46B86C
static void opPopFlagsExitExtern(Program* program)
{
    opPopFlags(program);

    Program* v1 = (Program*)programReturnStackPopPointer(program);
    v1->checkWaitFunc = (InterpretCheckWaitFunc*)programReturnStackPopPointer(program);
    v1->flags = programReturnStackPopInteger(program);

    program->instructionPointer = programReturnStackPopInteger(program);

    program->flags |= 0x40;
}

// pop value from stack 1 and push it to script popped from stack 2
// 0x46B8D8
static void opPopFlagsReturnValExtern(Program* program)
{
    ProgramValue value = programStackPopValue(program);

    opPopFlags(program);

    Program* v10 = (Program*)programReturnStackPopPointer(program);
    v10->checkWaitFunc = (InterpretCheckWaitFunc*)programReturnStackPopPointer(program);
    v10->flags = programReturnStackPopInteger(program);
    if ((value.opcode & 0xF7FF) == VALUE_TYPE_STRING) {
        char* string = programGetString(program, value.opcode, value.integerValue);
        ProgramValue otherValue;
        otherValue.integerValue = programPushString(v10, string);
        otherValue.opcode = VALUE_TYPE_DYNAMIC_STRING;
        programStackPushValue(v10, otherValue);
    } else {
        programStackPushValue(v10, value);
    }

    if (v10->flags & 0x80) {
        program->flags &= ~0x80;
    }

    program->instructionPointer = programReturnStackPopInteger(program);
    v10->instructionPointer = programReturnStackPopInteger(v10);
}

// 0x46BA10
static void opPopAddress(Program* program)
{
    programReturnStackPopValue(program);
}

// 0x46BA2C
static void opAtoD(Program* program)
{
    ProgramValue value = programReturnStackPopValue(program);
    programStackPushValue(program, value);
}

// 0x46BA68
static void opDtoA(Program* program)
{
    ProgramValue value = programStackPopValue(program);
    programReturnStackPushValue(program, value);
}

// 0x46BAC0
static void opExitProgram(Program* program)
{
    program->flags |= PROGRAM_FLAG_EXITED;
}

// 0x46BAC8
static void opStopProgram(Program* program)
{
    program->flags |= PROGRAM_FLAG_STOPPED;
}

// 0x46BAD0
static void opFetchGlobalVariable(Program* program)
{
    int addr = programStackPopInteger(program);

    ProgramValue value = program->stackValues->at(program->basePointer + addr);
    programStackPushValue(program, value);
}

// 0x46BB5C
static void opStoreGlobalVariable(Program* program)
{
    int addr = programStackPopInteger(program);
    ProgramValue value = programStackPopValue(program);

    ProgramValue oldValue = program->stackValues->at(program->basePointer + addr);
    if (oldValue.opcode == VALUE_TYPE_DYNAMIC_STRING) {
        _interpretDecStringRef(program, oldValue.opcode, oldValue.integerValue);
    }

    program->stackValues->at(program->basePointer + addr) = value;

    if (value.opcode == VALUE_TYPE_DYNAMIC_STRING) {
        // NOTE: Uninline.
        _interpretIncStringRef(program, VALUE_TYPE_DYNAMIC_STRING, value.integerValue);
    }
}

// 0x46BCAC
static void opSwapStack(Program* program)
{
    ProgramValue v1 = programStackPopValue(program);
    ProgramValue v2 = programStackPopValue(program);
    programStackPushValue(program, v1);
    programStackPushValue(program, v2);
}

// fetch_proc_address
// 0x46BD60
static void opFetchProcedureAddress(Program* program)
{
    int procedureIndex = programStackPopInteger(program);

    int address = stackReadInt32(program->procedures + 4 + sizeof(Procedure) * procedureIndex, offsetof(Procedure, bodyOffset));
    programStackPushInteger(program, address);
}

// Pops value from stack and throws it away.
//
// 0x46BE10
static void opPop(Program* program)
{
    programStackPopValue(program);
}

// 0x46BE4C
static void opDuplicate(Program* program)
{
    ProgramValue value = programStackPopValue(program);
    programStackPushValue(program, value);
    programStackPushValue(program, value);
}

// 0x46BEC8
static void opStoreExternalVariable(Program* program)
{
    ProgramValue addr = programStackPopValue(program);
    ProgramValue value = programStackPopValue(program);

    const char* identifier = programGetIdentifier(program, addr.integerValue);

    if (externalVariableSetValue(program, identifier, value)) {
        char err[256];
        snprintf(err, sizeof(err), "External variable %s does not exist\n", identifier);
        programFatalError(err);
    }
}

// 0x46BF90
static void opFetchExternalVariable(Program* program)
{
    ProgramValue addr = programStackPopValue(program);

    const char* identifier = programGetIdentifier(program, addr.integerValue);

    ProgramValue value;
    if (externalVariableGetValue(program, identifier, value) != 0) {
        char err[256];
        snprintf(err, sizeof(err), "External variable %s does not exist\n", identifier);
        programFatalError(err);
    }

    programStackPushValue(program, value);
}

// 0x46C044
static void opExportProcedure(Program* program)
{
    int procedureIndex = programStackPopInteger(program);
    int argumentCount = programStackPopInteger(program);

    unsigned char* proc_ptr = program->procedures + 4 + sizeof(Procedure) * procedureIndex;

    char* procedureName = programGetIdentifier(program, stackReadInt32(proc_ptr, offsetof(Procedure, nameOffset)));
    int procedureAddress = stackReadInt32(proc_ptr, offsetof(Procedure, bodyOffset));

    if (externalProcedureCreate(program, procedureName, procedureAddress, argumentCount) != 0) {
        char err[256];
        snprintf(err, sizeof(err), "Error exporting procedure %s", procedureName);
        programFatalError(err);
    }
}

// 0x46C120
static void opExportVariable(Program* program)
{
    ProgramValue addr = programStackPopValue(program);

    const char* identifier = programGetIdentifier(program, addr.integerValue);

    if (externalVariableCreate(program, identifier)) {
        char err[256];
        snprintf(err, sizeof(err), "External variable %s already exists", identifier);
        programFatalError(err);
    }
}

// 0x46C1A0
static void opExit(Program* program)
{
    program->flags |= PROGRAM_FLAG_EXITED;

    Program* parent = program->parent;
    if (parent != nullptr) {
        if ((parent->flags & PROGRAM_FLAG_0x0100) != 0) {
            parent->flags &= ~PROGRAM_FLAG_0x0100;
        }
    }

    if (!program->exited) {
        intLibRemoveProgramReferences(program);
        program->exited = true;
    }
}

// 0x46C1EC
static void opDetach(Program* program)
{
    Program* parent = program->parent;
    if (parent == nullptr) {
        return;
    }

    parent->flags &= ~PROGRAM_FLAG_0x20;
    parent->flags &= ~PROGRAM_FLAG_0x0100;

    if (parent->child == program) {
        parent->child = nullptr;
    }
}

// callstart
// 0x46C218
static void opCallStart(Program* program)
{
    if (program->child) {
        programFatalError("Error, already have a child process\n");
    }

    program->flags |= PROGRAM_FLAG_0x20;

    char* name = programStackPopString(program);

    // NOTE: Uninline.
    program->child = runScript(name);
    if (program->child == nullptr) {
        char err[260];
        snprintf(err, sizeof(err), "Error spawning child %s", name);
        programFatalError(err);
    }

    program->child->parent = program;
    program->child->windowId = program->windowId;
}

// spawn
// 0x46C344
static void opSpawn(Program* program)
{
    if (program->child) {
        programFatalError("Error, already have a child process\n");
    }

    program->flags |= PROGRAM_FLAG_0x0100;

    char* name = programStackPopString(program);

    // NOTE: Uninline.
    program->child = runScript(name);
    if (program->child == nullptr) {
        char err[260];
        snprintf(err, sizeof(err), "Error spawning child %s", name);
        programFatalError(err);
    }

    program->child->parent = program;
    program->child->windowId = program->windowId;

    if ((program->flags & PROGRAM_FLAG_CRITICAL_SECTION) != 0) {
        program->child->flags |= PROGRAM_FLAG_CRITICAL_SECTION;
        _interpret(program->child, -1);
    }
}

// fork
// 0x46C490
static Program* forkProgram(Program* program)
{
    char* name = programStackPopString(program);
    Program* forked = runScript(name);

    if (forked == nullptr) {
        char err[256];
        snprintf(err, sizeof(err), "couldn't fork script '%s'", name);
        programFatalError(err);
    }

    forked->windowId = program->windowId;

    return forked;
}

// NOTE: Uncollapsed 0x46C490 with different signature.
//
// 0x46C490
static void opFork(Program* program)
{
    forkProgram(program);
}

// 0x46C574
static void opExec(Program* program)
{
    Program* parent = program->parent;
    Program* fork = forkProgram(program);

    if (parent != nullptr) {
        fork->parent = parent;
        parent->child = fork;
    }

    fork->child = nullptr;

    program->parent = nullptr;
    program->flags |= PROGRAM_FLAG_EXITED;

    // probably inlining due to check for null
    parent = program->parent;
    if (parent != nullptr) {
        if ((parent->flags & PROGRAM_FLAG_0x0100) != 0) {
            parent->flags &= ~PROGRAM_FLAG_0x0100;
        }
    }

    _purgeProgram(program);
}

// 0x46C5D8
static void opCheckProcedureArgumentCount(Program* program)
{
    int expectedArgumentCount = programStackPopInteger(program);
    int procedureIndex = programStackPopInteger(program);

    int actualArgumentCount = stackReadInt32(program->procedures + 4 + 24 * procedureIndex, offsetof(Procedure, argCount));
    if (actualArgumentCount != expectedArgumentCount) {
        const char* identifier = programGetIdentifier(program, stackReadInt32(program->procedures + 4 + 24 * procedureIndex, offsetof(Procedure, nameOffset)));
        char err[260];
        snprintf(err, sizeof(err), "Wrong number of args to procedure %s\n", identifier);
        programFatalError(err);
    }
}

// lookup_string_proc
// 0x46C6B4
static void opLookupStringProc(Program* program)
{
    const char* procedureNameToLookup = programStackPopString(program);
    int procedureCount = stackReadInt32(program->procedures, 0);

    // Skip procedure count (4 bytes) and main procedure, which cannot be
    // looked up.
    unsigned char* procedurePtr = program->procedures + 4 + sizeof(Procedure);

    // Start with 1 since we've skipped main procedure, which is always at
    // index 0.
    for (int index = 1; index < procedureCount; index++) {
        int offset = stackReadInt32(procedurePtr, offsetof(Procedure, nameOffset));
        const char* procedureName = programGetIdentifier(program, offset);
        if (compat_stricmp(procedureName, procedureNameToLookup) == 0) {
            programStackPushInteger(program, index);
            return;
        }

        procedurePtr += sizeof(Procedure);
    }

    char err[260];
    snprintf(err, sizeof(err), "Couldn't find string procedure %s\n", procedureNameToLookup);
    programFatalError(err);
}

// 0x46C7DC
void interpreterRegisterOpcodeHandlers()
{
    _Enabled = 1;

    // NOTE: The original code has different sorting.
    interpreterRegisterOpcode(OPCODE_NOOP, opNoop);
    interpreterRegisterOpcode(OPCODE_PUSH, opPush);
    interpreterRegisterOpcode(OPCODE_ENTER_CRITICAL_SECTION, opEnterCriticalSection);
    interpreterRegisterOpcode(OPCODE_LEAVE_CRITICAL_SECTION, opLeaveCriticalSection);
    interpreterRegisterOpcode(OPCODE_JUMP, opJump);
    interpreterRegisterOpcode(OPCODE_CALL, opCall);
    interpreterRegisterOpcode(OPCODE_CALL_AT, opDelayedCall);
    interpreterRegisterOpcode(OPCODE_CALL_WHEN, opConditionalCall);
    interpreterRegisterOpcode(OPCODE_CALLSTART, opCallStart);
    interpreterRegisterOpcode(OPCODE_EXEC, opExec);
    interpreterRegisterOpcode(OPCODE_SPAWN, opSpawn);
    interpreterRegisterOpcode(OPCODE_FORK, opFork);
    interpreterRegisterOpcode(OPCODE_A_TO_D, opAtoD);
    interpreterRegisterOpcode(OPCODE_D_TO_A, opDtoA);
    interpreterRegisterOpcode(OPCODE_EXIT, opExit);
    interpreterRegisterOpcode(OPCODE_DETACH, opDetach);
    interpreterRegisterOpcode(OPCODE_EXIT_PROGRAM, opExitProgram);
    interpreterRegisterOpcode(OPCODE_STOP_PROGRAM, opStopProgram);
    interpreterRegisterOpcode(OPCODE_FETCH_GLOBAL, opFetchGlobalVariable);
    interpreterRegisterOpcode(OPCODE_STORE_GLOBAL, opStoreGlobalVariable);
    interpreterRegisterOpcode(OPCODE_FETCH_EXTERNAL, opFetchExternalVariable);
    interpreterRegisterOpcode(OPCODE_STORE_EXTERNAL, opStoreExternalVariable);
    interpreterRegisterOpcode(OPCODE_EXPORT_VARIABLE, opExportVariable);
    interpreterRegisterOpcode(OPCODE_EXPORT_PROCEDURE, opExportProcedure);
    interpreterRegisterOpcode(OPCODE_SWAP, opSwapStack);
    interpreterRegisterOpcode(OPCODE_SWAPA, opSwapReturnStack);
    interpreterRegisterOpcode(OPCODE_POP, opPop);
    interpreterRegisterOpcode(OPCODE_DUP, opDuplicate);
    interpreterRegisterOpcode(OPCODE_POP_RETURN, opPopReturn);
    interpreterRegisterOpcode(OPCODE_POP_EXIT, opPopExit);
    interpreterRegisterOpcode(OPCODE_POP_ADDRESS, opPopAddress);
    interpreterRegisterOpcode(OPCODE_POP_FLAGS, opPopFlags);
    interpreterRegisterOpcode(OPCODE_POP_FLAGS_RETURN, opPopFlagsReturn);
    interpreterRegisterOpcode(OPCODE_POP_FLAGS_EXIT, opPopFlagsExit);
    interpreterRegisterOpcode(OPCODE_POP_FLAGS_RETURN_EXTERN, opPopFlagsReturnExtern);
    interpreterRegisterOpcode(OPCODE_POP_FLAGS_EXIT_EXTERN, opPopFlagsExitExtern);
    interpreterRegisterOpcode(OPCODE_POP_FLAGS_RETURN_VAL_EXTERN, opPopFlagsReturnValExtern);
    interpreterRegisterOpcode(OPCODE_POP_FLAGS_RETURN_VAL_EXIT, opPopFlagsReturnValExit);
    interpreterRegisterOpcode(OPCODE_POP_FLAGS_RETURN_VAL_EXIT_EXTERN, opPopFlagsReturnValExitExtern);
    interpreterRegisterOpcode(OPCODE_CHECK_PROCEDURE_ARGUMENT_COUNT, opCheckProcedureArgumentCount);
    interpreterRegisterOpcode(OPCODE_LOOKUP_PROCEDURE_BY_NAME, opLookupStringProc);
    interpreterRegisterOpcode(OPCODE_POP_BASE, opPopBase);
    interpreterRegisterOpcode(OPCODE_POP_TO_BASE, opPopToBase);
    interpreterRegisterOpcode(OPCODE_PUSH_BASE, opPushBase);
    interpreterRegisterOpcode(OPCODE_SET_GLOBAL, opSetGlobal);
    interpreterRegisterOpcode(OPCODE_FETCH_PROCEDURE_ADDRESS, opFetchProcedureAddress);
    interpreterRegisterOpcode(OPCODE_DUMP, opDump);
    interpreterRegisterOpcode(OPCODE_IF, opIf);
    interpreterRegisterOpcode(OPCODE_WHILE, opWhile);
    interpreterRegisterOpcode(OPCODE_STORE, opStore);
    interpreterRegisterOpcode(OPCODE_FETCH, opFetch);
    interpreterRegisterOpcode(OPCODE_EQUAL, opConditionalOperatorEqual);
    interpreterRegisterOpcode(OPCODE_NOT_EQUAL, opConditionalOperatorNotEqual);
    interpreterRegisterOpcode(OPCODE_LESS_THAN_EQUAL, opConditionalOperatorLessThanEquals);
    interpreterRegisterOpcode(OPCODE_GREATER_THAN_EQUAL, opConditionalOperatorGreaterThanEquals);
    interpreterRegisterOpcode(OPCODE_LESS_THAN, opConditionalOperatorLessThan);
    interpreterRegisterOpcode(OPCODE_GREATER_THAN, opConditionalOperatorGreaterThan);
    interpreterRegisterOpcode(OPCODE_ADD, opAdd);
    interpreterRegisterOpcode(OPCODE_SUB, opSubtract);
    interpreterRegisterOpcode(OPCODE_MUL, opMultiply);
    interpreterRegisterOpcode(OPCODE_DIV, opDivide);
    interpreterRegisterOpcode(OPCODE_MOD, opModulo);
    interpreterRegisterOpcode(OPCODE_AND, opLogicalOperatorAnd);
    interpreterRegisterOpcode(OPCODE_OR, opLogicalOperatorOr);
    interpreterRegisterOpcode(OPCODE_BITWISE_AND, opBitwiseOperatorAnd);
    interpreterRegisterOpcode(OPCODE_BITWISE_OR, opBitwiseOperatorOr);
    interpreterRegisterOpcode(OPCODE_BITWISE_XOR, opBitwiseOperatorXor);
    interpreterRegisterOpcode(OPCODE_BITWISE_NOT, opBitwiseOperatorNot);
    interpreterRegisterOpcode(OPCODE_FLOOR, opFloor);
    interpreterRegisterOpcode(OPCODE_NOT, opLogicalOperatorNot);
    interpreterRegisterOpcode(OPCODE_NEGATE, opUnaryMinus);
    interpreterRegisterOpcode(OPCODE_WAIT, opWait);
    interpreterRegisterOpcode(OPCODE_CANCEL, opCancel);
    interpreterRegisterOpcode(OPCODE_CANCEL_ALL, opCancelAll);
    interpreterRegisterOpcode(OPCODE_START_CRITICAL, opEnterCriticalSection);
    interpreterRegisterOpcode(OPCODE_END_CRITICAL, opLeaveCriticalSection);

    intLibInit();
    _initExport();
}

// 0x46CC68
void _interpretClose()
{
    externalVariablesClear();
    intLibExit();
}

// 0x46CCA4
void _interpret(Program* program, int a2)
{
    char err[260];

    Program* oldCurrentProgram = gInterpreterCurrentProgram;

    if (!_Enabled) {
        return;
    }

    if (_busy) {
        return;
    }

    if (program->exited || (program->flags & PROGRAM_FLAG_0x20) != 0 || (program->flags & PROGRAM_FLAG_0x0100) != 0) {
        return;
    }

    if (program->field_78 == -1) {
        program->field_78 = 1000 * _timerFunc() / _timerTick;
    }

    gInterpreterCurrentProgram = program;

    if (setjmp(program->env)) {
        gInterpreterCurrentProgram = oldCurrentProgram;
        program->flags |= PROGRAM_FLAG_EXITED | PROGRAM_FLAG_0x04;
        return;
    }

    if ((program->flags & PROGRAM_FLAG_CRITICAL_SECTION) != 0 && a2 < 3) {
        a2 = 3;
    }

    while ((program->flags & PROGRAM_FLAG_CRITICAL_SECTION) != 0 || --a2 != -1) {
        if ((program->flags & (PROGRAM_FLAG_EXITED | PROGRAM_FLAG_0x04 | PROGRAM_FLAG_STOPPED | PROGRAM_FLAG_0x20 | PROGRAM_FLAG_0x40 | PROGRAM_FLAG_0x0100)) != 0) {
            break;
        }

        if (program->exited) {
            break;
        }

        if ((program->flags & PROGRAM_IS_WAITING) != 0) {
            _busy = 1;

            if (program->checkWaitFunc != nullptr) {
                if (!program->checkWaitFunc(program)) {
                    _busy = 0;
                    continue;
                }
            }

            _busy = 0;
            program->checkWaitFunc = nullptr;
            program->flags &= ~PROGRAM_IS_WAITING;
        }

        // NOTE: Uninline.
        opcode_t opcode = _getOp(program);

        // TODO: Replace with field_82 and field_80?
        program->flags &= 0xFFFF;
        program->flags |= (opcode << 16);

        if (!((opcode >> 8) & 0x80)) {
            snprintf(err, sizeof(err), "Bad opcode %x %c %d.", opcode, opcode, opcode);
            programFatalError(err);
        }

        unsigned int opcodeIndex = opcode & 0x3FF;
        OpcodeHandler* handler = gInterpreterOpcodeHandlers[opcodeIndex];
        if (handler == nullptr) {
            snprintf(err, sizeof(err), "Undefined opcode %x.", opcode);
            programFatalError(err);
        }

        handler(program);
    }

    if ((program->flags & PROGRAM_FLAG_EXITED) != 0) {
        if (program->parent != nullptr) {
            if (program->parent->flags & PROGRAM_FLAG_0x20) {
                program->parent->flags &= ~PROGRAM_FLAG_0x20;
                program->parent->child = nullptr;
                program->parent = nullptr;
            }
        }
    }

    program->flags &= ~PROGRAM_FLAG_0x40;
    gInterpreterCurrentProgram = oldCurrentProgram;

    programMarkHeap(program);
}

// Prepares program stacks for executing proc at [address].
//
// 0x46CED0
static void _setupCallWithReturnVal(Program* program, int address, int returnAddress)
{
    // Save current instruction pointer
    programReturnStackPushInteger(program, program->instructionPointer);

    // Save return address
    programReturnStackPushInteger(program, returnAddress);

    // Save program flags
    programStackPushInteger(program, program->flags & 0xFFFF);

    programStackPushPointer(program, (void*)program->checkWaitFunc);

    programStackPushInteger(program, program->windowId);

    program->flags &= ~0xFFFF;
    program->instructionPointer = address;
}

// NOTE: Inlined.
//
// 0x46CF78
static void _setupCall(Program* program, int address, int returnAddress)
{
    _setupCallWithReturnVal(program, address, returnAddress);
    programStackPushInteger(program, 0);
}

// 0x46CF9C
static void _setupExternalCallWithReturnVal(Program* program1, Program* program2, int address, int a4)
{
    programReturnStackPushInteger(program2, program2->instructionPointer);

    programReturnStackPushInteger(program2, program1->flags & 0xFFFF);

    programReturnStackPushPointer(program2, (void*)program1->checkWaitFunc);

    programReturnStackPushPointer(program2, program1);

    programReturnStackPushInteger(program2, a4);

    programStackPushInteger(program2, program2->flags & 0xFFFF);

    programStackPushPointer(program2, (void*)program2->checkWaitFunc);

    programStackPushInteger(program2, program2->windowId);

    program2->flags &= ~0xFFFF;
    program2->instructionPointer = address;
    program2->windowId = program1->windowId;

    program1->flags |= PROGRAM_FLAG_0x20;
}

// NOTE: Inlined.
//
// 0x46D0B0
static void _setupExternalCall(Program* program1, Program* program2, int address, int a4)
{
    _setupExternalCallWithReturnVal(program1, program2, address, a4);
    programStackPushInteger(program2, 0);
}

// 0x46DB58
void _executeProc(Program* program, int procedureIndex)
{
    unsigned char* procedurePtr;
    char* procedureIdentifier;
    int procedureAddress;
    Program* externalProgram;
    int externalProcedureAddress;
    int externalProcedureArgumentCount;
    int procedureFlags;
    char err[256];

    procedurePtr = program->procedures + 4 + sizeof(Procedure) * procedureIndex;
    procedureFlags = stackReadInt32(procedurePtr, offsetof(Procedure, flags));
    if ((procedureFlags & PROCEDURE_FLAG_IMPORTED) != 0) {
        procedureIdentifier = programGetIdentifier(program, stackReadInt32(procedurePtr, offsetof(Procedure, nameOffset)));
        externalProgram = externalProcedureGetProgram(procedureIdentifier, &externalProcedureAddress, &externalProcedureArgumentCount);
        if (externalProgram != nullptr) {
            if (externalProcedureArgumentCount == 0) {
            } else {
                snprintf(err, sizeof(err), "External procedure cannot take arguments in interrupt context");
                _interpretOutput(err);
            }
        } else {
            snprintf(err, sizeof(err), "External procedure %s not found\n", procedureIdentifier);
            _interpretOutput(err);
        }

        // NOTE: Uninline.
        _setupExternalCall(program, externalProgram, externalProcedureAddress, 28);

        procedurePtr = externalProgram->procedures + 4 + sizeof(Procedure) * procedureIndex;
        procedureFlags = stackReadInt32(procedurePtr, offsetof(Procedure, flags));

        if ((procedureFlags & PROCEDURE_FLAG_CRITICAL) != 0) {
            // NOTE: Uninline.
            opEnterCriticalSection(externalProgram);
            _interpret(externalProgram, 0);
        }
    } else {
        procedureAddress = stackReadInt32(procedurePtr, offsetof(Procedure, bodyOffset));

        // NOTE: Uninline.
        _setupCall(program, procedureAddress, 20);

        if ((procedureFlags & PROCEDURE_FLAG_CRITICAL) != 0) {
            // NOTE: Uninline.
            opEnterCriticalSection(program);
            _interpret(program, 0);
        }
    }
}

// Returns index of the procedure with specified name or -1 if no such
// procedure exists.
//
// 0x46DCD0
int programFindProcedure(Program* program, const char* name)
{
    int procedureCount = stackReadInt32(program->procedures, 0);

    unsigned char* ptr = program->procedures + 4;
    for (int index = 0; index < procedureCount; index++) {
        int identifierOffset = stackReadInt32(ptr, offsetof(Procedure, nameOffset));
        if (compat_stricmp((char*)(program->identifiers + identifierOffset), name) == 0) {
            return index;
        }

        ptr += sizeof(Procedure);
    }

    return -1;
}

// 0x46DD2C
void _executeProcedure(Program* program, int procedureIndex)
{
    unsigned char* procedurePtr;
    char* procedureIdentifier;
    int procedureAddress;
    Program* externalProgram;
    int externalProcedureAddress;
    int externalProcedureArgumentCount;
    int procedureFlags;
    char err[256];
    jmp_buf env;

    procedurePtr = program->procedures + 4 + sizeof(Procedure) * procedureIndex;
    procedureFlags = stackReadInt32(procedurePtr, offsetof(Procedure, flags));

    if ((procedureFlags & PROCEDURE_FLAG_IMPORTED) != 0) {
        procedureIdentifier = programGetIdentifier(program, stackReadInt32(procedurePtr, offsetof(Procedure, nameOffset)));
        externalProgram = externalProcedureGetProgram(procedureIdentifier, &externalProcedureAddress, &externalProcedureArgumentCount);
        if (externalProgram != nullptr) {
            if (externalProcedureArgumentCount == 0) {
                // NOTE: Uninline.
                _setupExternalCall(program, externalProgram, externalProcedureAddress, 32);
                memcpy(env, program->env, sizeof(env));
                _interpret(externalProgram, -1);
                memcpy(externalProgram->env, env, sizeof(env));
            } else {
                snprintf(err, sizeof(err), "External procedure cannot take arguments in interrupt context");
                _interpretOutput(err);
            }
        } else {
            snprintf(err, sizeof(err), "External procedure %s not found\n", procedureIdentifier);
            _interpretOutput(err);
        }
    } else {
        procedureAddress = stackReadInt32(procedurePtr, offsetof(Procedure, bodyOffset));

        // NOTE: Uninline.
        _setupCall(program, procedureAddress, 24);
        memcpy(env, program->env, sizeof(env));
        _interpret(program, -1);
        memcpy(program->env, env, sizeof(env));
    }
}

// 0x46DEE4
static void _doEvents()
{
    ProgramListNode* programListNode;
    unsigned int time;
    int procedureCount;
    int procedureIndex;
    unsigned char* procedurePtr;
    int procedureFlags;
    int oldProgramFlags;
    int oldInstructionPointer;
    int data;
    jmp_buf env;

    if (_suspendEvents) {
        return;
    }

    programListNode = gInterpreterProgramListHead;
    time = 1000 * _timerFunc() / _timerTick;

    while (programListNode != nullptr) {
        procedureCount = stackReadInt32(programListNode->program->procedures, 0);

        procedurePtr = programListNode->program->procedures + 4;
        for (procedureIndex = 0; procedureIndex < procedureCount; procedureIndex++) {
            procedureFlags = stackReadInt32(procedurePtr, offsetof(Procedure, flags));
            if ((procedureFlags & PROCEDURE_FLAG_CONDITIONAL) != 0) {
                memcpy(env, programListNode->program, sizeof(env));
                oldProgramFlags = programListNode->program->flags;
                oldInstructionPointer = programListNode->program->instructionPointer;

                programListNode->program->flags = 0;
                programListNode->program->instructionPointer = stackReadInt32(procedurePtr, offsetof(Procedure, conditionOffset));
                _interpret(programListNode->program, -1);

                if ((programListNode->program->flags & PROGRAM_FLAG_0x04) == 0) {
                    data = programStackPopInteger(programListNode->program);

                    programListNode->program->flags = oldProgramFlags;
                    programListNode->program->instructionPointer = oldInstructionPointer;

                    if (data != 0) {
                        // NOTE: Uninline.
                        stackWriteInt32(0, procedurePtr, offsetof(Procedure, flags));
                        _executeProc(programListNode->program, procedureIndex);
                    }
                }

                memcpy(programListNode->program, env, sizeof(env));
            } else if ((procedureFlags & PROCEDURE_FLAG_TIMED) != 0) {
                if ((unsigned int)stackReadInt32(procedurePtr, offsetof(Procedure, time)) < time) {
                    // NOTE: Uninline.
                    stackWriteInt32(0, procedurePtr, offsetof(Procedure, flags));
                    _executeProc(programListNode->program, procedureIndex);
                }
            }
            procedurePtr += sizeof(Procedure);
        }

        programListNode = programListNode->next;
    }
}

// 0x46E10C
static void programListNodeFree(ProgramListNode* programListNode)
{
    ProgramListNode* tmp;

    tmp = programListNode->next;
    if (tmp != nullptr) {
        tmp->prev = programListNode->prev;
    }

    tmp = programListNode->prev;
    if (tmp != nullptr) {
        tmp->next = programListNode->next;
    } else {
        gInterpreterProgramListHead = programListNode->next;
    }

    programFree(programListNode->program);
    internal_free_safe(programListNode, __FILE__, __LINE__); // "..\\int\\INTRPRET.C", 2923
}

// 0x46E154
void programListNodeCreate(Program* program)
{
    ProgramListNode* programListNode = (ProgramListNode*)internal_malloc_safe(sizeof(*programListNode), __FILE__, __LINE__); // .\\int\\INTRPRET.C, 2907
    programListNode->program = program;
    programListNode->next = gInterpreterProgramListHead;
    programListNode->prev = nullptr;

    if (gInterpreterProgramListHead != nullptr) {
        gInterpreterProgramListHead->prev = programListNode;
    }

    gInterpreterProgramListHead = programListNode;
}

// NOTE: Inlined.
//
// 0x46E15C
void runProgram(Program* program)
{
    program->flags |= PROGRAM_FLAG_0x02;
    programListNodeCreate(program);
}

// NOTE: Inlined.
//
// 0x46E19C
Program* runScript(char* name)
{
    Program* program;

    // NOTE: Uninline.
    program = programCreateByPath(_interpretMangleName(name));
    if (program != nullptr) {
        // NOTE: Uninline.
        runProgram(program);
        _interpret(program, 24);
    }

    return program;
}

// 0x46E1EC
void _updatePrograms()
{
    // CE: Implementation is different. Sfall inserts global scripts into
    // program list upon creation, so engine does not diffirentiate between
    // global and normal scripts. Global scripts in CE are not part of program
    // list, so we need a separate call to continue execution (usually
    // non-critical calls scheduled from managed windows). One more thing to
    // note is that global scripts in CE cannot handle conditional/timed procs
    // (which are not used anyway).
    sfall_gl_scr_update(_cpuBurstSize);

    ProgramListNode* curr = gInterpreterProgramListHead;
    while (curr != nullptr) {
        ProgramListNode* next = curr->next;
        if (curr->program != nullptr) {
            _interpret(curr->program, _cpuBurstSize);

            if (curr->program->exited) {
                programListNodeFree(curr);
            }
        }
        curr = next;
    }
    _doEvents();
    intLibUpdate();
}

// 0x46E238
void programListFree()
{
    ProgramListNode* curr = gInterpreterProgramListHead;
    while (curr != nullptr) {
        ProgramListNode* next = curr->next;
        programListNodeFree(curr);
        curr = next;
    }
}

// 0x46E368
void interpreterRegisterOpcode(int opcode, OpcodeHandler* handler)
{
    int index = opcode & 0x3FFF;
    if (index >= OPCODE_MAX_COUNT) {
        printf("Too many opcodes!\n");
        exit(1);
    }

    gInterpreterOpcodeHandlers[index] = handler;
}

// 0x46E5EC
static void interpreterPrintStats()
{
    ProgramListNode* programListNode = gInterpreterProgramListHead;
    while (programListNode != nullptr) {
        Program* program = programListNode->program;
        if (program != nullptr) {
            int total = 0;

            if (program->dynamicStrings != nullptr) {
                debugPrint("Program %s\n");

                unsigned char* heap = program->dynamicStrings + sizeof(int);
                while (*(unsigned short*)heap != 0x8000) {
                    int size = *(short*)heap;
                    if (size >= 0) {
                        int refcount = *(short*)(heap + sizeof(short));
                        debugPrint("Size: %d, ref: %d, string %s\n", size, refcount, (char*)(heap + sizeof(short) + sizeof(short)));
                    } else {
                        debugPrint("Free space, length %d\n", -size);
                    }

                    // TODO: Not sure about total, probably calculated wrong, check.
                    heap += sizeof(short) + sizeof(short) + size;
                    total += sizeof(short) + sizeof(short) + size;
                }

                debugPrint("Total length of heap %d, stored length %d\n", total, *(int*)(program->dynamicStrings));
            } else {
                debugPrint("No string heap for program %s\n", program->name);
            }
        }

        programListNode = programListNode->next;
    }
}

void programStackPushValue(Program* program, ProgramValue& programValue)
{
    if (program->stackValues->size() > 0x1000) {
        programFatalError("programStackPushValue: Stack overflow.");
    }

    program->stackValues->push_back(programValue);

    if (programValue.opcode == VALUE_TYPE_DYNAMIC_STRING) {
        // NOTE: Uninline.
        _interpretIncStringRef(program, VALUE_TYPE_DYNAMIC_STRING, programValue.integerValue);
    }
}

void programStackPushInteger(Program* program, int value)
{
    ProgramValue programValue;
    programValue.opcode = VALUE_TYPE_INT;
    programValue.integerValue = value;
    programStackPushValue(program, programValue);
}

void programStackPushFloat(Program* program, float value)
{
    ProgramValue programValue;
    programValue.opcode = VALUE_TYPE_FLOAT;
    programValue.floatValue = value;
    programStackPushValue(program, programValue);
}

void programStackPushString(Program* program, char* value)
{
    ProgramValue programValue;
    programValue.opcode = VALUE_TYPE_DYNAMIC_STRING;
    programValue.integerValue = programPushString(program, value);
    programStackPushValue(program, programValue);
}

void programStackPushPointer(Program* program, void* value)
{
    ProgramValue programValue;
    programValue.opcode = VALUE_TYPE_PTR;
    programValue.pointerValue = value;
    programStackPushValue(program, programValue);
}

ProgramValue programStackPopValue(Program* program)
{
    if (program->stackValues->empty()) {
        programFatalError("programStackPopValue: Stack underflow.");
    }

    ProgramValue programValue = program->stackValues->back();
    program->stackValues->pop_back();

    if (programValue.opcode == VALUE_TYPE_DYNAMIC_STRING) {
        _interpretDecStringRef(program, programValue.opcode, programValue.integerValue);
    }

    return programValue;
}

int programStackPopInteger(Program* program)
{
    ProgramValue programValue = programStackPopValue(program);
    if (programValue.opcode != VALUE_TYPE_INT) {
        programFatalError("integer expected, got %x", programValue.opcode);
    }
    return programValue.integerValue;
}

float programStackPopFloat(Program* program)
{
    ProgramValue programValue = programStackPopValue(program);
    if (programValue.opcode != VALUE_TYPE_INT) {
        programFatalError("float expected, got %x", programValue.opcode);
    }
    return programValue.floatValue;
}

char* programStackPopString(Program* program)
{
    ProgramValue programValue = programStackPopValue(program);
    if ((programValue.opcode & VALUE_TYPE_MASK) != VALUE_TYPE_STRING) {
        programFatalError("string expected, got %x", programValue.opcode);
    }
    return programGetString(program, programValue.opcode, programValue.integerValue);
}

void* programStackPopPointer(Program* program)
{
    ProgramValue programValue = programStackPopValue(program);

    // There are certain places in the scripted code where they refer to
    // uninitialized exported variables designed to hold objects (pointers).
    // If this is one theses places simply return NULL.
    if (programValue.opcode == VALUE_TYPE_INT && programValue.integerValue == 0) {
        return nullptr;
    }

    if (programValue.opcode != VALUE_TYPE_PTR) {
        programFatalError("pointer expected, got %x", programValue.opcode);
    }
    return programValue.pointerValue;
}

void programReturnStackPushValue(Program* program, ProgramValue& programValue)
{
    if (program->returnStackValues->size() > 0x1000) {
        programFatalError("programReturnStackPushValue: Stack overflow.");
    }

    program->returnStackValues->push_back(programValue);

    if (programValue.opcode == VALUE_TYPE_DYNAMIC_STRING) {
        // NOTE: Uninline.
        _interpretIncStringRef(program, VALUE_TYPE_DYNAMIC_STRING, programValue.integerValue);
    }
}

void programReturnStackPushInteger(Program* program, int value)
{
    ProgramValue programValue;
    programValue.opcode = VALUE_TYPE_INT;
    programValue.integerValue = value;
    programReturnStackPushValue(program, programValue);
}

void programReturnStackPushPointer(Program* program, void* value)
{
    ProgramValue programValue;
    programValue.opcode = VALUE_TYPE_PTR;
    programValue.pointerValue = value;
    programReturnStackPushValue(program, programValue);
}

ProgramValue programReturnStackPopValue(Program* program)
{
    if (program->returnStackValues->empty()) {
        programFatalError("programReturnStackPopValue: Stack underflow.");
    }

    ProgramValue programValue = program->returnStackValues->back();
    program->returnStackValues->pop_back();

    if (programValue.opcode == VALUE_TYPE_DYNAMIC_STRING) {
        _interpretDecStringRef(program, programValue.opcode, programValue.integerValue);
    }

    return programValue;
}

int programReturnStackPopInteger(Program* program)
{
    ProgramValue programValue = programReturnStackPopValue(program);
    return programValue.integerValue;
}

void* programReturnStackPopPointer(Program* program)
{
    ProgramValue programValue = programReturnStackPopValue(program);
    return programValue.pointerValue;
}

bool ProgramValue::isEmpty() const
{
    switch (opcode) {
    case VALUE_TYPE_INT:
    case VALUE_TYPE_STRING:
        return integerValue == 0;
    case VALUE_TYPE_FLOAT:
        return floatValue == 0.0;
    case VALUE_TYPE_PTR:
        return pointerValue == nullptr;
    }

    // Should be unreachable.
    return true;
}

// Matches Sfall implementation.
bool ProgramValue::isInt() const
{
    return opcode == VALUE_TYPE_INT;
}

// Matches Sfall implementation.
bool ProgramValue::isFloat() const
{
    return opcode == VALUE_TYPE_FLOAT;
}

// Matches Sfall implementation.
float ProgramValue::asFloat() const
{
    switch (opcode) {
    case VALUE_TYPE_INT:
        return static_cast<float>(integerValue);
    case VALUE_TYPE_FLOAT:
        return floatValue;
    default:
        return 0.0;
    }
}

bool ProgramValue::isString() const
{
    return opcode == VALUE_TYPE_STRING || opcode == VALUE_TYPE_DYNAMIC_STRING;
}

ProgramValue::ProgramValue()
{
    opcode = VALUE_TYPE_INT;
    integerValue = 0;
}
ProgramValue::ProgramValue(int value)
{
    opcode = VALUE_TYPE_INT;
    integerValue = value;
};
ProgramValue::ProgramValue(Object* value)
{
    opcode = VALUE_TYPE_PTR;
    pointerValue = value;
};

bool ProgramValue::isPointer() const
{
    return opcode == VALUE_TYPE_PTR;
}

int ProgramValue::asInt() const
{
    switch (opcode) {
    case VALUE_TYPE_INT:
        return integerValue;
    case VALUE_TYPE_FLOAT:
        return static_cast<int>(floatValue);
    default:
        return 0;
    }
}

} // namespace fallout
