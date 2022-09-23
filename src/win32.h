#ifndef WIN32_H
#define WIN32_H

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace fallout {

#ifdef _WIN32
extern bool gProgramIsActive;
extern HANDLE _GNW95_mutex;
#else
extern bool gProgramIsActive;
#endif

} // namespace fallout

#endif /* WIN32_H */
