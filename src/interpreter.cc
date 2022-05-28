#include "interpreter.h"

#include "core.h"
#include "db.h"
#include "debug.h"
#include "export.h"
#include "interpreter_lib.h"
#include "memory_manager.h"
#include "platform_compat.h"

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 0x50942C
char _aCouldnTFindPro[] = "<couldn't find proc>";

// sayTimeoutMsg
// 0x519038
int _TimeOut = 0;

// 0x51903C
int _Enabled = 1;

// 0x519040
int (*_timerFunc)() = _defaultTimerFunc;

// 0x519044
int _timerTick = 1000;

// 0x519048
char* (*_filenameFunc)(char*) = _defaultFilename_;

// 0x51904C
int (*_outputFunc)(char*) = _outputStr;

// 0x519050
int _cpuBurstSize = 10;

// 0x59E230
OpcodeHandler* gInterpreterOpcodeHandlers[342];

// 0x59E78C
Program* gInterpreterCurrentProgram;

// 0x59E790
ProgramListNode* gInterpreterProgramListHead;
int _suspendEvents;
int _busy;

// 0x4670A0
int _defaultTimerFunc()
{
    return _get_time();
}

// 0x4670B4
char* _defaultFilename_(char* s)
{
    return s;
}

// 0x4670B8
char* _interpretMangleName(char* s)
{
    return _filenameFunc(s);
}

// 0x4670C0
int _outputStr(char* a1)
{
    return 1;
}

// 0x4670C8
int _checkWait(Program* program)
{
    return 1000 * _timerFunc() / _timerTick <= program->field_70;
}

// 0x4670FC
void _interpretOutputFunc(int (*func)(char*))
{
    _outputFunc = func;
}

// 0x467104
int _interpretOutput(const char* format, ...)
{
    if (_outputFunc == NULL) {
        return 0;
    }

    char string[260];

    va_list args;
    va_start(args, format);
    int rc = vsprintf(string, format, args);
    va_end(args);

    debugPrint(string);

    return rc;
}

// 0x467160
char* programGetCurrentProcedureName(Program* program)
{
    int procedureCount = stackReadInt32(program->procedures, 0);
    unsigned char* ptr = program->procedures + 4;

    int procedureOffset = stackReadInt32(ptr, 16);
    int identifierOffset = stackReadInt32(ptr, 0);

    for (int index = 0; index < procedureCount; index++) {
        int nextProcedureOffset = stackReadInt32(ptr + 24, 16);
        if (program->instructionPointer >= procedureOffset && program->instructionPointer < nextProcedureOffset) {
            return (char*)(program->identifiers + identifierOffset);
        }

        ptr += 24;
        identifierOffset = stackReadInt32(ptr, 0);
    }

    return _aCouldnTFindPro;
}

// 0x4671F0
[[noreturn]] void programFatalError(const char* format, ...)
{
    char string[260];

    va_list argptr;
    va_start(argptr, format);
    vsprintf(string, format, argptr);
    va_end(argptr);

    debugPrint("\nError during execution: %s\n", string);

    if (gInterpreterCurrentProgram == NULL) {
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
opcode_t stackReadInt16(unsigned char* data, int pos)
{
    // TODO: The return result is probably short.
    opcode_t value = 0;
    value |= data[pos++] << 8;
    value |= data[pos++];
    return value;
}

// 0x4672A4
int stackReadInt32(unsigned char* data, int pos)
{
    int value = 0;
    value |= data[pos++] << 24;
    value |= data[pos++] << 16;
    value |= data[pos++] << 8;
    value |= data[pos++] & 0xFF;

    return value;
}

// 0x4672D4
void stackWriteInt16(int value, unsigned char* stack, int pos)
{
    stack[pos++] = (value >> 8) & 0xFF;
    stack[pos] = value & 0xFF;
}

// NOTE: Inlined.
//
// 0x4672E8
void stackWriteInt32(int value, unsigned char* stack, int pos)
{
    stack[pos++] = (value >> 24) & 0xFF;
    stack[pos++] = (value >> 16) & 0xFF;
    stack[pos++] = (value >> 8) & 0xFF;
    stack[pos] = value & 0xFF;
}

// pushShortStack
// 0x467324
void stackPushInt16(unsigned char* data, int* pointer, int value)
{
    if (*pointer + 2 >= 0x1000) {
        programFatalError("pushShortStack: Stack overflow.");
    }

    stackWriteInt16(value, data, *pointer);

    *pointer += 2;
}

// pushLongStack
// 0x46736C
void stackPushInt32(unsigned char* data, int* pointer, int value)
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
int stackPopInt32(unsigned char* data, int* pointer)
{
    if (*pointer < 4) {
        programFatalError("\nStack underflow long.");
    }

    *pointer -= 4;

    return stackReadInt32(data, *pointer);
}

// popStackShort
// 0x4673F0
opcode_t stackPopInt16(unsigned char* data, int* pointer)
{
    if (*pointer < 2) {
        programFatalError("\nStack underflow short.");
    }

    *pointer -= 2;

    // NOTE: uninline
    return stackReadInt16(data, *pointer);
}

// 0x467440
void programPopString(Program* program, opcode_t opcode, int value)
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

// 0x46748C
void programStackPushInt16(Program* program, int value)
{
    int v1, v2;
    unsigned char* v3;

    stackPushInt16(program->stack, &(program->stackPointer), value);

    if (value == VALUE_TYPE_DYNAMIC_STRING) {
        v1 = program->stackPointer;
        if (v1 >= 6) {
            v2 = stackReadInt32(program->stack, v1 - 6);
            v3 = program->dynamicStrings + 4 + v2 - 2;
            *(short*)v3 = *(short*)v3 + 1;
        }
    }
}

// 0x4674DC
void programStackPushInt32(Program* program, int value)
{
    stackPushInt32(program->stack, &(program->stackPointer), value);
}

// 0x4674F0
opcode_t programStackPopInt16(Program* program)
{
    return stackPopInt16(program->stack, &(program->stackPointer));
}

// 0x467500
int programStackPopInt32(Program* program)
{
    return stackPopInt32(program->stack, &(program->stackPointer));
}

// 0x467510
void programReturnStackPushInt16(Program* program, int value)
{
    stackPushInt16(program->returnStack, &(program->returnStackPointer), value);

    if (value == VALUE_TYPE_DYNAMIC_STRING && program->stackPointer >= 6) {
        int v4 = stackReadInt32(program->returnStack, program->returnStackPointer - 6);
        *(short*)(program->dynamicStrings + 4 + v4 - 2) += 1;
    }
}

// 0x467574
opcode_t programReturnStackPopInt16(Program* program)
{
    opcode_t type;
    int v5;

    type = stackPopInt16(program->returnStack, &(program->returnStackPointer));
    if (type == VALUE_TYPE_DYNAMIC_STRING && program->stackPointer >= 4) {
        v5 = stackReadInt32(program->returnStack, program->returnStackPointer - 4);
        programPopString(program, type, v5);
    }

    return type;
}

// 0x4675B8
int programReturnStackPopInt32(Program* program)
{
    return stackPopInt32(program->returnStack, &(program->returnStackPointer));
}

// NOTE: Inlined.
//
// 0x4675C8
void _detachProgram(Program* program)
{
    Program* parent = program->parent;
    if (parent != NULL) {
        parent->flags &= ~PROGRAM_FLAG_0x20;
        parent->flags &= ~PROGRAM_FLAG_0x0100;
        if (program == parent->child) {
            parent->child = NULL;
        }
    }
}

// 0x4675F4
void _purgeProgram(Program* program)
{
    if (!program->exited) {
        _removeProgramReferences_(program);
        program->exited = true;
    }
}

// 0x467614
void programFree(Program* program)
{
    // NOTE: Uninline.
    _detachProgram(program);

    Program* curr = program->child;
    while (curr != NULL) {
        // NOTE: Uninline.
        _purgeProgram(curr);

        curr->parent = NULL;

        Program* next = curr->child;
        curr->child = NULL;

        curr = next;
    }

    // NOTE: Uninline.
    _purgeProgram(program);

    if (program->dynamicStrings != NULL) {
        internal_free_safe(program->dynamicStrings, __FILE__, __LINE__); // "..\\int\\INTRPRET.C", 429
    }

    if (program->data != NULL) {
        internal_free_safe(program->data, __FILE__, __LINE__); // "..\\int\\INTRPRET.C", 430
    }

    if (program->name != NULL) {
        internal_free_safe(program->name, __FILE__, __LINE__); // "..\\int\\INTRPRET.C", 431
    }

    if (program->stack != NULL) {
        internal_free_safe(program->stack, __FILE__, __LINE__); // "..\\int\\INTRPRET.C", 432
    }

    if (program->returnStack != NULL) {
        internal_free_safe(program->returnStack, __FILE__, __LINE__); // "..\\int\\INTRPRET.C", 433
    }

    internal_free_safe(program, __FILE__, __LINE__); // "..\\int\\INTRPRET.C", 435
}

// 0x467734
Program* programCreateByPath(const char* path)
{
    File* stream = fileOpen(path, "rb");
    if (stream == NULL) {
        char err[260];
        sprintf(err, "Couldn't open %s for read\n", path);
        programFatalError(err);
        return NULL;
    }

    int fileSize = fileGetSize(stream);
    unsigned char* data = (unsigned char*)internal_malloc_safe(fileSize, __FILE__, __LINE__); // ..\\int\\INTRPRET.C, 458

    fileRead(data, 1, fileSize, stream);
    fileClose(stream);

    Program* program = (Program*)internal_malloc_safe(sizeof(Program), __FILE__, __LINE__); // ..\\int\\INTRPRET.C, 463
    memset(program, 0, sizeof(Program));

    program->name = (char*)internal_malloc_safe(strlen(path) + 1, __FILE__, __LINE__); // ..\\int\\INTRPRET.C, 466
    strcpy(program->name, path);

    program->child = NULL;
    program->parent = NULL;
    program->field_78 = -1;
    program->stack = (unsigned char*)internal_calloc_safe(1, 4096, __FILE__, __LINE__); // ..\\int\\INTRPRET.C, 472
    program->exited = false;
    program->basePointer = -1;
    program->framePointer = -1;
    program->returnStack = (unsigned char*)internal_calloc_safe(1, 4096, __FILE__, __LINE__); // ..\\int\\INTRPRET.C, 473
    program->data = data;
    program->procedures = data + 42;
    program->identifiers = 24 * stackReadInt32(program->procedures, 0) + program->procedures + 4;
    program->staticStrings = program->identifiers + stackReadInt32(program->identifiers, 0) + 4;

    return program;
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

    return NULL;
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
void programMarkHeap(Program* program)
{
    unsigned char* ptr;
    short len;
    unsigned char* next_ptr;
    short next_len;
    short diff;

    if (program->dynamicStrings == NULL) {
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

    if (program == NULL) {
        return 0;
    }

    v27 = strlen(string) + 1;

    // Align memory
    if (v27 & 1) {
        v27++;
    }

    if (program->dynamicStrings != NULL) {
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
        *(short*)(program->dynamicStrings + 4) = 0x8000;
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
    *(short*)(v23 + 4) = 0x8000;
    *(short*)(v23 + 6) = 1;

    return v20 + 4 - (program->dynamicStrings + 4);
}

// 0x467C90
void opNoop(Program* program)
{
}

// 0x467C94
void opPush(Program* program)
{
    int pos = program->instructionPointer;
    program->instructionPointer = pos + 4;

    int value = stackReadInt32(program->data, pos);
    stackPushInt32(program->stack, &(program->stackPointer), value);
    programStackPushInt16(program, (program->flags >> 16) & 0xFFFF);
}

// - Pops value from stack, which is a number of arguments in the procedure.
// - Saves current frame pointer in return stack.
// - Sets frame pointer to the stack pointer minus number of arguments.
//
// 0x467CD0
void opPushBase(Program* program)
{
    opcode_t opcode = stackPopInt16(program->stack, &(program->stackPointer));
    int value = stackPopInt32(program->stack, &(program->stackPointer));

    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, value);
    }

    stackPushInt32(program->returnStack, &(program->returnStackPointer), program->framePointer);
    programReturnStackPushInt16(program, VALUE_TYPE_INT);

    program->framePointer = program->stackPointer - 6 * value;
}

// pop_base
// 0x467D3C
void opPopBase(Program* program)
{
    opcode_t opcode = programReturnStackPopInt16(program);
    int data = stackPopInt32(program->returnStack, &(program->returnStackPointer));

    if (opcode != VALUE_TYPE_INT) {
        char err[260];
        sprintf(err, "Invalid type given to pop_base: %x", opcode);
        programFatalError(err);
    }

    program->framePointer = data;
}

// 0x467D94
void opPopToBase(Program* program)
{
    while (program->stackPointer != program->framePointer) {
        opcode_t opcode = stackPopInt16(program->stack, &(program->stackPointer));
        int data = stackPopInt32(program->stack, &(program->stackPointer));

        if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode, data);
        }
    }
}

// 0x467DE0
void op802C(Program* program)
{
    program->basePointer = program->stackPointer;
}

// 0x467DEC
void opDump(Program* program)
{
    opcode_t opcode = stackPopInt16(program->stack, &(program->stackPointer));
    int data = stackPopInt32(program->stack, &(program->stackPointer));

    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    if (opcode != VALUE_TYPE_INT) {
        char err[256];
        sprintf(err, "Invalid type given to dump, %x", opcode);
        programFatalError(err);
    }

    // NOTE: Original code is slightly different - it goes backwards to -1.
    for (int index = 0; index < data; index++) {
        opcode = stackPopInt16(program->stack, &(program->stackPointer));
        data = stackPopInt32(program->stack, &(program->stackPointer));

        if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode, data);
        }
    }
}

// 0x467EA4
void opDelayedCall(Program* program)
{
    opcode_t opcode[2];
    int data[2];

    for (int arg = 0; arg < 2; arg++) {
        opcode[arg] = stackPopInt16(program->stack, &(program->stackPointer));
        data[arg] = stackPopInt32(program->stack, &(program->stackPointer));

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }

        if (arg == 0) {
            if ((opcode[arg] & 0xF7FF) != VALUE_TYPE_INT) {
                programFatalError("Invalid procedure type given to call");
            }
        } else if (arg == 1) {
            if ((opcode[arg] & 0xF7FF) != VALUE_TYPE_INT) {
                programFatalError("Invalid time given to call");
            }
        }
    }

    unsigned char* procedure_ptr = program->procedures + 4 + 24 * data[0];

    int delay = 1000 * data[1];

    if (!_suspendEvents) {
        delay += 1000 * _timerFunc() / _timerTick;
    }

    int flags = stackReadInt32(procedure_ptr, 4);

    stackWriteInt32(delay, procedure_ptr, 8);
    stackWriteInt32(flags | PROCEDURE_FLAG_TIMED, procedure_ptr, 4);
}

// 0x468034
void opConditionalCall(Program* program)
{
    opcode_t opcode[2];
    int data[2];

    for (int arg = 0; arg < 2; arg++) {
        opcode[arg] = stackPopInt16(program->stack, &(program->stackPointer));
        data[arg] = stackPopInt32(program->stack, &(program->stackPointer));

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }
    }

    if ((opcode[0] & 0xF7FF) != VALUE_TYPE_INT) {
        programFatalError("Invalid procedure type given to conditional call");
    }

    if ((opcode[1] & 0xF7FF) != VALUE_TYPE_INT) {
        programFatalError("Invalid address given to conditional call");
    }

    unsigned char* procedure_ptr = program->procedures + 4 + 24 * data[0];
    int flags = stackReadInt32(procedure_ptr, 4);

    stackWriteInt32(flags | PROCEDURE_FLAG_CONDITIONAL, procedure_ptr, 4);
    stackWriteInt32(data[1], procedure_ptr, 12);
}

// 0x46817C
void opWait(Program* program)
{
    opcode_t opcode = programStackPopInt16(program);
    int data = programStackPopInt32(program);

    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    if ((opcode & 0xF7FF) != VALUE_TYPE_INT) {
        programFatalError("Invalid type given to wait\n");
    }

    program->field_74 = 1000 * _timerFunc() / _timerTick;
    program->field_70 = program->field_74 + data;
    program->field_7C = _checkWait;
    program->flags |= PROGRAM_FLAG_0x10;
}

// 0x468218
void opCancel(Program* program)
{
    opcode_t opcode = programStackPopInt16(program);
    int data = programStackPopInt32(program);

    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    if ((opcode & 0xF7FF) != VALUE_TYPE_INT) {
        programFatalError("invalid type given to cancel");
    }

    if (data >= stackReadInt32(program->procedures, 0)) {
        programFatalError("Invalid procedure offset given to cancel");
    }

    Procedure* proc = (Procedure*)(program->procedures + 4 + data * sizeof(*proc));
    proc->field_4 = 0;
    proc->field_8 = 0;
    proc->field_C = 0;
}

// 0x468330
void opCancelAll(Program* program)
{
    int procedureCount = stackReadInt32(program->procedures, 0);

    for (int index = 0; index < procedureCount; index++) {
        // TODO: Original code uses different approach, check.
        Procedure* proc = (Procedure*)(program->procedures + 4 + index * sizeof(*proc));

        proc->field_4 = 0;
        proc->field_8 = 0;
        proc->field_C = 0;
    }
}

// 0x468400
void opIf(Program* program)
{
    opcode_t opcode = stackPopInt16(program->stack, &(program->stackPointer));
    int data = stackPopInt32(program->stack, &(program->stackPointer));
    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    if (data) {
        opcode = stackPopInt16(program->stack, &(program->stackPointer));
        data = stackPopInt32(program->stack, &(program->stackPointer));
        if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode, data);
        }
    } else {
        opcode = stackPopInt16(program->stack, &(program->stackPointer));
        data = stackPopInt32(program->stack, &(program->stackPointer));
        if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode, data);
        }

        program->instructionPointer = data;
    }
}

// 0x4684A4
void opWhile(Program* program)
{
    opcode_t opcode = stackPopInt16(program->stack, &(program->stackPointer));
    int data = stackPopInt32(program->stack, &(program->stackPointer));

    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    if (data == 0) {
        opcode = stackPopInt16(program->stack, &(program->stackPointer));
        data = stackPopInt32(program->stack, &(program->stackPointer));

        if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode, data);
        }

        program->instructionPointer = data;
    }
}

// 0x468518
void opStore(Program* program)
{
    opcode_t opcode[2];
    int data[2];

    // NOTE: Original code does not use loop.
    for (int arg = 0; arg < 2; arg++) {
        opcode[arg] = stackPopInt16(program->stack, &(program->stackPointer));
        data[arg] = stackPopInt32(program->stack, &(program->stackPointer));

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }
    }

    int var_address = program->framePointer + 6 * data[0];

    // NOTE: original code is different, does not use reading functions
    opcode_t var_type = stackReadInt16(program->stack, var_address + 4);
    int var_value = stackReadInt32(program->stack, var_address);

    if (var_type == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, var_type, var_value);
    }

    // TODO: Original code is different, check.
    stackWriteInt32(data[1], program->stack, var_address);

    stackWriteInt16(opcode[1], program->stack, var_address + 4);

    if (opcode[1] == VALUE_TYPE_DYNAMIC_STRING) {
        // increment ref count
        *(short*)(program->dynamicStrings + 4 - 2 + data[1]) += 1;
    }
}

// fetch
// 0x468678
void opFetch(Program* program)
{
    char err[256];

    opcode_t opcode = stackPopInt16(program->stack, &(program->stackPointer));
    int data = stackPopInt32(program->stack, &(program->stackPointer));
    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    if (opcode != VALUE_TYPE_INT) {
        sprintf(err, "Invalid type given to fetch, %x", opcode);
        programFatalError(err);
    }

    // NOTE: original code is a bit different
    int variableAddress = program->framePointer + 6 * data;
    int variableType = stackReadInt16(program->stack, variableAddress + 4);
    int variableValue = stackReadInt32(program->stack, variableAddress);
    programStackPushInt32(program, variableValue);
    programStackPushInt16(program, variableType);
}

// 0x46873C
void opConditionalOperatorNotEqual(Program* program)
{
    opcode_t opcode[2];
    int data[2];
    float* floats = (float*)data;
    char text[2][80];
    char* str_ptr[2];
    int res;

    // NOTE: Original code does not use loop.
    for (int arg = 0; arg < 2; arg++) {
        opcode[arg] = stackPopInt16(program->stack, &(program->stackPointer));
        data[arg] = stackPopInt32(program->stack, &(program->stackPointer));

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }
    }

    switch (opcode[1]) {
    case VALUE_TYPE_STRING:
    case VALUE_TYPE_DYNAMIC_STRING:
        str_ptr[1] = programGetString(program, opcode[1], data[1]);

        switch (opcode[0]) {
        case VALUE_TYPE_STRING:
        case VALUE_TYPE_DYNAMIC_STRING:
            str_ptr[0] = programGetString(program, opcode[0], data[0]);
            break;
        case VALUE_TYPE_FLOAT:
            sprintf(text[0], "%.5f", floats[0]);
            str_ptr[0] = text[0];
            break;
        case VALUE_TYPE_INT:
            sprintf(text[0], "%d", data[0]);
            str_ptr[0] = text[0];
            break;
        default:
            assert(false && "Should be unreachable");
        }

        res = strcmp(str_ptr[1], str_ptr[0]) != 0;
        break;
    case VALUE_TYPE_FLOAT:
        switch (opcode[0]) {
        case VALUE_TYPE_STRING:
        case VALUE_TYPE_DYNAMIC_STRING:
            sprintf(text[1], "%.5f", floats[1]);
            str_ptr[1] = text[1];
            str_ptr[0] = programGetString(program, opcode[0], data[0]);
            res = strcmp(str_ptr[1], str_ptr[0]) != 0;
            break;
        case VALUE_TYPE_FLOAT:
            res = floats[1] != floats[0];
            break;
        case VALUE_TYPE_INT:
            res = floats[1] != (float)data[0];
            break;
        default:
            assert(false && "Should be unreachable");
        }
        break;
    case VALUE_TYPE_INT:
        switch (opcode[0]) {
        case VALUE_TYPE_STRING:
        case VALUE_TYPE_DYNAMIC_STRING:
            sprintf(text[1], "%d", data[1]);
            str_ptr[1] = text[1];
            str_ptr[0] = programGetString(program, opcode[0], data[0]);
            res = strcmp(str_ptr[1], str_ptr[0]) != 0;
            break;
        case VALUE_TYPE_FLOAT:
            res = (float)data[1] != floats[0];
            break;
        case VALUE_TYPE_INT:
            res = data[1] != data[0];
            break;
        default:
            assert(false && "Should be unreachable");
        }
        break;
    default:
        assert(false && "Should be unreachable");
    }

    stackPushInt32(program->stack, &(program->stackPointer), res);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// 0x468AA8
void opConditionalOperatorEqual(Program* program)
{
    int arg;
    opcode_t type[2];
    int value[2];
    float* floats = (float*)&value;
    char text[2][80];
    char* str_ptr[2];
    int res;

    for (arg = 0; arg < 2; arg++) {
        type[arg] = stackPopInt16(program->stack, &(program->stackPointer));
        value[arg] = stackPopInt32(program->stack, &(program->stackPointer));

        if (type[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, type[arg], value[arg]);
        }
    }

    switch (type[1]) {
    case VALUE_TYPE_STRING:
    case VALUE_TYPE_DYNAMIC_STRING:
        str_ptr[1] = programGetString(program, type[1], value[1]);

        switch (type[0]) {
        case VALUE_TYPE_STRING:
        case VALUE_TYPE_DYNAMIC_STRING:
            str_ptr[0] = programGetString(program, type[0], value[0]);
            break;
        case VALUE_TYPE_FLOAT:
            sprintf(text[0], "%.5f", floats[0]);
            str_ptr[0] = text[0];
            break;
        case VALUE_TYPE_INT:
            sprintf(text[0], "%d", value[0]);
            str_ptr[0] = text[0];
            break;
        default:
            assert(false && "Should be unreachable");
        }

        res = strcmp(str_ptr[1], str_ptr[0]) == 0;
        break;
    case VALUE_TYPE_FLOAT:
        switch (type[0]) {
        case VALUE_TYPE_STRING:
        case VALUE_TYPE_DYNAMIC_STRING:
            sprintf(text[1], "%.5f", floats[1]);
            str_ptr[1] = text[1];
            str_ptr[0] = programGetString(program, type[0], value[0]);
            res = strcmp(str_ptr[1], str_ptr[0]) == 0;
            break;
        case VALUE_TYPE_FLOAT:
            res = floats[1] == floats[0];
            break;
        case VALUE_TYPE_INT:
            res = floats[1] == (float)value[0];
            break;
        default:
            assert(false && "Should be unreachable");
        }
        break;
    case VALUE_TYPE_INT:
        switch (type[0]) {
        case VALUE_TYPE_STRING:
        case VALUE_TYPE_DYNAMIC_STRING:
            sprintf(text[1], "%d", value[1]);
            str_ptr[1] = text[1];
            str_ptr[0] = programGetString(program, type[0], value[0]);
            res = strcmp(str_ptr[1], str_ptr[0]) == 0;
            break;
        case VALUE_TYPE_FLOAT:
            res = (float)value[1] == floats[0];
            break;
        case VALUE_TYPE_INT:
            res = value[1] == value[0];
            break;
        default:
            assert(false && "Should be unreachable");
        }
        break;
    default:
        assert(false && "Should be unreachable");
    }

    stackPushInt32(program->stack, &(program->stackPointer), res);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// 0x468E14
void opConditionalOperatorLessThanEquals(Program* program)
{
    int arg;
    opcode_t type[2];
    int value[2];
    float* floats = (float*)&value;
    char text[2][80];
    char* str_ptr[2];
    int res;

    for (arg = 0; arg < 2; arg++) {
        type[arg] = stackPopInt16(program->stack, &(program->stackPointer));
        value[arg] = stackPopInt32(program->stack, &(program->stackPointer));

        if (type[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, type[arg], value[arg]);
        }
    }

    switch (type[1]) {
    case VALUE_TYPE_STRING:
    case VALUE_TYPE_DYNAMIC_STRING:
        str_ptr[1] = programGetString(program, type[1], value[1]);

        switch (type[0]) {
        case VALUE_TYPE_STRING:
        case VALUE_TYPE_DYNAMIC_STRING:
            str_ptr[0] = programGetString(program, type[0], value[0]);
            break;
        case VALUE_TYPE_FLOAT:
            sprintf(text[0], "%.5f", floats[0]);
            str_ptr[0] = text[0];
            break;
        case VALUE_TYPE_INT:
            sprintf(text[0], "%d", value[0]);
            str_ptr[0] = text[0];
            break;
        default:
            assert(false && "Should be unreachable");
        }

        res = strcmp(str_ptr[1], str_ptr[0]) <= 0;
        break;
    case VALUE_TYPE_FLOAT:
        switch (type[0]) {
        case VALUE_TYPE_STRING:
        case VALUE_TYPE_DYNAMIC_STRING:
            sprintf(text[1], "%.5f", floats[1]);
            str_ptr[1] = text[1];
            str_ptr[0] = programGetString(program, type[0], value[0]);
            res = strcmp(str_ptr[1], str_ptr[0]) <= 0;
            break;
        case VALUE_TYPE_FLOAT:
            res = floats[1] <= floats[0];
            break;
        case VALUE_TYPE_INT:
            res = floats[1] <= (float)value[0];
            break;
        default:
            assert(false && "Should be unreachable");
        }
        break;
    case VALUE_TYPE_INT:
        switch (type[0]) {
        case VALUE_TYPE_STRING:
        case VALUE_TYPE_DYNAMIC_STRING:
            sprintf(text[1], "%d", value[1]);
            str_ptr[1] = text[1];
            str_ptr[0] = programGetString(program, type[0], value[0]);
            res = strcmp(str_ptr[1], str_ptr[0]) <= 0;
            break;
        case VALUE_TYPE_FLOAT:
            res = (float)value[1] <= floats[0];
            break;
        case VALUE_TYPE_INT:
            res = value[1] <= value[0];
            break;
        default:
            assert(false && "Should be unreachable");
        }
        break;
    default:
        assert(false && "Should be unreachable");
    }

    stackPushInt32(program->stack, &(program->stackPointer), res);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// 0x469180
void opConditionalOperatorGreaterThanEquals(Program* program)
{
    int arg;
    opcode_t type[2];
    int value[2];
    float* floats = (float*)&value;
    char text[2][80];
    char* str_ptr[2];
    int res;

    // NOTE: original code does not use loop
    for (arg = 0; arg < 2; arg++) {
        type[arg] = stackPopInt16(program->stack, &(program->stackPointer));
        value[arg] = stackPopInt32(program->stack, &(program->stackPointer));

        if (type[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, type[arg], value[arg]);
        }
    }

    switch (type[1]) {
    case VALUE_TYPE_STRING:
    case VALUE_TYPE_DYNAMIC_STRING:
        str_ptr[1] = programGetString(program, type[1], value[1]);

        switch (type[0]) {
        case VALUE_TYPE_STRING:
        case VALUE_TYPE_DYNAMIC_STRING:
            str_ptr[0] = programGetString(program, type[0], value[0]);
            break;
        case VALUE_TYPE_FLOAT:
            sprintf(text[0], "%.5f", floats[0]);
            str_ptr[0] = text[0];
            break;
        case VALUE_TYPE_INT:
            sprintf(text[0], "%d", value[0]);
            str_ptr[0] = text[0];
            break;
        default:
            assert(false && "Should be unreachable");
        }

        res = strcmp(str_ptr[1], str_ptr[0]) >= 0;
        break;
    case VALUE_TYPE_FLOAT:
        switch (type[0]) {
        case VALUE_TYPE_STRING:
        case VALUE_TYPE_DYNAMIC_STRING:
            sprintf(text[1], "%.5f", floats[1]);
            str_ptr[1] = text[1];
            str_ptr[0] = programGetString(program, type[0], value[0]);
            res = strcmp(str_ptr[1], str_ptr[0]) >= 0;
            break;
        case VALUE_TYPE_FLOAT:
            res = floats[1] >= floats[0];
            break;
        case VALUE_TYPE_INT:
            res = floats[1] >= (float)value[0];
            break;
        default:
            assert(false && "Should be unreachable");
        }
        break;
    case VALUE_TYPE_INT:
        switch (type[0]) {
        case VALUE_TYPE_STRING:
        case VALUE_TYPE_DYNAMIC_STRING:
            sprintf(text[1], "%d", value[1]);
            str_ptr[1] = text[1];
            str_ptr[0] = programGetString(program, type[0], value[0]);
            res = strcmp(str_ptr[1], str_ptr[0]) >= 0;
            break;
        case VALUE_TYPE_FLOAT:
            res = (float)value[1] >= floats[0];
            break;
        case VALUE_TYPE_INT:
            res = value[1] >= value[0];
            break;
        default:
            assert(false && "Should be unreachable");
        }
        break;
    default:
        assert(false && "Should be unreachable");
    }

    stackPushInt32(program->stack, &(program->stackPointer), res);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// 0x4694EC
void opConditionalOperatorLessThan(Program* program)
{
    opcode_t opcodes[2];
    int values[2];
    float* floats = (float*)&values;
    char text[2][80];
    char* str_ptr[2];
    int res;

    for (int arg = 0; arg < 2; arg++) {
        opcodes[arg] = stackPopInt16(program->stack, &(program->stackPointer));
        values[arg] = stackPopInt32(program->stack, &(program->stackPointer));

        if (opcodes[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcodes[arg], values[arg]);
        }
    }

    switch (opcodes[1]) {
    case VALUE_TYPE_STRING:
    case VALUE_TYPE_DYNAMIC_STRING:
        str_ptr[1] = programGetString(program, opcodes[1], values[1]);

        switch (opcodes[0]) {
        case VALUE_TYPE_STRING:
        case VALUE_TYPE_DYNAMIC_STRING:
            str_ptr[0] = programGetString(program, opcodes[0], values[0]);
            break;
        case VALUE_TYPE_FLOAT:
            sprintf(text[0], "%.5f", floats[0]);
            str_ptr[0] = text[0];
            break;
        case VALUE_TYPE_INT:
            sprintf(text[0], "%d", values[0]);
            str_ptr[0] = text[0];
            break;
        default:
            assert(false && "Should be unreachable");
        }

        res = strcmp(str_ptr[1], str_ptr[0]) < 0;
        break;
    case VALUE_TYPE_FLOAT:
        switch (opcodes[0]) {
        case VALUE_TYPE_STRING:
        case VALUE_TYPE_DYNAMIC_STRING:
            sprintf(text[1], "%.5f", floats[1]);
            str_ptr[1] = text[1];
            str_ptr[0] = programGetString(program, opcodes[0], values[0]);
            res = strcmp(str_ptr[1], str_ptr[0]) < 0;
            break;
        case VALUE_TYPE_FLOAT:
            res = floats[1] < floats[0];
            break;
        case VALUE_TYPE_INT:
            res = floats[1] < (float)values[0];
            break;
        default:
            assert(false && "Should be unreachable");
        }
        break;
    case VALUE_TYPE_INT:
        switch (opcodes[0]) {
        case VALUE_TYPE_STRING:
        case VALUE_TYPE_DYNAMIC_STRING:
            sprintf(text[1], "%d", values[1]);
            str_ptr[1] = text[1];
            str_ptr[0] = programGetString(program, opcodes[0], values[0]);
            res = strcmp(str_ptr[1], str_ptr[0]) < 0;
            break;
        case VALUE_TYPE_FLOAT:
            res = (float)values[1] < floats[0];
            break;
        case VALUE_TYPE_INT:
            res = values[1] < values[0];
            break;
        default:
            assert(false && "Should be unreachable");
        }
        break;
    default:
        assert(false && "Should be unreachable");
    }

    stackPushInt32(program->stack, &(program->stackPointer), res);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// 0x469858
void opConditionalOperatorGreaterThan(Program* program)
{
    int arg;
    opcode_t type[2];
    int value[2];
    float* floats = (float*)&value;
    char text[2][80];
    char* str_ptr[2];
    int res;

    for (arg = 0; arg < 2; arg++) {
        type[arg] = stackPopInt16(program->stack, &(program->stackPointer));
        value[arg] = stackPopInt32(program->stack, &(program->stackPointer));

        if (type[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, type[arg], value[arg]);
        }
    }

    switch (type[1]) {
    case VALUE_TYPE_STRING:
    case VALUE_TYPE_DYNAMIC_STRING:
        str_ptr[1] = programGetString(program, type[1], value[1]);

        switch (type[0]) {
        case VALUE_TYPE_STRING:
        case VALUE_TYPE_DYNAMIC_STRING:
            str_ptr[0] = programGetString(program, type[0], value[0]);
            break;
        case VALUE_TYPE_FLOAT:
            sprintf(text[0], "%.5f", floats[0]);
            str_ptr[0] = text[0];
            break;
        case VALUE_TYPE_INT:
            sprintf(text[0], "%d", value[0]);
            str_ptr[0] = text[0];
            break;
        default:
            assert(false && "Should be unreachable");
        }

        res = strcmp(str_ptr[1], str_ptr[0]) > 0;
        break;
    case VALUE_TYPE_FLOAT:
        switch (type[0]) {
        case VALUE_TYPE_STRING:
        case VALUE_TYPE_DYNAMIC_STRING:
            sprintf(text[1], "%.5f", floats[1]);
            str_ptr[1] = text[1];
            str_ptr[0] = programGetString(program, type[0], value[0]);
            res = strcmp(str_ptr[1], str_ptr[0]) > 0;
            break;
        case VALUE_TYPE_FLOAT:
            res = floats[1] > floats[0];
            break;
        case VALUE_TYPE_INT:
            res = floats[1] > (float)value[0];
            break;
        default:
            assert(false && "Should be unreachable");
        }
        break;
    case VALUE_TYPE_INT:
        switch (type[0]) {
        case VALUE_TYPE_STRING:
        case VALUE_TYPE_DYNAMIC_STRING:
            sprintf(text[1], "%d", value[1]);
            str_ptr[1] = text[1];
            str_ptr[0] = programGetString(program, type[0], value[0]);
            res = strcmp(str_ptr[1], str_ptr[0]) > 0;
            break;
        case VALUE_TYPE_FLOAT:
            res = (float)value[1] > floats[0];
            break;
        case VALUE_TYPE_INT:
            res = value[1] > value[0];
            break;
        default:
            assert(false && "Should be unreachable");
        }
        break;
    default:
        assert(false && "Should be unreachable");
    }

    stackPushInt32(program->stack, &(program->stackPointer), res);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// 0x469BC4
void opAdd(Program* program)
{
    // TODO: Check everything, too many conditions, variables and allocations.
    opcode_t opcodes[2];
    int values[2];
    float* floats = (float*)&values;
    char* str_ptr[2];
    char* t;
    float resf;

    // NOTE: original code does not use loop
    for (int arg = 0; arg < 2; arg++) {
        opcodes[arg] = stackPopInt16(program->stack, &(program->stackPointer));
        values[arg] = stackPopInt32(program->stack, &(program->stackPointer));

        if (opcodes[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcodes[arg], values[arg]);
        }
    }

    switch (opcodes[1]) {
    case VALUE_TYPE_STRING:
    case VALUE_TYPE_DYNAMIC_STRING:
        str_ptr[1] = programGetString(program, opcodes[1], values[1]);

        switch (opcodes[0]) {
        case VALUE_TYPE_STRING:
        case VALUE_TYPE_DYNAMIC_STRING:
            t = programGetString(program, opcodes[0], values[0]);
            str_ptr[0] = (char*)internal_malloc_safe(strlen(t) + 1, __FILE__, __LINE__); // "..\\int\\INTRPRET.C", 1002
            strcpy(str_ptr[0], t);
            break;
        case VALUE_TYPE_FLOAT:
            str_ptr[0] = (char*)internal_malloc_safe(80, __FILE__, __LINE__); // "..\\int\\INTRPRET.C", 1011
            sprintf(str_ptr[0], "%.5f", floats[0]);
            break;
        case VALUE_TYPE_INT:
            str_ptr[0] = (char*)internal_malloc_safe(80, __FILE__, __LINE__); // "..\\int\\INTRPRET.C", 1007
            sprintf(str_ptr[0], "%d", values[0]);
            break;
        }

        t = (char*)internal_malloc_safe(strlen(str_ptr[1]) + strlen(str_ptr[0]) + 1, __FILE__, __LINE__); // "..\\int\\INTRPRET.C", 1015
        strcpy(t, str_ptr[1]);
        strcat(t, str_ptr[0]);

        stackPushInt32(program->stack, &(program->stackPointer), programPushString(program, t));
        programStackPushInt16(program, VALUE_TYPE_DYNAMIC_STRING);

        internal_free_safe(str_ptr[0], __FILE__, __LINE__); // "..\\int\\INTRPRET.C", 1019
        internal_free_safe(t, __FILE__, __LINE__); // "..\\int\\INTRPRET.C", 1020
        break;
    case VALUE_TYPE_FLOAT:
        switch (opcodes[0]) {
        case VALUE_TYPE_STRING:
        case VALUE_TYPE_DYNAMIC_STRING:
            str_ptr[0] = programGetString(program, opcodes[0], values[0]);
            t = (char*)internal_malloc_safe(strlen(str_ptr[0]) + 80, __FILE__, __LINE__); // "..\\int\\INTRPRET.C", 1039
            sprintf(t, "%.5f", floats[1]);
            strcat(t, str_ptr[0]);

            stackPushInt32(program->stack, &(program->stackPointer), programPushString(program, t));
            programStackPushInt16(program, VALUE_TYPE_DYNAMIC_STRING);

            internal_free_safe(t, __FILE__, __LINE__); // "..\\int\\INTRPRET.C", 1044
            break;
        case VALUE_TYPE_FLOAT:
            resf = floats[1] + floats[0];
            stackPushInt32(program->stack, &(program->stackPointer), *(int*)&resf);
            programStackPushInt16(program, VALUE_TYPE_FLOAT);
            break;
        case VALUE_TYPE_INT:
            resf = floats[1] + (float)values[0];
            stackPushInt32(program->stack, &(program->stackPointer), *(int*)&resf);
            programStackPushInt16(program, VALUE_TYPE_FLOAT);
            break;
        }
        break;
    case VALUE_TYPE_INT:
        switch (opcodes[0]) {
        case VALUE_TYPE_STRING:
        case VALUE_TYPE_DYNAMIC_STRING:
            str_ptr[0] = programGetString(program, opcodes[0], values[0]);
            t = (char*)internal_malloc_safe(strlen(str_ptr[0]) + 80, __FILE__, __LINE__); // "..\\int\\INTRPRET.C", 1070
            sprintf(t, "%d", values[1]);
            strcat(t, str_ptr[0]);

            stackPushInt32(program->stack, &(program->stackPointer), programPushString(program, t));
            programStackPushInt16(program, VALUE_TYPE_DYNAMIC_STRING);

            internal_free_safe(t, __FILE__, __LINE__); // "..\\int\\INTRPRET.C", 1075
            break;
        case VALUE_TYPE_FLOAT:
            resf = (float)values[1] + floats[0];
            stackPushInt32(program->stack, &(program->stackPointer), *(int*)&resf);
            programStackPushInt16(program, VALUE_TYPE_FLOAT);
            break;
        case VALUE_TYPE_INT:
            if ((values[0] <= 0 || (INT_MAX - values[0]) > values[1])
                && (values[0] >= 0 || (INT_MIN - values[0]) <= values[1])) {
                stackPushInt32(program->stack, &(program->stackPointer), values[1] + values[0]);
                programStackPushInt16(program, VALUE_TYPE_INT);
            } else {
                resf = (float)values[1] + (float)values[0];
                stackPushInt32(program->stack, &(program->stackPointer), *(int*)&resf);
                programStackPushInt16(program, VALUE_TYPE_FLOAT);
            }
            break;
        }
        break;
    }
}

// 0x46A1D8
void opSubtract(Program* program)
{
    opcode_t type[2];
    int value[2];
    float* floats = (float*)&value;
    float resf;

    for (int arg = 0; arg < 2; arg++) {
        type[arg] = stackPopInt16(program->stack, &(program->stackPointer));
        value[arg] = stackPopInt32(program->stack, &(program->stackPointer));

        if (type[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, type[arg], value[arg]);
        }
    }

    switch (type[1]) {
    case VALUE_TYPE_FLOAT:
        switch (type[0]) {
        case VALUE_TYPE_FLOAT:
            resf = floats[1] - floats[0];
            break;
        default:
            resf = floats[1] - value[0];
            break;
        }

        stackPushInt32(program->stack, &(program->stackPointer), *(int*)&resf);
        programStackPushInt16(program, VALUE_TYPE_FLOAT);
        break;
    case VALUE_TYPE_INT:
        switch (type[0]) {
        case VALUE_TYPE_FLOAT:
            resf = value[1] - floats[0];

            stackPushInt32(program->stack, &(program->stackPointer), *(int*)&resf);
            programStackPushInt16(program, VALUE_TYPE_FLOAT);
            break;
        default:
            stackPushInt32(program->stack, &(program->stackPointer), value[1] - value[0]);
            programStackPushInt16(program, VALUE_TYPE_INT);
            break;
        }
        break;
    }
}

// 0x46A300
void opMultiply(Program* program)
{
    int arg;
    opcode_t type[2];
    int value[2];
    float* floats = (float*)&value;
    float resf;

    for (arg = 0; arg < 2; arg++) {
        type[arg] = stackPopInt16(program->stack, &(program->stackPointer));
        value[arg] = stackPopInt32(program->stack, &(program->stackPointer));

        if (type[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, type[arg], value[arg]);
        }
    }

    switch (type[1]) {
    case VALUE_TYPE_FLOAT:
        switch (type[0]) {
        case VALUE_TYPE_FLOAT:
            resf = floats[1] * floats[0];
            break;
        default:
            resf = floats[1] * value[0];
            break;
        }

        stackPushInt32(program->stack, &(program->stackPointer), *(int*)&resf);
        programStackPushInt16(program, VALUE_TYPE_FLOAT);
        break;
    case VALUE_TYPE_INT:
        switch (type[0]) {
        case VALUE_TYPE_FLOAT:
            resf = value[1] * floats[0];

            stackPushInt32(program->stack, &(program->stackPointer), *(int*)&resf);
            programStackPushInt16(program, VALUE_TYPE_FLOAT);
            break;
        default:
            stackPushInt32(program->stack, &(program->stackPointer), value[0] * value[1]);
            programStackPushInt16(program, VALUE_TYPE_INT);
            break;
        }
        break;
    }
}

// 0x46A424
void opDivide(Program* program)
{
    // TODO: Check entire function, probably errors due to casts.
    opcode_t type[2];
    int value[2];
    float* float_value = (float*)&value;
    float divisor;
    float result;

    type[0] = stackPopInt16(program->stack, &(program->stackPointer));
    value[0] = stackPopInt32(program->stack, &(program->stackPointer));

    if (type[0] == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, type[0], value[0]);
    }

    type[1] = stackPopInt16(program->stack, &(program->stackPointer));
    value[1] = stackPopInt32(program->stack, &(program->stackPointer));

    if (type[1] == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, type[1], value[1]);
    }

    switch (type[1]) {
    case VALUE_TYPE_FLOAT:
        if (type[0] == VALUE_TYPE_FLOAT) {
            divisor = float_value[0];
        } else {
            divisor = (float)value[0];
        }

        if ((int)divisor & 0x7FFFFFFF) {
            programFatalError("Division (DIV) by zero");
        }

        result = float_value[1] / divisor;
        stackPushInt32(program->stack, &(program->stackPointer), *(int*)&result);
        programStackPushInt16(program, VALUE_TYPE_FLOAT);
        break;
    case VALUE_TYPE_INT:
        if (type[0] == VALUE_TYPE_FLOAT) {
            divisor = float_value[0];

            if ((int)divisor & 0x7FFFFFFF) {
                programFatalError("Division (DIV) by zero");
            }

            result = (float)value[1] / divisor;
            stackPushInt32(program->stack, &(program->stackPointer), *(int*)&result);
            programStackPushInt16(program, VALUE_TYPE_FLOAT);
        } else {
            if (value[0] == 0) {
                programFatalError("Division (DIV) by zero");
            }

            stackPushInt32(program->stack, &(program->stackPointer), value[1] / value[0]);
            programStackPushInt16(program, VALUE_TYPE_INT);
        }
        break;
    }
}

// 0x46A5B8
void opModulo(Program* program)
{
    opcode_t type[2];
    int value[2];

    type[0] = stackPopInt16(program->stack, &(program->stackPointer));
    value[0] = stackPopInt32(program->stack, &(program->stackPointer));

    if (type[0] == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, type[0], value[0]);
    }

    type[1] = stackPopInt16(program->stack, &(program->stackPointer));
    value[1] = stackPopInt32(program->stack, &(program->stackPointer));

    if (type[1] == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, type[1], value[1]);
    }

    if (type[1] == VALUE_TYPE_FLOAT) {
        programFatalError("Trying to MOD a float");
    }

    if (type[1] != VALUE_TYPE_INT) {
        return;
    }

    if (type[0] == VALUE_TYPE_FLOAT) {
        programFatalError("Trying to MOD with a float");
    }

    if (value[0] == 0) {
        programFatalError("Division (MOD) by zero");
    }

    stackPushInt32(program->stack, &(program->stackPointer), value[1] % value[0]);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// 0x46A6B4
void opLogicalOperatorAnd(Program* program)
{
    opcode_t type[2];
    int value[2];
    int result;

    type[0] = stackPopInt16(program->stack, &(program->stackPointer));
    value[0] = stackPopInt32(program->stack, &(program->stackPointer));

    if (type[0] == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, type[0], value[0]);
    }

    type[1] = stackPopInt16(program->stack, &(program->stackPointer));
    value[1] = stackPopInt32(program->stack, &(program->stackPointer));

    if (type[1] == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, type[1], value[1]);
    }

    switch (type[1]) {
    case VALUE_TYPE_STRING:
    case VALUE_TYPE_DYNAMIC_STRING:
        switch (type[0]) {
        case VALUE_TYPE_STRING:
        case VALUE_TYPE_DYNAMIC_STRING:
            result = 1;
            break;
        case VALUE_TYPE_FLOAT:
            result = (value[0] & 0x7FFFFFFF) != 0;
            break;
        case VALUE_TYPE_INT:
            result = value[0] != 0;
            break;
        default:
            assert(false && "Should be unreachable");
        }
        break;
    case VALUE_TYPE_FLOAT:
        switch (type[0]) {
        case VALUE_TYPE_STRING:
        case VALUE_TYPE_DYNAMIC_STRING:
            result = value[1] != 0;
            break;
        case VALUE_TYPE_FLOAT:
            result = (value[1] & 0x7FFFFFFF) && (value[0] & 0x7FFFFFFF);
            break;
        case VALUE_TYPE_INT:
            result = (value[1] & 0x7FFFFFFF) && (value[0] != 0);
            break;
        default:
            assert(false && "Should be unreachable");
        }
        break;
    case VALUE_TYPE_INT:
        switch (type[0]) {
        case VALUE_TYPE_STRING:
        case VALUE_TYPE_DYNAMIC_STRING:
            result = value[1] != 0;
            break;
        case VALUE_TYPE_FLOAT:
            result = (value[1] != 0) && (value[0] & 0x7FFFFFFF);
            break;
        case VALUE_TYPE_INT:
            result = (value[1] != 0) && (value[0] != 0);
            break;
        default:
            assert(false && "Should be unreachable");
        }
        break;
    default:
        assert(false && "Should be unreachable");
    }

    stackPushInt32(program->stack, &(program->stackPointer), result);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// 0x46A8D8
void opLogicalOperatorOr(Program* program)
{
    opcode_t type[2];
    int value[2];
    int result;

    type[0] = stackPopInt16(program->stack, &(program->stackPointer));
    value[0] = stackPopInt32(program->stack, &(program->stackPointer));

    if (type[0] == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, type[0], value[0]);
    }

    type[1] = stackPopInt16(program->stack, &(program->stackPointer));
    value[1] = stackPopInt32(program->stack, &(program->stackPointer));

    if (type[1] == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, type[1], value[1]);
    }

    switch (type[1]) {
    case VALUE_TYPE_STRING:
    case VALUE_TYPE_DYNAMIC_STRING:
        switch (type[0]) {
        case VALUE_TYPE_STRING:
        case VALUE_TYPE_DYNAMIC_STRING:
        case VALUE_TYPE_FLOAT:
        case VALUE_TYPE_INT:
            result = 1;
            break;
        default:
            assert(false && "Should be unreachable");
        }
        break;
    case VALUE_TYPE_FLOAT:
        switch (type[0]) {
        case VALUE_TYPE_STRING:
        case VALUE_TYPE_DYNAMIC_STRING:
            result = 1;
            break;
        case VALUE_TYPE_FLOAT:
            result = (value[1] & 0x7FFFFFFF) || (value[0] & 0x7FFFFFFF);
            break;
        case VALUE_TYPE_INT:
            result = (value[1] & 0x7FFFFFFF) || (value[0] != 0);
            break;
        default:
            assert(false && "Should be unreachable");
        }
        break;
    case VALUE_TYPE_INT:
        switch (type[0]) {
        case VALUE_TYPE_STRING:
        case VALUE_TYPE_DYNAMIC_STRING:
            result = 1;
            break;
        case VALUE_TYPE_FLOAT:
            result = (value[1] != 0) || (value[0] & 0x7FFFFFFF);
            break;
        case VALUE_TYPE_INT:
            result = (value[1] != 0) || (value[0] != 0);
            break;
        default:
            assert(false && "Should be unreachable");
        }
        break;
    default:
        assert(false && "Should be unreachable");
    }

    stackPushInt32(program->stack, &(program->stackPointer), result);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// 0x46AACC
void opLogicalOperatorNot(Program* program)
{
    opcode_t type;
    int value;

    type = programStackPopInt16(program);
    value = programStackPopInt32(program);

    if (type == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, type, value);
    }

    programStackPushInt32(program, value == 0);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// 0x46AB2C
void opUnaryMinus(Program* program)
{
    opcode_t type;
    int value;

    type = stackPopInt16(program->stack, &(program->stackPointer));
    value = stackPopInt32(program->stack, &(program->stackPointer));
    if (type == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, type, value);
    }

    stackPushInt32(program->stack, &(program->stackPointer), -value);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// 0x46AB84
void opBitwiseOperatorNot(Program* program)
{
    opcode_t type;
    int value;

    type = programStackPopInt16(program);
    value = programStackPopInt32(program);

    if (type == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, type, value);
    }

    programStackPushInt32(program, ~value);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// floor
// 0x46ABDC
void opFloor(Program* program)
{
    opcode_t type = stackPopInt16(program->stack, &(program->stackPointer));
    int data = stackPopInt32(program->stack, &(program->stackPointer));

    if (type == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, type, data);
    }

    if (type == VALUE_TYPE_STRING) {
        programFatalError("Invalid arg given to floor()");
    } else if (type == VALUE_TYPE_FLOAT) {
        type = VALUE_TYPE_INT;
        data = (int)(*((float*)&data));
    }

    stackPushInt32(program->stack, &(program->stackPointer), data);
    programStackPushInt16(program, type);
}

// 0x46AC78
void opBitwiseOperatorAnd(Program* program)
{
    opcode_t type[2];
    int value[2];
    int result;

    type[0] = stackPopInt16(program->stack, &(program->stackPointer));
    value[0] = stackPopInt32(program->stack, &(program->stackPointer));

    if (type[0] == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, type[0], value[0]);
    }

    type[1] = stackPopInt16(program->stack, &(program->stackPointer));
    value[1] = stackPopInt32(program->stack, &(program->stackPointer));

    if (type[1] == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, type[1], value[1]);
    }

    switch (type[1]) {
    case VALUE_TYPE_FLOAT:
        switch (type[0]) {
        case VALUE_TYPE_FLOAT:
            result = (int)(float)value[1] & (int)(float)value[0];
            break;
        default:
            result = (int)(float)value[1] & value[0];
            break;
        }
        break;
    case VALUE_TYPE_INT:
        switch (type[0]) {
        case VALUE_TYPE_FLOAT:
            result = value[1] & (int)(float)value[0];
            break;
        default:
            result = value[1] & value[0];
            break;
        }
        break;
    default:
        return;
    }

    stackPushInt32(program->stack, &(program->stackPointer), result);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// 0x46ADA4
void opBitwiseOperatorOr(Program* program)
{
    opcode_t type[2];
    int value[2];
    int result;

    type[0] = stackPopInt16(program->stack, &(program->stackPointer));
    value[0] = stackPopInt32(program->stack, &(program->stackPointer));

    if (type[0] == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, type[0], value[0]);
    }

    type[1] = stackPopInt16(program->stack, &(program->stackPointer));
    value[1] = stackPopInt32(program->stack, &(program->stackPointer));

    if (type[1] == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, type[1], value[1]);
    }

    switch (type[1]) {
    case VALUE_TYPE_FLOAT:
        switch (type[0]) {
        case VALUE_TYPE_FLOAT:
            result = (int)(float)value[1] | (int)(float)value[0];
            break;
        default:
            result = (int)(float)value[1] | value[0];
            break;
        }
        break;
    case VALUE_TYPE_INT:
        switch (type[0]) {
        case VALUE_TYPE_FLOAT:
            result = value[1] | (int)(float)value[0];
            break;
        default:
            result = value[1] | value[0];
            break;
        }
        break;
    default:
        return;
    }

    stackPushInt32(program->stack, &(program->stackPointer), result);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// 0x46AED0
void opBitwiseOperatorXor(Program* program)
{
    opcode_t type[2];
    int value[2];
    int result;

    type[0] = stackPopInt16(program->stack, &(program->stackPointer));
    value[0] = stackPopInt32(program->stack, &(program->stackPointer));

    if (type[0] == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, type[0], value[0]);
    }

    type[1] = stackPopInt16(program->stack, &(program->stackPointer));
    value[1] = stackPopInt32(program->stack, &(program->stackPointer));

    if (type[1] == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, type[1], value[1]);
    }

    switch (type[1]) {
    case VALUE_TYPE_FLOAT:
        switch (type[0]) {
        case VALUE_TYPE_FLOAT:
            result = (int)(float)value[1] ^ (int)(float)value[0];
            break;
        default:
            result = (int)(float)value[1] ^ value[0];
            break;
        }
        break;
    case VALUE_TYPE_INT:
        switch (type[0]) {
        case VALUE_TYPE_FLOAT:
            result = value[1] ^ (int)(float)value[0];
            break;
        default:
            result = value[1] ^ value[0];
            break;
        }
        break;
    default:
        return;
    }

    stackPushInt32(program->stack, &(program->stackPointer), result);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// 0x46AFFC
void opSwapReturnStack(Program* program)
{
    opcode_t v1;
    int v5;
    opcode_t a2;
    int v10;

    v1 = programReturnStackPopInt16(program);
    v5 = stackPopInt32(program->returnStack, &(program->returnStackPointer));

    a2 = programReturnStackPopInt16(program);
    v10 = stackPopInt32(program->returnStack, &(program->returnStackPointer));

    stackPushInt32(program->returnStack, &(program->returnStackPointer), v5);
    programReturnStackPushInt16(program, v1);

    stackPushInt32(program->returnStack, &(program->returnStackPointer), v10);
    programReturnStackPushInt16(program, a2);
}

// 0x46B070
void opLeaveCriticalSection(Program* program)
{
    program->flags &= ~PROGRAM_FLAG_CRITICAL_SECTION;
}

// 0x46B078
void opEnterCriticalSection(Program* program)
{
    program->flags |= PROGRAM_FLAG_CRITICAL_SECTION;
}

// 0x46B080
void opJump(Program* program)
{
    opcode_t type;
    int value;
    char err[260];

    type = stackPopInt16(program->stack, &(program->stackPointer));
    value = stackPopInt32(program->stack, &(program->stackPointer));

    // NOTE: comparing shorts (0x46B0B1)
    if (type == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, type, value);
    }

    // NOTE: comparing ints (0x46B0D3)
    if ((type & VALUE_TYPE_MASK) != VALUE_TYPE_INT) {
        sprintf(err, "Invalid type given to jmp, %x", value);
        programFatalError(err);
    }

    program->instructionPointer = value;
}

// 0x46B108
void opCall(Program* program)
{
    opcode_t type = stackPopInt16(program->stack, &(program->stackPointer));
    int value = stackPopInt32(program->stack, &(program->stackPointer));
    if (type == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, type, value);
    }

    if ((type & 0xF7FF) != VALUE_TYPE_INT) {
        programFatalError("Invalid address given to call");
    }

    unsigned char* ptr = program->procedures + 4 + 24 * value;

    int flags = stackReadInt32(ptr, 4);
    if ((flags & 4) != 0) {
        // TODO: Incomplete.
    } else {
        program->instructionPointer = stackReadInt32(ptr, 16);
        if ((flags & 0x10) != 0) {
            program->flags |= PROGRAM_FLAG_CRITICAL_SECTION;
        }
    }
}

// 0x46B590
void op801F(Program* program)
{
    opcode_t opcode[3];
    int data[3];

    for (int arg = 0; arg < 3; arg++) {
        opcode[arg] = stackPopInt16(program->stack, &(program->stackPointer));
        data[arg] = stackPopInt32(program->stack, &(program->stackPointer));

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }
    }

    program->field_84 = data[0];
    program->field_7C = (int (*)(Program*))data[1];
    program->flags = data[2] & 0xFFFF;
}

// pop stack 2 -> set program address
// 0x46B63C
void op801C(Program* program)
{
    programReturnStackPopInt16(program);
    program->instructionPointer = stackPopInt32(program->returnStack, &(program->returnStackPointer));
}

// 0x46B658
void op801D(Program* program)
{
    programReturnStackPopInt16(program);
    program->instructionPointer = stackPopInt32(program->returnStack, &(program->returnStackPointer));

    program->flags |= PROGRAM_FLAG_0x40;
}

// 0x46B67C
void op8020(Program* program)
{
    op801F(program);
    programReturnStackPopInt16(program);
    program->instructionPointer = programReturnStackPopInt32(program);
}

// 0x46B698
void op8021(Program* program)
{
    op801F(program);
    programReturnStackPopInt16(program);
    program->instructionPointer = programReturnStackPopInt32(program);
    program->flags |= PROGRAM_FLAG_0x40;
}

// 0x46B6BC
void op8025(Program* program)
{
    opcode_t type;
    int value;

    type = programStackPopInt16(program);
    value = programStackPopInt32(program);

    if (type == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, type, value);
    }

    op801F(program);
    programReturnStackPopInt16(program);
    program->instructionPointer = programReturnStackPopInt32(program);
    program->flags |= PROGRAM_FLAG_0x40;
    programStackPushInt32(program, value);
    programStackPushInt16(program, type);
}

// 0x46B73C
void op8026(Program* program)
{
    opcode_t type;
    int value;
    Program* v1;

    type = stackPopInt16(program->stack, &(program->stackPointer));
    value = stackPopInt32(program->stack, &(program->stackPointer));

    if (type == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, type, value);
    }

    op801F(program);

    programReturnStackPopInt16(program);
    v1 = (Program*)stackPopInt32(program->returnStack, &(program->returnStackPointer));

    programReturnStackPopInt16(program);
    v1->field_7C = (int (*)(Program*))stackPopInt32(program->returnStack, &(program->returnStackPointer));

    programReturnStackPopInt16(program);
    v1->flags = stackPopInt32(program->returnStack, &(program->returnStackPointer));

    program->instructionPointer = programReturnStackPopInt32(program);

    program->flags |= PROGRAM_FLAG_0x40;

    stackPushInt32(program->stack, &(program->stackPointer), value);
    programStackPushInt16(program, type);
}

// 0x46B808
void op8022(Program* program)
{
    Program* v1;

    op801F(program);

    programReturnStackPopInt16(program);
    v1 = (Program*)stackPopInt32(program->returnStack, &(program->returnStackPointer));

    programReturnStackPopInt16(program);
    v1->field_7C = (int (*)(Program*))stackPopInt32(program->returnStack, &(program->returnStackPointer));

    programReturnStackPopInt16(program);
    v1->flags = stackPopInt32(program->returnStack, &(program->returnStackPointer));

    programReturnStackPopInt16(program);
    program->instructionPointer = programReturnStackPopInt32(program);
}

// 0x46B86C
void op8023(Program* program)
{
    Program* v1;

    op801F(program);

    programReturnStackPopInt16(program);
    v1 = (Program*)stackPopInt32(program->returnStack, &(program->returnStackPointer));

    programReturnStackPopInt16(program);
    v1->field_7C = (int (*)(Program*))stackPopInt32(program->returnStack, &(program->returnStackPointer));

    programReturnStackPopInt16(program);
    v1->flags = stackPopInt32(program->returnStack, &(program->returnStackPointer));

    programReturnStackPopInt16(program);
    program->instructionPointer = programReturnStackPopInt32(program);

    program->flags |= 0x40;
}

// pop value from stack 1 and push it to script popped from stack 2
// 0x46B8D8
void op8024(Program* program)
{
    opcode_t type;
    int value;
    Program* v10;
    char* str;

    type = stackPopInt16(program->stack, &(program->stackPointer));
    value = stackPopInt32(program->stack, &(program->stackPointer));

    if (type == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, type, value);
    }

    op801F(program);

    programReturnStackPopInt16(program);
    v10 = (Program*)stackPopInt32(program->returnStack, &(program->returnStackPointer));

    programReturnStackPopInt16(program);
    v10->field_7C = (int (*)(Program*))stackPopInt32(program->returnStack, &(program->returnStackPointer));

    programReturnStackPopInt16(program);
    v10->flags = stackPopInt32(program->returnStack, &(program->returnStackPointer));

    if ((type & 0xF7FF) == VALUE_TYPE_STRING) {
        str = programGetString(program, type, value);
        stackPushInt32(v10->stack, &(v10->stackPointer), programPushString(v10, str));
        type = VALUE_TYPE_DYNAMIC_STRING;
    } else {
        stackPushInt32(v10->stack, &(v10->stackPointer), value);
    }

    programStackPushInt16(v10, type);

    if (v10->flags & 0x80) {
        program->flags &= ~0x80;
    }

    programReturnStackPopInt16(program);
    program->instructionPointer = programReturnStackPopInt32(program);

    programReturnStackPopInt16(v10);
    v10->instructionPointer = programReturnStackPopInt32(program);
}

// 0x46BA10
void op801E(Program* program)
{
    programReturnStackPopInt16(program);
    programReturnStackPopInt32(program);
}

// 0x46BA2C
void opAtoD(Program* program)
{
    opcode_t opcode = programReturnStackPopInt16(program);
    int data = stackPopInt32(program->returnStack, &(program->returnStackPointer));

    stackPushInt32(program->stack, &(program->stackPointer), data);
    programStackPushInt16(program, opcode);
}

// 0x46BA68
void opDtoA(Program* program)
{
    opcode_t opcode = stackPopInt16(program->stack, &(program->stackPointer));
    int data = stackPopInt32(program->stack, &(program->stackPointer));

    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    stackPushInt32(program->returnStack, &(program->returnStackPointer), data);
    programReturnStackPushInt16(program, opcode);
}

// 0x46BAC0
void opExitProgram(Program* program)
{
    program->flags |= PROGRAM_FLAG_EXITED;
}

// 0x46BAC8
void opStopProgram(Program* program)
{
    program->flags |= PROGRAM_FLAG_STOPPED;
}

// 0x46BAD0
void opFetchGlobalVariable(Program* program)
{
    opcode_t opcode = programStackPopInt16(program);
    int data = programStackPopInt32(program);

    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    // TODO: Check.
    int addr = program->basePointer + 6 * data;
    int v8 = stackReadInt32(program->stack, addr);
    opcode_t varType = stackReadInt16(program->stack, addr + 4);

    programStackPushInt32(program, v8);
    // TODO: Check.
    programStackPushInt16(program, varType);
}

// 0x46BB5C
void opStoreGlobalVariable(Program* program)
{
    opcode_t type[2];
    int value[2];

    for (int arg = 0; arg < 2; arg++) {
        type[arg] = stackPopInt16(program->stack, &(program->stackPointer));
        value[arg] = stackPopInt32(program->stack, &(program->stackPointer));

        if (type[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, type[arg], value[arg]);
        }
    }

    int var_address = program->basePointer + 6 * value[0];

    opcode_t var_type = stackReadInt16(program->stack, var_address + 4);
    int var_value = stackReadInt32(program->stack, var_address);

    if (var_type == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, var_type, var_value);
    }

    // TODO: Check offsets.
    stackWriteInt32(value[1], program->stack, var_address);

    // TODO: Check endianness.
    stackWriteInt16(type[1], program->stack, var_address + 4);

    if (type[1] == VALUE_TYPE_DYNAMIC_STRING) {
        *(short*)(program->dynamicStrings + 4 + value[1] - 2) += 1;
    }
}

// 0x46BCAC
void opSwapStack(Program* program)
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
        programStackPushInt32(program, data[arg]);
        programStackPushInt16(program, opcode[arg]);
    }
}

// fetch_proc_address
// 0x46BD60
void opFetchProcedureAddress(Program* program)
{
    opcode_t opcode = stackPopInt16(program->stack, &(program->stackPointer));
    int data = stackPopInt32(program->stack, &(program->stackPointer));

    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    if (opcode != VALUE_TYPE_INT) {
        char err[256];
        sprintf(err, "Invalid type given to fetch_proc_address, %x", opcode);
        programFatalError(err);
    }

    int procedureIndex = data;

    int address = stackReadInt32(program->procedures + 4 + sizeof(Procedure) * procedureIndex, 16);
    stackPushInt32(program->stack, &(program->stackPointer), address);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// Pops value from stack and throws it away.
//
// 0x46BE10
void opPop(Program* program)
{
    opcode_t opcode = stackPopInt16(program->stack, &(program->stackPointer));
    int data = stackPopInt32(program->stack, &(program->stackPointer));

    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }
}

// 0x46BE4C
void opDuplicate(Program* program)
{
    opcode_t opcode = programStackPopInt16(program);
    int data = programStackPopInt32(program);

    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    programStackPushInt32(program, data);
    programStackPushInt16(program, opcode);

    programStackPushInt32(program, data);
    programStackPushInt16(program, opcode);
}

// 0x46BEC8
void opStoreExternalVariable(Program* program)
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

    const char* identifier = programGetIdentifier(program, data[0]);

    if (externalVariableSetValue(program, identifier, opcode[1], data[1])) {
        char err[256];
        sprintf(err, "External variable %s does not exist\n", identifier);
        programFatalError(err);
    }
}

// 0x46BF90
void opFetchExternalVariable(Program* program)
{
    opcode_t opcode = programStackPopInt16(program);
    int data = programStackPopInt32(program);

    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    const char* identifier = programGetIdentifier(program, data);

    opcode_t variableOpcode;
    int variableData;
    if (externalVariableGetValue(program, identifier, &variableOpcode, &variableData) != 0) {
        char err[256];
        sprintf(err, "External variable %s does not exist\n", identifier);
        programFatalError(err);
    }

    programStackPushInt32(program, variableData);
    programStackPushInt16(program, variableOpcode);
}

// 0x46C044
void opExportProcedure(Program* program)
{
    opcode_t type;
    int value;
    int proc_index;
    unsigned char* proc_ptr;
    char* v9;
    int v10;
    char err[256];

    type = programStackPopInt16(program);
    value = programStackPopInt32(program);

    if (type == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, type, value);
    }

    proc_index = value;

    type = programStackPopInt16(program);
    value = programStackPopInt32(program);

    if (type == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, type, value);
    }

    proc_ptr = program->procedures + 4 + sizeof(Procedure) * proc_index;

    v9 = (char*)(program->identifiers + stackReadInt32(proc_ptr, 0));
    v10 = stackReadInt32(proc_ptr, 16);

    if (externalProcedureCreate(program, v9, v10, value) != 0) {
        sprintf(err, "Error exporting procedure %s", v9);
        programFatalError(err);
    }
}

// 0x46C120
void opExportVariable(Program* program)
{
    opcode_t opcode = stackPopInt16(program->stack, &(program->stackPointer));
    int data = stackPopInt32(program->stack, &(program->stackPointer));

    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    if (externalVariableCreate(program, programGetIdentifier(program, data))) {
        char err[256];
        sprintf(err, "External variable %s already exists", programGetIdentifier(program, data));
        programFatalError(err);
    }
}

// 0x46C1A0
void opExit(Program* program)
{
    program->flags |= PROGRAM_FLAG_EXITED;

    Program* parent = program->parent;
    if (parent != NULL) {
        if ((parent->flags & PROGRAM_FLAG_0x0100) != 0) {
            parent->flags &= ~PROGRAM_FLAG_0x0100;
        }
    }

    if (!program->exited) {
        _removeProgramReferences_(program);
        program->exited = true;
    }
}

// 0x46C1EC
void opDetach(Program* program)
{
    Program* parent = program->parent;
    if (parent == NULL) {
        return;
    }

    parent->flags &= ~PROGRAM_FLAG_0x20;
    parent->flags &= ~PROGRAM_FLAG_0x0100;

    if (parent->child == program) {
        parent->child = NULL;
    }
}

// callstart
// 0x46C218
void opCallStart(Program* program)
{
    opcode_t type;
    int value;
    char* name;
    char err[260];

    if (program->child) {
        programFatalError("Error, already have a child process\n");
    }

    type = programStackPopInt16(program);
    value = programStackPopInt32(program);

    if (type == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, type, value);
    }

    if ((type & 0xF7FF) != VALUE_TYPE_STRING) {
        programFatalError("Invalid type given to callstart");
    }

    program->flags |= PROGRAM_FLAG_0x20;

    name = programGetString(program, type, value);

    name = _interpretMangleName(name);
    program->child = programCreateByPath(name);
    if (program->child == NULL) {
        sprintf(err, "Error spawning child %s", programGetString(program, type, value));
        programFatalError(err);
    }

    programListNodeCreate(program->child);
    _interpret(program->child, 24);

    program->child->parent = program;
    program->child->field_84 = program->field_84;
}

// spawn
// 0x46C344
void opSpawn(Program* program)
{
    opcode_t type;
    int value;
    char* name;
    char err[256];

    if (program->child) {
        programFatalError("Error, already have a child process\n");
    }

    type = programStackPopInt16(program);
    value = programStackPopInt32(program);

    if (type == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, type, value);
    }

    if ((type & 0xF7FF) != VALUE_TYPE_STRING) {
        programFatalError("Invalid type given to spawn");
    }

    program->flags |= PROGRAM_FLAG_0x0100;

    if ((type >> 8) & 8) {
        name = (char*)program->dynamicStrings + 4 + value;
    } else if ((type >> 8) & 16) {
        name = (char*)program->staticStrings + 4 + value;
    } else {
        name = NULL;
    }

    name = _interpretMangleName(name);
    program->child = programCreateByPath(name);
    if (program->child == NULL) {
        sprintf(err, "Error spawning child %s", programGetString(program, type, value));
        programFatalError(err);
    }

    programListNodeCreate(program->child);
    _interpret(program->child, 24);

    program->child->parent = program;
    program->child->field_84 = program->field_84;

    if ((program->flags & PROGRAM_FLAG_CRITICAL_SECTION) != 0) {
        program->child->flags |= PROGRAM_FLAG_CRITICAL_SECTION;
        _interpret(program->child, -1);
    }
}

// fork
// 0x46C490
Program* forkProgram(Program* program)
{
    opcode_t opcode = stackPopInt16(program->stack, &(program->stackPointer));
    int data = stackPopInt32(program->stack, &(program->stackPointer));

    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    char* name = programGetString(program, opcode, data);
    name = _interpretMangleName(name);
    Program* forked = programCreateByPath(name);

    if (forked == NULL) {
        char err[256];
        sprintf(err, "couldn't fork script '%s'", programGetString(program, opcode, data));
        programFatalError(err);
    }

    programListNodeCreate(forked);

    _interpret(forked, 24);

    forked->field_84 = program->field_84;

    return forked;
}

// NOTE: Uncollapsed 0x46C490 with different signature.
//
// 0x46C490
void opFork(Program* program)
{
    forkProgram(program);
}

// 0x46C574
void opExec(Program* program)
{
    Program* parent = program->parent;
    Program* fork = forkProgram(program);

    if (parent != NULL) {
        fork->parent = parent;
        parent->child = fork;
    }

    fork->child = NULL;

    program->parent = NULL;
    program->flags |= PROGRAM_FLAG_EXITED;

    // probably inlining due to check for null
    parent = program->parent;
    if (parent != NULL) {
        if ((parent->flags & PROGRAM_FLAG_0x0100) != 0) {
            parent->flags &= ~PROGRAM_FLAG_0x0100;
        }
    }

    _purgeProgram(program);
}

// 0x46C5D8
void opCheckProcedureArgumentCount(Program* program)
{
    opcode_t opcode[2];
    int data[2];

    // NOTE: original code does not use loop
    for (int arg = 0; arg < 2; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }
    }

    int expectedArgumentCount = data[0];
    int procedureIndex = data[1];

    int actualArgumentCount = stackReadInt32(program->procedures + 4 + 24 * procedureIndex, 20);
    if (actualArgumentCount != expectedArgumentCount) {
        const char* identifier = programGetIdentifier(program, stackReadInt32(program->procedures + 4 + 24 * procedureIndex, 0));
        char err[260];
        sprintf(err, "Wrong number of args to procedure %s\n", identifier);
        programFatalError(err);
    }
}

// lookup_string_proc
// 0x46C6B4
void opLookupStringProc(Program* program)
{
    opcode_t opcode = programStackPopInt16(program);
    int data = programStackPopInt32(program);

    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    if ((opcode & 0xF7FF) != VALUE_TYPE_STRING) {
        programFatalError("Wrong type given to lookup_string_proc\n");
    }

    const char* procedureNameToLookup = programGetString(program, opcode, data);

    int procedureCount = stackReadInt32(program->procedures, 0);

    // Skip procedure count (4 bytes) and main procedure, which cannot be
    // looked up.
    unsigned char* procedurePtr = program->procedures + 4 + sizeof(Procedure);

    // Start with 1 since we've skipped main procedure, which is always at
    // index 0.
    for (int index = 1; index < procedureCount; index++) {
        int offset = stackReadInt32(procedurePtr, 0);
        const char* procedureName = programGetIdentifier(program, offset);
        if (compat_stricmp(procedureName, procedureNameToLookup) == 0) {
            programStackPushInt32(program, index);
            programStackPushInt16(program, VALUE_TYPE_INT);
            return;
        }

        procedurePtr += sizeof(Procedure);
    }

    char err[260];
    sprintf(err, "Couldn't find string procedure %s\n", procedureNameToLookup);
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
    interpreterRegisterOpcode(OPCODE_POP_RETURN, op801C);
    interpreterRegisterOpcode(OPCODE_POP_EXIT, op801D);
    interpreterRegisterOpcode(OPCODE_POP_ADDRESS, op801E);
    interpreterRegisterOpcode(OPCODE_POP_FLAGS, op801F);
    interpreterRegisterOpcode(OPCODE_POP_FLAGS_RETURN, op8020);
    interpreterRegisterOpcode(OPCODE_POP_FLAGS_EXIT, op8021);
    interpreterRegisterOpcode(OPCODE_POP_FLAGS_RETURN_EXTERN, op8022);
    interpreterRegisterOpcode(OPCODE_POP_FLAGS_EXIT_EXTERN, op8023);
    interpreterRegisterOpcode(OPCODE_POP_FLAGS_RETURN_VAL_EXTERN, op8024);
    interpreterRegisterOpcode(OPCODE_POP_FLAGS_RETURN_VAL_EXIT, op8025);
    interpreterRegisterOpcode(OPCODE_POP_FLAGS_RETURN_VAL_EXIT_EXTERN, op8026);
    interpreterRegisterOpcode(OPCODE_CHECK_PROCEDURE_ARGUMENT_COUNT, opCheckProcedureArgumentCount);
    interpreterRegisterOpcode(OPCODE_LOOKUP_PROCEDURE_BY_NAME, opLookupStringProc);
    interpreterRegisterOpcode(OPCODE_POP_BASE, opPopBase);
    interpreterRegisterOpcode(OPCODE_POP_TO_BASE, opPopToBase);
    interpreterRegisterOpcode(OPCODE_PUSH_BASE, opPushBase);
    interpreterRegisterOpcode(OPCODE_SET_GLOBAL, op802C);
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

    _initIntlib();
    _initExport();
}

// 0x46CC68
void _interpretClose()
{
    externalVariablesClear();
    _intlibClose();
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

        if ((program->flags & PROGRAM_FLAG_0x10) != 0) {
            _busy = 1;

            if (program->field_7C != NULL) {
                if (!program->field_7C(program)) {
                    _busy = 0;
                    continue;
                }
            }

            _busy = 0;
            program->field_7C = NULL;
            program->flags &= ~PROGRAM_FLAG_0x10;
        }

        int instructionPointer = program->instructionPointer;
        program->instructionPointer = instructionPointer + 2;

        opcode_t opcode = stackReadInt16(program->data, instructionPointer);
        // TODO: Replace with field_82 and field_80?
        program->flags &= 0xFFFF;
        program->flags |= (opcode << 16);

        if (!((opcode >> 8) & 0x80)) {
            sprintf(err, "Bad opcode %x %c %d.", opcode, opcode, opcode);
            programFatalError(err);
        }

        unsigned int opcodeIndex = opcode & 0x3FF;
        OpcodeHandler* handler = gInterpreterOpcodeHandlers[opcodeIndex];
        if (handler == NULL) {
            sprintf(err, "Undefined opcode %x.", opcode);
            programFatalError(err);
        }

        handler(program);
    }

    if ((program->flags & PROGRAM_FLAG_EXITED) != 0) {
        if (program->parent != NULL) {
            if (program->parent->flags & PROGRAM_FLAG_0x20) {
                program->parent->flags &= ~PROGRAM_FLAG_0x20;
                program->parent->child = NULL;
                program->parent = NULL;
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
void _setupCallWithReturnVal(Program* program, int address, int returnAddress)
{
    // Save current instruction pointer
    stackPushInt32(program->returnStack, &(program->returnStackPointer), program->instructionPointer);
    programReturnStackPushInt16(program, VALUE_TYPE_INT);

    // Save return address
    stackPushInt32(program->returnStack, &(program->returnStackPointer), returnAddress);
    programReturnStackPushInt16(program, VALUE_TYPE_INT);

    // Save program flags
    stackPushInt32(program->stack, &(program->stackPointer), program->flags & 0xFFFF);
    programStackPushInt16(program, VALUE_TYPE_INT);

    stackPushInt32(program->stack, &(program->stackPointer), (intptr_t)program->field_7C);
    programStackPushInt16(program, VALUE_TYPE_INT);

    stackPushInt32(program->stack, &(program->stackPointer), program->field_84);
    programStackPushInt16(program, VALUE_TYPE_INT);

    program->flags &= ~0xFFFF;
    program->instructionPointer = address;
}

// 0x46CF9C
void _setupExternalCallWithReturnVal(Program* program1, Program* program2, int address, int a4)
{
    stackPushInt32(program2->returnStack, &(program2->returnStackPointer), program2->instructionPointer);
    programReturnStackPushInt16(program2, VALUE_TYPE_INT);

    stackPushInt32(program2->returnStack, &(program2->returnStackPointer), program1->flags & 0xFFFF);
    programReturnStackPushInt16(program2, VALUE_TYPE_INT);

    stackPushInt32(program2->returnStack, &(program2->returnStackPointer), (intptr_t)program1->field_7C);
    programReturnStackPushInt16(program2, VALUE_TYPE_INT);

    stackPushInt32(program2->returnStack, &(program2->returnStackPointer), (intptr_t)program1);
    programReturnStackPushInt16(program2, VALUE_TYPE_INT);

    stackPushInt32(program2->returnStack, &(program2->returnStackPointer), a4);
    programReturnStackPushInt16(program2, VALUE_TYPE_INT);

    stackPushInt32(program2->stack, &(program2->stackPointer), program2->flags & 0xFFFF);
    programReturnStackPushInt16(program2, VALUE_TYPE_INT);

    stackPushInt32(program2->stack, &(program2->stackPointer), (intptr_t)program2->field_7C);
    programReturnStackPushInt16(program2, VALUE_TYPE_INT);

    stackPushInt32(program2->stack, &(program2->stackPointer), program2->field_84);
    programReturnStackPushInt16(program2, VALUE_TYPE_INT);

    program2->flags &= ~0xFFFF;
    program2->instructionPointer = address;
    program2->field_84 = program1->field_84;

    program1->flags |= PROGRAM_FLAG_0x20;
}

// 0x46DB58
void _executeProc(Program* program, int procedure_index)
{
    Program* external_program;
    char* identifier;
    int address;
    int arguments_count;
    unsigned char* procedure_ptr;
    int flags;
    char err[256];
    Program* v12;

    procedure_ptr = program->procedures + 4 + 24 * procedure_index;
    flags = stackReadInt32(procedure_ptr, 4);
    if (!(flags & PROCEDURE_FLAG_IMPORTED)) {
        address = stackReadInt32(procedure_ptr, 16);

        _setupCallWithReturnVal(program, address, 20);

        programStackPushInt32(program, 0);
        programStackPushInt16(program, VALUE_TYPE_INT);

        if (!(flags & PROCEDURE_FLAG_CRITICAL)) {
            return;
        }

        program->flags |= PROGRAM_FLAG_CRITICAL_SECTION;
        v12 = program;
    } else {
        identifier = programGetIdentifier(program, stackReadInt32(procedure_ptr, 0));
        external_program = externalProcedureGetProgram(identifier, &address, &arguments_count);
        if (external_program == NULL) {
            sprintf(err, "External procedure %s not found\n", identifier);
            // TODO: Incomplete.
            // _interpretOutput(err);
            return;
        }

        if (arguments_count != 0) {
            sprintf(err, "External procedure cannot take arguments in interrupt context");
            // TODO: Incomplete.
            // _interpretOutput(err);
            return;
        }

        _setupExternalCallWithReturnVal(program, external_program, address, 28);

        programStackPushInt32(external_program, 0);
        programStackPushInt16(external_program, VALUE_TYPE_INT);

        procedure_ptr = external_program->procedures + 4 + 24 * procedure_index;
        flags = stackReadInt32(procedure_ptr, 4);

        if (!(flags & PROCEDURE_FLAG_CRITICAL)) {
            return;
        }

        external_program->flags |= PROGRAM_FLAG_CRITICAL_SECTION;
        v12 = external_program;
    }

    _interpret(v12, 0);
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
        int identifierOffset = stackReadInt32(ptr, offsetof(Procedure, field_0));
        if (compat_stricmp((char*)(program->identifiers + identifierOffset), name) == 0) {
            return index;
        }

        ptr += sizeof(Procedure);
    }

    return -1;
}

// 0x46DD2C
void _executeProcedure(Program* program, int procedure_index)
{
    Program* external_program;
    char* identifier;
    int address;
    int arguments_count;
    unsigned char* procedure_ptr;
    int flags;
    char err[256];
    jmp_buf jmp_buf;
    Program* v13;

    procedure_ptr = program->procedures + 4 + 24 * procedure_index;
    flags = stackReadInt32(procedure_ptr, 4);

    if (flags & 0x04) {
        identifier = programGetIdentifier(program, stackReadInt32(procedure_ptr, 0));
        external_program = externalProcedureGetProgram(identifier, &address, &arguments_count);
        if (external_program == NULL) {
            sprintf(err, "External procedure %s not found\n", identifier);
            // TODO: Incomplete.
            // _interpretOutput(err);
            return;
        }

        if (arguments_count != 0) {
            sprintf(err, "External procedure cannot take arguments in interrupt context");
            // TODO: Incomplete.
            // _interpretOutput(err);
            return;
        }

        _setupExternalCallWithReturnVal(program, external_program, address, 32);

        programStackPushInt32(external_program, 0);
        programStackPushInt16(external_program, VALUE_TYPE_INT);

        memcpy(jmp_buf, program->env, sizeof(jmp_buf));

        v13 = external_program;
    } else {
        address = stackReadInt32(procedure_ptr, 16);

        _setupCallWithReturnVal(program, address, 24);

        // Push number of arguments. It's always zero for built-in procs. This
        // number is consumed by 0x802B.
        programStackPushInt32(program, 0);
        programStackPushInt16(program, VALUE_TYPE_INT);

        memcpy(jmp_buf, program->env, sizeof(jmp_buf));

        v13 = program;
    }

    _interpret(v13, -1);

    memcpy(v13->env, jmp_buf, sizeof(jmp_buf));
}

// 0x46DEE4
void _doEvents()
{
    // TODO: Incomplete.
}

// 0x46E10C
void programListNodeFree(ProgramListNode* programListNode)
{
    ProgramListNode* tmp;

    tmp = programListNode->next;
    if (tmp != NULL) {
        tmp->prev = programListNode->prev;
    }

    tmp = programListNode->prev;
    if (tmp != NULL) {
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
    program->flags |= PROGRAM_FLAG_0x02;

    ProgramListNode* programListNode = (ProgramListNode*)internal_malloc_safe(sizeof(*programListNode), __FILE__, __LINE__); // .\\int\\INTRPRET.C, 2907
    programListNode->program = program;
    programListNode->next = gInterpreterProgramListHead;
    programListNode->prev = NULL;

    if (gInterpreterProgramListHead != NULL) {
        gInterpreterProgramListHead->prev = programListNode;
    }

    gInterpreterProgramListHead = programListNode;
}

// 0x46E1EC
void _updatePrograms()
{
    ProgramListNode* curr = gInterpreterProgramListHead;
    while (curr != NULL) {
        ProgramListNode* next = curr->next;
        if (curr->program != NULL) {
            _interpret(curr->program, _cpuBurstSize);
        }
        if (curr->program->exited) {
            programListNodeFree(curr);
        }
        curr = next;
    }
    _doEvents();
    _updateIntLib();
}

// 0x46E238
void programListFree()
{
    ProgramListNode* curr = gInterpreterProgramListHead;
    while (curr != NULL) {
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
void interpreterPrintStats()
{
    ProgramListNode* programListNode = gInterpreterProgramListHead;
    while (programListNode != NULL) {
        Program* program = programListNode->program;
        if (program != NULL) {
            int total = 0;

            if (program->dynamicStrings != NULL) {
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
