#ifndef WIN32_H
#define WIN32_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define DIRECTDRAW_VERSION 0x0300
#include <ddraw.h>

#include <mmreg.h>

#define DIRECTSOUND_VERSION 0x0300
#include <dsound.h>

#include <stdbool.h>

typedef HRESULT(__stdcall DirectDrawCreateProc)(GUID*, LPDIRECTDRAW*, IUnknown*);
typedef HRESULT(__stdcall DirectSoundCreateProc)(GUID*, LPDIRECTSOUND*, IUnknown*);

extern DirectDrawCreateProc* gDirectDrawCreateProc;
extern DirectSoundCreateProc* gDirectSoundCreateProc;
extern HWND gProgramWindow;
extern HINSTANCE gInstance;
extern LPSTR gCmdLine;
extern int gCmdShow;
extern bool gProgramIsActive;
extern HANDLE _GNW95_mutex;
extern HMODULE gDDrawDLL;
extern HMODULE gDSoundDLL;

bool _InitInstance();
bool _LoadDirectX();
void _UnloadDirectX(void);
void _SignalHandler(int sig);

#endif /* WIN32_H */
