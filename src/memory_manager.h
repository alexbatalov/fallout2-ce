#ifndef MEMORY_MANAGER_H
#define MEMORY_MANAGER_H

#include "memory_defs.h"

typedef void(MemoryManagerPrintErrorProc)(const char* string);

extern MemoryManagerPrintErrorProc* gMemoryManagerPrintErrorProc;
extern MallocProc* gMemoryManagerMallocProc;
extern ReallocProc* gMemoryManagerReallocProc;
extern FreeProc* gMemoryManagerFreeProc;
extern char gMemoryManagerLastError[256];

extern char gMemoryManagerLastError[256];

void memoryManagerDefaultPrintErrorImpl(const char* string);
int memoryManagerPrintError(const char* format, ...);
[[noreturn]] void memoryManagerFatalAllocationError(const char* func, size_t size, const char* file, int line);
void* memoryManagerDefaultMallocImpl(size_t size);
void* memoryManagerDefaultReallocImpl(void* ptr, size_t size);
void memoryManagerDefaultFreeImpl(void* ptr);
void memoryManagerSetProcs(MallocProc* mallocProc, ReallocProc* reallocProc, FreeProc* freeProc);
void* internal_malloc_safe(size_t size, const char* file, int line);
void* internal_realloc_safe(void* ptr, size_t size, const char* file, int line);
void internal_free_safe(void* ptr, const char* file, int line);
void* internal_calloc_safe(int count, int size, const char* file, int line);
char* strdup_safe(const char* string, const char* file, int line);

#endif /* MEMORY_MANAGER_H */
