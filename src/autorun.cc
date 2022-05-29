#include "autorun.h"

#ifdef _WIN32
// 0x530010
HANDLE gInterplayGenericAutorunMutex;
#endif

// 0x4139C0
bool autorunMutexCreate()
{
#ifdef _WIN32
    gInterplayGenericAutorunMutex = CreateMutexA(NULL, FALSE, "InterplayGenericAutorunMutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(gInterplayGenericAutorunMutex);
        return false;
    }
#endif

    return true;
}

// 0x413A00
void autorunMutexClose()
{
#ifdef _WIN32
    if (gInterplayGenericAutorunMutex != NULL) {
        CloseHandle(gInterplayGenericAutorunMutex);
    }
#endif
}
