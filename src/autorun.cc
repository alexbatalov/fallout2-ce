#include "autorun.h"

#ifdef _WIN32
#include <windows.h>
#endif

#ifdef _WIN32
// 0x530010
static HANDLE gInterplayGenericAutorunMutex;
#endif

namespace fallout {

// 0x4139C0
bool autorunMutexCreate()
{
#ifdef _WIN32
    gInterplayGenericAutorunMutex = CreateMutexA(nullptr, FALSE, "InterplayGenericAutorunMutex");
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
    if (gInterplayGenericAutorunMutex != nullptr) {
        CloseHandle(gInterplayGenericAutorunMutex);
    }
#endif
}

} // namespace fallout
