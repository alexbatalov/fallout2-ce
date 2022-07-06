#ifndef NEVS_H
#define NEVS_H

#include "interpreter.h"

typedef enum NevsType {
    NEVS_TYPE_EVENT = 0,
    NEVS_TYPE_HANDLER = 1,
} NevsType;

void _nevs_close();
void _nevs_initonce();
int _nevs_addevent(const char* name, Program* program, int proc, int type);
int _nevs_clearevent(const char* name);
int _nevs_signal(const char* name);
void _nevs_update();

#endif /* NEVS_H */
