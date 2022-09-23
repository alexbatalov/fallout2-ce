#ifndef EXPORT_H
#define EXPORT_H

#include "interpreter.h"

namespace fallout {

int externalVariableSetValue(Program* program, const char* identifier, ProgramValue& value);
int externalVariableGetValue(Program* program, const char* name, ProgramValue& value);
int externalVariableCreate(Program* program, const char* identifier);
void _initExport();
void externalVariablesClear();
Program* externalProcedureGetProgram(const char* identifier, int* addressPtr, int* argumentCountPtr);
int externalProcedureCreate(Program* program, const char* identifier, int address, int argumentCount);
void _exportClearAllVariables();

} // namespace fallout

#endif /* EXPORT_H */
