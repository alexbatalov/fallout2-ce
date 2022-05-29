#ifndef DSOUND_COMPAT_H
#define DSOUND_COMPAT_H

#ifdef _WIN32
#define HAVE_DSOUND 1

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <mmreg.h>

#define DIRECTSOUND_VERSION 0x0300
#include <dsound.h>
#endif

#endif /* DSOUND_COMPAT_H */
