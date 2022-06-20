#ifndef INTERPRETER_LIB_H
#define INTERPRETER_LIB_H

#include "interpreter.h"

typedef void (*OFF_59E160)(Program*);

void _updateIntLib();
void _intlibClose();
void _initIntlib();
void _interpretRegisterProgramDeleteCallback(OFF_59E160 fn);
void _removeProgramReferences_(Program* program);

#endif /* INTERPRETER_LIB_H */
