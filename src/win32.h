#ifndef WIN32_H
#define WIN32_H

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

extern bool gProgramIsActive;
extern HANDLE _GNW95_mutex;
#else
extern bool gProgramIsActive;
#endif

#endif /* WIN32_H */
