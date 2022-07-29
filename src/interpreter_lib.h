#ifndef INTERPRETER_LIB_H
#define INTERPRETER_LIB_H

#include "interpreter.h"

typedef void(IntLibProgramDeleteCallback)(Program*);

void intLibUpdate();
void intLibExit();
void intLibInit();
void intLibRegisterProgramDeleteCallback(IntLibProgramDeleteCallback* callback);
void intLibRemoveProgramReferences(Program* program);

#endif /* INTERPRETER_LIB_H */
