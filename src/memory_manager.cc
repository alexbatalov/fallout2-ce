#include "memory_manager.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 0x519588
MemoryManagerPrintErrorProc* gMemoryManagerPrintErrorProc = memoryManagerDefaultPrintErrorImpl;

// 0x51958C
MallocProc* gMemoryManagerMallocProc = memoryManagerDefaultMallocImpl;

// 0x519590
ReallocProc* gMemoryManagerReallocProc = memoryManagerDefaultReallocImpl;

// 0x519594
FreeProc* gMemoryManagerFreeProc = memoryManagerDefaultFreeImpl;

// 0x631F7C
char gMemoryManagerLastError[256];

// 0x4845B0
void memoryManagerDefaultPrintErrorImpl(const char* string)
{
    printf("%s", string);
}

// 0x4845C8
int memoryManagerPrintError(const char* format, ...)
{
    int length = 0;

    if (gMemoryManagerPrintErrorProc != NULL) {
        va_list args;
        va_start(args, format);
        length = vsprintf(gMemoryManagerLastError, format, args);
        va_end(args);

        gMemoryManagerPrintErrorProc(gMemoryManagerLastError);
    }

    return length;
}

// 0x484610
[[noreturn]] void memoryManagerFatalAllocationError(const char* func, size_t size, const char* file, int line)
{
    memoryManagerPrintError("%s: Error allocating block of size %ld (%x), %s %d\n", func, size, size, file, line);
    exit(1);
}

// 0x48462C
void* memoryManagerDefaultMallocImpl(size_t size)
{
    return malloc(size);
}

// 0x484634
void* memoryManagerDefaultReallocImpl(void* ptr, size_t size)
{
    return realloc(ptr, size);
}

// 0x48463C
void memoryManagerDefaultFreeImpl(void* ptr)
{
    free(ptr);
}

// 0x484644
void memoryManagerSetProcs(MallocProc* mallocProc, ReallocProc* reallocProc, FreeProc* freeProc)
{
    gMemoryManagerMallocProc = mallocProc;
    gMemoryManagerReallocProc = reallocProc;
    gMemoryManagerFreeProc = freeProc;
}

// 0x484660
void* internal_malloc_safe(size_t size, const char* file, int line)
{
    void* ptr = gMemoryManagerMallocProc(size);
    if (ptr == NULL) {
        memoryManagerFatalAllocationError("malloc", size, file, line);
    }

    return ptr;
}

// 0x4846B4
void* internal_realloc_safe(void* ptr, size_t size, const char* file, int line)
{
    ptr = gMemoryManagerReallocProc(ptr, size);
    if (ptr == NULL) {
        memoryManagerFatalAllocationError("realloc", size, file, line);
    }

    return ptr;
}

// 0x484688
void internal_free_safe(void* ptr, const char* file, int line)
{
    if (ptr == NULL) {
        memoryManagerPrintError("free: free of a null ptr, %s %d\n", file, line);
        exit(1);
    }

    gMemoryManagerFreeProc(ptr);
}

// 0x4846D8
void* internal_calloc_safe(int count, int size, const char* file, int line)
{
    void* ptr = gMemoryManagerMallocProc(count * size);
    if (ptr == NULL) {
        memoryManagerFatalAllocationError("calloc", size, file, line);
    }

    memset(ptr, 0, count * size);

    return ptr;
}

// 0x484710
char* strdup_safe(const char* string, const char* file, int line)
{
    size_t size = strlen(string) + 1;
    char* copy = (char*)gMemoryManagerMallocProc(size);
    if (copy == NULL) {
        memoryManagerFatalAllocationError("strdup", size, file, line);
    }

    strcpy(copy, string);

    return copy;
}
