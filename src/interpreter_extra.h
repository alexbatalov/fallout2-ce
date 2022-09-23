#ifndef INTERPRETER_EXTRA_H
#define INTERPRETER_EXTRA_H

#include "interpreter.h"

namespace fallout {

void _intExtraClose_();
void _initIntExtra();
void intExtraUpdate();
void intExtraRemoveProgramReferences(Program* program);

} // namespace fallout

#endif /* INTERPRETER_EXTRA_H */
