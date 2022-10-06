#include "electronic_registration.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "platform_compat.h"
#include "settings.h"

namespace fallout {

// 0x440DD0
void runElectronicRegistration()
{
    int timesRun = settings.system.times_run;
    if (timesRun > 0 && timesRun < 5) {
#ifdef _WIN32
        char path[COMPAT_MAX_PATH];
        if (GetModuleFileNameA(NULL, path, sizeof(path)) != 0) {
            char* pch = strrchr(path, '\\');
            if (pch == NULL) {
                pch = path;
            }

            strcpy(pch, "\\ereg");

            STARTUPINFOA startupInfo;
            memset(&startupInfo, 0, sizeof(startupInfo));
            startupInfo.cb = sizeof(startupInfo);

            PROCESS_INFORMATION processInfo;

            // FIXME: Leaking processInfo.hProcess and processInfo.hThread:
            // https://docs.microsoft.com/en-us/cpp/code-quality/c6335.
            if (CreateProcessA("ereg\\reg32a.exe", NULL, NULL, NULL, FALSE, 0, NULL, path, &startupInfo, &processInfo)) {
                WaitForSingleObject(processInfo.hProcess, INFINITE);
            }
        }
#endif

        settings.system.times_run = timesRun + 1;
    } else {
        if (timesRun == 0) {
            settings.system.times_run = 1;
        }
    }
}

} // namespace fallout
