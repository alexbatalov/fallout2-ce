#ifndef EXPORT_H
#define EXPORT_H

#include "interpreter.h"

typedef struct ExternalVariable {
    char name[32];
    char* programName;
    opcode_t type;
    union {
        int value;
        char* stringValue;
    };
} ExternalVariable;

typedef struct ExternalProcedure {
    char name[32];
    Program* program;
    int argumentCount;
    int address;
} ExternalProcedure;

extern ExternalProcedure gExternalProcedures[1013];
extern ExternalVariable gExternalVariables[1013];

ExternalProcedure* externalProcedureFind(const char* identifier);
ExternalProcedure* externalProcedureAdd(const char* identifier);
ExternalVariable* externalVariableFind(const char* identifier);
ExternalVariable* externalVariableAdd(const char* identifier);
int externalVariableSetValue(Program* program, const char* identifier, opcode_t opcode, int data);
int externalVariableGetValue(Program* program, const char* name, opcode_t* opcodePtr, int* dataPtr);
int externalVariableCreate(Program* program, const char* identifier);
void _removeProgramReferences(Program* program);
void _initExport();
void externalVariablesClear();
Program* externalProcedureGetProgram(const char* identifier, int* addressPtr, int* argumentCountPtr);
int externalProcedureCreate(Program* program, const char* identifier, int address, int argumentCount);
void _exportClearAllVariables();

#endif /* EXPORT_H */
