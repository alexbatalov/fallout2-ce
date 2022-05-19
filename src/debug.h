#ifndef DEBUG_H
#define DEBUG_H

#include <stdio.h>

typedef int(DebugPrintProc)(char* string);

extern FILE* _fd;
extern int _curx;
extern int _cury;
extern DebugPrintProc* gDebugPrintProc;

void _GNW_debug_init();
void _debug_register_mono();
void _debug_register_log(const char* fileName, const char* mode);
void _debug_register_screen();
void _debug_register_env();
void _debug_register_func(DebugPrintProc* proc);
int debugPrint(const char* format, ...);
int _debug_puts(char* string);
void _debug_clear();
int _debug_mono(char* string);
int _debug_log(char* string);
int _debug_screen(char* string);
void _debug_putc();
void _debug_exit(void);

#endif /* DEBUG_H */
