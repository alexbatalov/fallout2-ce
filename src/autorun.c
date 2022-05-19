#include "autorun.h"

// 0x530010
HANDLE gInterplayGenericAutorunMutex;

// 0x4139C0
bool autorunMutexCreate()
{
    gInterplayGenericAutorunMutex = CreateMutexA(NULL, FALSE, "InterplayGenericAutorunMutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(gInterplayGenericAutorunMutex);
        return false;
    }

    return true;
}

// 0x413A00
void autorunMutexClose()
{
    if (gInterplayGenericAutorunMutex != NULL) {
        CloseHandle(gInterplayGenericAutorunMutex);
    }
}
