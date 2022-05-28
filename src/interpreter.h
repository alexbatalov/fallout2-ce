#ifndef INTERPRETER_H
#define INTERPRETER_H

#include <setjmp.h>

// The maximum number of opcodes.
#define OPCODE_MAX_COUNT (342)

typedef enum Opcode {
    OPCODE_NOOP = 0x8000,
    OPCODE_PUSH = 0x8001,
    OPCODE_ENTER_CRITICAL_SECTION = 0x8002,
    OPCODE_LEAVE_CRITICAL_SECTION = 0x8003,
    OPCODE_JUMP = 0x8004,
    OPCODE_CALL = 0x8005,
    OPCODE_CALL_AT = 0x8006,
    OPCODE_CALL_WHEN = 0x8007,
    OPCODE_CALLSTART = 0x8008,
    OPCODE_EXEC = 0x8009,
    OPCODE_SPAWN = 0x800A,
    OPCODE_FORK = 0x800B,
    OPCODE_A_TO_D = 0x800C,
    OPCODE_D_TO_A = 0x800D,
    OPCODE_EXIT = 0x800E,
    OPCODE_DETACH = 0x800F,
    OPCODE_EXIT_PROGRAM = 0x8010,
    OPCODE_STOP_PROGRAM = 0x8011,
    OPCODE_FETCH_GLOBAL = 0x8012,
    OPCODE_STORE_GLOBAL = 0x8013,
    OPCODE_FETCH_EXTERNAL = 0x8014,
    OPCODE_STORE_EXTERNAL = 0x8015,
    OPCODE_EXPORT_VARIABLE = 0x8016,
    OPCODE_EXPORT_PROCEDURE = 0x8017,
    OPCODE_SWAP = 0x8018,
    OPCODE_SWAPA = 0x8019,
    OPCODE_POP = 0x801A,
    OPCODE_DUP = 0x801B,
    OPCODE_POP_RETURN = 0x801C,
    OPCODE_POP_EXIT = 0x801D,
    OPCODE_POP_ADDRESS = 0x801E,
    OPCODE_POP_FLAGS = 0x801F,
    OPCODE_POP_FLAGS_RETURN = 0x8020,
    OPCODE_POP_FLAGS_EXIT = 0x8021,
    OPCODE_POP_FLAGS_RETURN_EXTERN = 0x8022,
    OPCODE_POP_FLAGS_EXIT_EXTERN = 0x8023,
    OPCODE_POP_FLAGS_RETURN_VAL_EXTERN = 0x8024,
    OPCODE_POP_FLAGS_RETURN_VAL_EXIT = 0x8025,
    OPCODE_POP_FLAGS_RETURN_VAL_EXIT_EXTERN = 0x8026,
    OPCODE_CHECK_PROCEDURE_ARGUMENT_COUNT = 0x8027,
    OPCODE_LOOKUP_PROCEDURE_BY_NAME = 0x8028,
    OPCODE_POP_BASE = 0x8029,
    OPCODE_POP_TO_BASE = 0x802A,
    OPCODE_PUSH_BASE = 0x802B,
    OPCODE_SET_GLOBAL = 0x802C,
    OPCODE_FETCH_PROCEDURE_ADDRESS = 0x802D,
    OPCODE_DUMP = 0x802E,
    OPCODE_IF = 0x802F,
    OPCODE_WHILE = 0x8030,
    OPCODE_STORE = 0x8031,
    OPCODE_FETCH = 0x8032,
    OPCODE_EQUAL = 0x8033,
    OPCODE_NOT_EQUAL = 0x8034,
    OPCODE_LESS_THAN_EQUAL = 0x8035,
    OPCODE_GREATER_THAN_EQUAL = 0x8036,
    OPCODE_LESS_THAN = 0x8037,
    OPCODE_GREATER_THAN = 0x8038,
    OPCODE_ADD = 0x8039,
    OPCODE_SUB = 0x803A,
    OPCODE_MUL = 0x803B,
    OPCODE_DIV = 0x803C,
    OPCODE_MOD = 0x803D,
    OPCODE_AND = 0x803E,
    OPCODE_OR = 0x803F,
    OPCODE_BITWISE_AND = 0x8040,
    OPCODE_BITWISE_OR = 0x8041,
    OPCODE_BITWISE_XOR = 0x8042,
    OPCODE_BITWISE_NOT = 0x8043,
    OPCODE_FLOOR = 0x8044,
    OPCODE_NOT = 0x8045,
    OPCODE_NEGATE = 0x8046,
    OPCODE_WAIT = 0x8047,
    OPCODE_CANCEL = 0x8048,
    OPCODE_CANCEL_ALL = 0x8049,
    OPCODE_START_CRITICAL = 0x804A,
    OPCODE_END_CRITICAL = 0x804B,
} Opcode;

typedef enum ProcedureFlags {
    PROCEDURE_FLAG_TIMED = 0x01,
    PROCEDURE_FLAG_CONDITIONAL = 0x02,
    PROCEDURE_FLAG_IMPORTED = 0x04,
    PROCEDURE_FLAG_EXPORTED = 0x08,
    PROCEDURE_FLAG_CRITICAL = 0x10,
} ProcedureFlags;

typedef enum ProgramFlags {
    PROGRAM_FLAG_EXITED = 0x01,
    PROGRAM_FLAG_0x02 = 0x02,
    PROGRAM_FLAG_0x04 = 0x04,
    PROGRAM_FLAG_STOPPED = 0x08,
    PROGRAM_FLAG_0x10 = 0x10,
    PROGRAM_FLAG_0x20 = 0x20,
    PROGRAM_FLAG_0x40 = 0x40,
    PROGRAM_FLAG_CRITICAL_SECTION = 0x80,
    PROGRAM_FLAG_0x0100 = 0x0100,
} ProgramFlags;

enum RawValueType {
    RAW_VALUE_TYPE_OPCODE = 0x8000,
    RAW_VALUE_TYPE_INT = 0x4000,
    RAW_VALUE_TYPE_FLOAT = 0x2000,
    RAW_VALUE_TYPE_STATIC_STRING = 0x1000,
    RAW_VALUE_TYPE_DYNAMIC_STRING = 0x0800,
};

#define VALUE_TYPE_MASK 0xF7FF

#define VALUE_TYPE_INT 0xC001
#define VALUE_TYPE_FLOAT 0xA001
#define VALUE_TYPE_STRING 0x9001
#define VALUE_TYPE_DYNAMIC_STRING 0x9801

typedef unsigned short opcode_t;

typedef struct Procedure {
    int field_0;
    int field_4;
    int field_8;
    int field_C;
    int field_10;
    int field_14;
} Procedure;

// It's size in original code is 144 (0x8C) bytes due to the different
// size of `jmp_buf`.
typedef struct Program {
    char* name;
    unsigned char* data;
    struct Program* parent;
    struct Program* child;
    int instructionPointer; // current pos in data
    int framePointer; // saved stack 1 pos - probably beginning of local variables - probably called base
    int basePointer; // saved stack 1 pos - probably beginning of global variables
    unsigned char* stack; // stack 1 (4096 bytes)
    unsigned char* returnStack; // stack 2 (4096 bytes)
    int stackPointer; // stack pointer 1
    int returnStackPointer; // stack pointer 2
    unsigned char* staticStrings; // static strings table
    unsigned char* dynamicStrings; // dynamic strings table
    unsigned char* identifiers;
    unsigned char* procedures;
    jmp_buf env;
    int field_70; // end time of timer (field_74 + wait time)
    int field_74; // time when wait was called
    int field_78; // time when program begin execution (for the first time)?, -1 - program never executed
    int (*field_7C)(struct Program* s); // proc to check timer
    int flags; // flags
    int field_84;
    bool exited;
} Program;

typedef void OpcodeHandler(Program* program);

typedef struct ProgramListNode {
    Program* program;
    struct ProgramListNode* next; // next
    struct ProgramListNode* prev; // prev
} ProgramListNode;

extern int _TimeOut;
extern int _Enabled;
extern int (*_timerFunc)();
extern int _timerTick;
extern char* (*_filenameFunc)(char*);
extern int (*_outputFunc)(char*);
extern int _cpuBurstSize;

extern OpcodeHandler* gInterpreterOpcodeHandlers[OPCODE_MAX_COUNT];
extern Program* gInterpreterCurrentProgram;
extern ProgramListNode* gInterpreterProgramListHead;
extern int _suspendEvents;
extern int _busy;

int _defaultTimerFunc();
char* _defaultFilename_(char* s);
char* _interpretMangleName(char* s);
int _outputStr(char* a1);
int _checkWait(Program* program);
void _interpretOutputFunc(int (*func)(char*));
int _interpretOutput(const char* format, ...);
char* programGetCurrentProcedureName(Program* s);
[[noreturn]] void programFatalError(const char* str, ...);
opcode_t stackReadInt16(unsigned char* data, int pos);
int stackReadInt32(unsigned char* a1, int a2);
void stackWriteInt16(int value, unsigned char* a2, int a3);
void stackWriteInt32(int value, unsigned char* stack, int pos);
void stackPushInt16(unsigned char* a1, int* a2, int value);
void stackPushInt32(unsigned char* a1, int* a2, int value);
int stackPopInt32(unsigned char* a1, int* a2);
opcode_t stackPopInt16(unsigned char* a1, int* a2);
void programPopString(Program* program, opcode_t a2, int a3);
void programStackPushInt16(Program* program, int value);
void programStackPushInt32(Program* program, int value);
opcode_t programStackPopInt16(Program* program);
int programStackPopInt32(Program* program);
void programReturnStackPushInt16(Program* program, int value);
opcode_t programReturnStackPopInt16(Program* program);
int programReturnStackPopInt32(Program* program);
void _detachProgram(Program* program);
void _purgeProgram(Program* program);
void programFree(Program* program);
Program* programCreateByPath(const char* path);
char* programGetString(Program* program, opcode_t opcode, int offset);
char* programGetIdentifier(Program* program, int offset);
void programMarkHeap(Program* program);
int programPushString(Program* program, char* string);
void opNoop(Program* program);
void opPush(Program* program);
void opPushBase(Program* program);
void opPopBase(Program* program);
void opPopToBase(Program* program);
void op802C(Program* program);
void opDump(Program* program);
void opDelayedCall(Program* program);
void opConditionalCall(Program* program);
void opWait(Program* program);
void opCancel(Program* program);
void opCancelAll(Program* program);
void opIf(Program* program);
void opWhile(Program* program);
void opStore(Program* program);
void opFetch(Program* program);
void opConditionalOperatorNotEqual(Program* program);
void opConditionalOperatorEqual(Program* program);
void opConditionalOperatorLessThanEquals(Program* program);
void opConditionalOperatorGreaterThanEquals(Program* program);
void opConditionalOperatorLessThan(Program* program);
void opConditionalOperatorGreaterThan(Program* program);
void opAdd(Program* program);
void opSubtract(Program* program);
void opMultiply(Program* program);
void opDivide(Program* program);
void opModulo(Program* program);
void opLogicalOperatorAnd(Program* program);
void opLogicalOperatorOr(Program* program);
void opLogicalOperatorNot(Program* program);
void opUnaryMinus(Program* program);
void opBitwiseOperatorNot(Program* program);
void opFloor(Program* program);
void opBitwiseOperatorAnd(Program* program);
void opBitwiseOperatorOr(Program* program);
void opBitwiseOperatorXor(Program* program);
void opSwapReturnStack(Program* program);
void opLeaveCriticalSection(Program* program);
void opEnterCriticalSection(Program* program);
void opJump(Program* program);
void opCall(Program* program);
void op801F(Program* program);
void op801C(Program* program);
void op801D(Program* program);
void op8020(Program* program);
void op8021(Program* program);
void op8025(Program* program);
void op8026(Program* program);
void op8022(Program* program);
void op8023(Program* program);
void op8024(Program* program);
void op801E(Program* program);
void opAtoD(Program* program);
void opDtoA(Program* program);
void opExitProgram(Program* program);
void opStopProgram(Program* program);
void opFetchGlobalVariable(Program* program);
void opStoreGlobalVariable(Program* program);
void opSwapStack(Program* program);
void opFetchProcedureAddress(Program* program);
void opPop(Program* program);
void opDuplicate(Program* program);
void opStoreExternalVariable(Program* program);
void opFetchExternalVariable(Program* program);
void opExportProcedure(Program* program);
void opExportVariable(Program* program);
void opExit(Program* program);
void opDetach(Program* program);
void opCallStart(Program* program);
void opSpawn(Program* program);
Program* forkProgram(Program* program);
void opFork(Program* program);
void opExec(Program* program);
void opCheckProcedureArgumentCount(Program* program);
void opLookupStringProc(Program* program);
void interpreterRegisterOpcodeHandlers();
void _interpretClose();
void _interpret(Program* program, int a2);
void _setupCallWithReturnVal(Program* program, int address, int a3);
void _setupExternalCallWithReturnVal(Program* program1, Program* program2, int address, int a4);
void _executeProc(Program* program, int procedure_index);
int programFindProcedure(Program* prg, const char* name);
void _executeProcedure(Program* program, int procedure_index);
void _doEvents();
void programListNodeFree(ProgramListNode* programListNode);
void programListNodeCreate(Program* program);
void _updatePrograms();
void programListFree();
void interpreterRegisterOpcode(int opcode, OpcodeHandler* handler);
void interpreterPrintStats();

#endif /* INTERPRETER_H */
