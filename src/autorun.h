#ifndef AUTORUN_H
#define AUTORUN_H

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

extern HANDLE gInterplayGenericAutorunMutex;

bool autorunMutexCreate();
void autorunMutexClose();

#endif /* AUTORUN_H */
