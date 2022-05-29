#include "win32.h"

#include "core.h"
#include "main.h"
#include "window_manager.h"

#include <SDL.h>
#include <stdlib.h>

#ifdef HAVE_DSOUND
// 0x51E430
DirectSoundCreateProc* gDirectSoundCreateProc = NULL;
#endif

// 0x51E434
HWND gProgramWindow = NULL;

// 0x51E444
bool gProgramIsActive = false;

// GNW95MUTEX
HANDLE _GNW95_mutex = NULL;

#ifdef HAVE_DSOUND
// 0x51E454
HMODULE gDSoundDLL = NULL;
#endif

// 0x4DE700
int main(int argc, char* argv[])
{
    _GNW95_mutex = CreateMutexA(0, TRUE, "GNW95MUTEX");
    if (GetLastError() == ERROR_SUCCESS) {
        SDL_ShowCursor(SDL_DISABLE);
        if (_LoadDirectX()) {
            gProgramIsActive = true;
            falloutMain(argc, argv);
        }
        CloseHandle(_GNW95_mutex);
    }
    return 0;
}

// 0x4DE8D0
bool _LoadDirectX()
{
#ifdef HAVE_DSOUND
    gDSoundDLL = LoadLibraryA("DSOUND.DLL");
    if (gDSoundDLL == NULL) {
        goto err;
    }

    gDirectSoundCreateProc = (DirectSoundCreateProc*)GetProcAddress(gDSoundDLL, "DirectSoundCreate");
    if (gDirectSoundCreateProc == NULL) {
        goto err;
    }
#endif

    atexit(_UnloadDirectX);

    return true;

err:
    _UnloadDirectX();

    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Could not load DirectX", "This program requires DirectX 3.0a or later.", NULL);

    return false;
}

// 0x4DE988
void _UnloadDirectX(void)
{
#ifdef HAVE_DSOUND
    if (gDSoundDLL != NULL) {
        FreeLibrary(gDSoundDLL);
        gDSoundDLL = NULL;
    }
#endif
}
