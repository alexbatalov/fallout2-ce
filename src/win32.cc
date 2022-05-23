#include "win32.h"

#include "args.h"
#include "core.h"
#include "main.h"
#include "window_manager.h"

#include <signal.h>

// 0x51E428
DirectDrawCreateProc* gDirectDrawCreateProc = NULL;

// 0x51E430
DirectSoundCreateProc* gDirectSoundCreateProc = NULL;

// 0x51E434
HWND gProgramWindow = NULL;

// 0x51E438
HINSTANCE gInstance = NULL;

// 0x51E43C
LPSTR gCmdLine = NULL;

// 0x51E440
int gCmdShow = 0;

// 0x51E444
bool gProgramIsActive = false;

// GNW95MUTEX
HANDLE _GNW95_mutex = NULL;

// 0x51E44C
HMODULE gDDrawDLL = NULL;

// 0x51E454
HMODULE gDSoundDLL = NULL;

// 0x4DE700
int WINAPI WinMain(_In_ HINSTANCE hInst, _In_opt_ HINSTANCE hPrevInst, _In_ LPSTR lpCmdLine, _In_ int nCmdShow)
{
    CommandLineArguments args;

    _GNW95_mutex = CreateMutexA(0, TRUE, "GNW95MUTEX");
    if (GetLastError() == ERROR_SUCCESS) {
        ShowCursor(0);
        if (_InitInstance()) {
            if (_LoadDirectX()) {
                gInstance = hInst;
                gCmdLine = lpCmdLine;
                gCmdShow = nCmdShow;
                argsInit(&args);
                if (argsParse(&args, lpCmdLine)) {
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
        MessageBoxA(NULL, "This program requires Windows 95 or Windows NT version 4.0 or greater.", "Wrong Windows Version", MB_ICONSTOP);
    }

    return result;
}

// 0x4DE8D0
bool _LoadDirectX()
{
    gDDrawDLL = LoadLibraryA("DDRAW.DLL");
    if (gDDrawDLL == NULL) {
        goto err;
    }

    gDirectDrawCreateProc = (DirectDrawCreateProc*)GetProcAddress(gDDrawDLL, "DirectDrawCreate");
    if (gDirectDrawCreateProc == NULL) {
        goto err;
    }

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
        gDirectDrawCreateProc = NULL;
    }

    if (gDDrawDLL != NULL) {
        FreeLibrary(gDDrawDLL);
        gDDrawDLL = NULL;
        gDirectSoundCreateProc = NULL;
    }
}

// 0x4DE9F4
void _SignalHandler(int sig)
{
    // TODO: Incomplete.
}
