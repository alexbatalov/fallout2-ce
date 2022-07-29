#ifndef INTERPRETER_EXTRA_H
#define INTERPRETER_EXTRA_H

#include "interpreter.h"

void _intExtraClose_();
void _initIntExtra();
void intExtraUpdate();
void intExtraRemoveProgramReferences(Program* program);

#endif /* INTERPRETER_EXTRA_H */
