#ifndef WIN32_H
#define WIN32_H

#include "dsound_compat.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#ifdef HAVE_DSOUND
typedef HRESULT(__stdcall DirectSoundCreateProc)(GUID*, LPDIRECTSOUND*, IUnknown*);
#endif

#ifdef HAVE_DSOUND
extern DirectSoundCreateProc* gDirectSoundCreateProc;
#endif
extern HWND gProgramWindow;
extern bool gProgramIsActive;
extern HANDLE _GNW95_mutex;
#ifdef HAVE_DSOUND
extern HMODULE gDSoundDLL;
#endif
bool _LoadDirectX();
void _UnloadDirectX(void);
#else
extern bool gProgramIsActive;
#endif

#endif /* WIN32_H */
