#ifndef NEVS_H
#define NEVS_H

#include "interpreter.h"

#define NEVS_COUNT (40)

typedef struct Nevs {
    int field_0;
    char field_4[32];
    Program* program;
    int proc;
    int field_2C;
    int field_30;
    int field_34;
    void (*field_38)();
} Nevs;

extern Nevs* _nevs;

extern int _anyhits;

Nevs* _nevs_alloc();
void _nevs_close();
void _nevs_removeprogramreferences(Program* program);
void _nevs_initonce();
Nevs* _nevs_find(const char* a1);
int _nevs_addevent(const char* a1, Program* program, int proc, int a4);
int _nevs_clearevent(const char* a1);
int _nevs_signal(const char* a1);
void _nevs_update();

#endif /* NEVS_H */
