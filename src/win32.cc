#include "win32.h"

#include "args.h"
#include "core.h"
#include "main.h"
#include "window_manager.h"

#include <signal.h>
#include <SDL.h>

// 0x51E430
DirectSoundCreateProc* gDirectSoundCreateProc = NULL;

// 0x51E434
HWND gProgramWindow = NULL;

// 0x51E444
bool gProgramIsActive = false;

// GNW95MUTEX
HANDLE _GNW95_mutex = NULL;

// 0x51E454
HMODULE gDSoundDLL = NULL;

// 0x4DE700
int main(int argc, char* argv[])
{
    CommandLineArguments args;

    _GNW95_mutex = CreateMutexA(0, TRUE, "GNW95MUTEX");
    if (GetLastError() == ERROR_SUCCESS) {
        ShowCursor(0);
        if (_InitInstance()) {
            if (_LoadDirectX()) {
                argsInit(&args);
                if (argsParse(&args, argc, argv)) {
                    signal(1, _SignalHandler);
                    signal(3, _SignalHandler);
                    signal(5, _SignalHandler);
                    gProgramIsActive = true;
                    falloutMain(args.argc, args.argv);
                    argsFree(&args);
                    return 1;
                }
            }
        }
        CloseHandle(_GNW95_mutex);
    }
    return 0;
}

// 0x4DE864
bool _InitInstance()
{
    OSVERSIONINFOA osvi;
    bool result;

    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOA);

#pragma warning(suppress : 4996 28159)
    if (!GetVersionExA(&osvi)) {
        return true;
    }

    result = true;

    if (osvi.dwPlatformId == 0 || osvi.dwPlatformId == 2 && osvi.dwMajorVersion < 4) {
        result = false;
    }

    if (!result) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
            "Wrong Windows Version",
            "This program requires Windows 95 or Windows NT version 4.0 or greater.",
            NULL);
    }

    return result;
}

// 0x4DE8D0
bool _LoadDirectX()
{
    gDSoundDLL = LoadLibraryA("DSOUND.DLL");
    if (gDSoundDLL == NULL) {
        goto err;
    }

    gDirectSoundCreateProc = (DirectSoundCreateProc*)GetProcAddress(gDSoundDLL, "DirectSoundCreate");
    if (gDirectSoundCreateProc == NULL) {
        goto err;
    }

    atexit(_UnloadDirectX);

    return true;

err:
    _UnloadDirectX();

    MessageBoxA(NULL, "This program requires Windows 95 with DirectX 3.0a or later or Windows NT version 4.0 with Service Pack 3 or greater.", "Could not load DirectX", MB_ICONSTOP);

    return false;
}

// 0x4DE988
void _UnloadDirectX(void)
{
    if (gDSoundDLL != NULL) {
        FreeLibrary(gDSoundDLL);
        gDSoundDLL = NULL;
    }
}

// 0x4DE9F4
void _SignalHandler(int sig)
{
    // TODO: Incomplete.
}
