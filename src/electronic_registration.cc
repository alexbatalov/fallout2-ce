#include "electronic_registration.h"

#include "game_config.h"
#include "platform_compat.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// 0x440DD0
void runElectronicRegistration()
{
    int timesRun = 0;
    configGetInt(&gGameConfig, GAME_CONFIG_SYSTEM_KEY, GAME_CONFIG_TIMES_RUN_KEY, &timesRun);
    if (timesRun > 0 && timesRun < 5) {
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

        configSetInt(&gGameConfig, GAME_CONFIG_SYSTEM_KEY, GAME_CONFIG_TIMES_RUN_KEY, timesRun + 1);
    } else {
        if (timesRun == 0) {
            configSetInt(&gGameConfig, GAME_CONFIG_SYSTEM_KEY, GAME_CONFIG_TIMES_RUN_KEY, 1);
        }
    }
}
