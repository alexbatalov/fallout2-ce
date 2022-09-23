#ifndef DEBUG_H
#define DEBUG_H

namespace fallout {

typedef int(DebugPrintProc)(char* string);

void _GNW_debug_init();
void _debug_register_mono();
void _debug_register_log(const char* fileName, const char* mode);
void _debug_register_screen();
void _debug_register_env();
void _debug_register_func(DebugPrintProc* proc);
int debugPrint(const char* format, ...);
void _debug_exit(void);

} // namespace fallout

#endif /* DEBUG_H */
