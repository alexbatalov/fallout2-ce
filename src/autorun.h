#ifndef AUTORUN_H
#define AUTORUN_H

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

extern HANDLE gInterplayGenericAutorunMutex;
#endif

bool autorunMutexCreate();
void autorunMutexClose();

#endif /* AUTORUN_H */
