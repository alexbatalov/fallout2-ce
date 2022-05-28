#ifndef WIN32_H
#define WIN32_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <mmreg.h>

#define DIRECTSOUND_VERSION 0x0300
#include <dsound.h>

typedef HRESULT(__stdcall DirectSoundCreateProc)(GUID*, LPDIRECTSOUND*, IUnknown*);

extern DirectSoundCreateProc* gDirectSoundCreateProc;
extern HWND gProgramWindow;
extern bool gProgramIsActive;
extern HANDLE _GNW95_mutex;
extern HMODULE gDSoundDLL;

bool _LoadDirectX();
void _UnloadDirectX(void);

#endif /* WIN32_H */
