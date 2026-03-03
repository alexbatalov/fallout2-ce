#ifndef XBOX_DEBUG_H
#define XBOX_DEBUG_H

#ifdef NXDK
// On Xbox/NXDK: pull in the real kernel header (provides DbgPrint, etc.)
#include "xboxkrnl/xboxkrnl.h"
#else
// On all other platforms: DbgPrint is a no-op variadic macro.
#define DbgPrint(...) ((void)0)
#endif

#endif /* XBOX_DEBUG_H */
