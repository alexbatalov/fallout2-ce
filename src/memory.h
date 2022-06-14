#ifndef MEMORY_H
#define MEMORY_H

#include "memory_defs.h"

// A special value that denotes a beginning of a memory block data.
#define MEMORY_BLOCK_HEADER_GUARD (0xFEEDFACE)

// A special value that denotes an ending of a memory block data.
#define MEMORY_BLOCK_FOOTER_GUARD (0xBEEFCAFE)

// A header of a memory block.
typedef struct MemoryBlockHeader {
    // Size of the memory block including header and footer.
    size_t size;

    // See [MEMORY_BLOCK_HEADER_GUARD].
    int guard;
} MemoryBlockHeader;

// A footer of a memory block.
typedef struct MemoryBlockFooter {
    // See [MEMORY_BLOCK_FOOTER_GUARD].
    int guard;
} MemoryBlockFooter;

extern MallocProc* gMallocProc;
extern ReallocProc* gReallocProc;
extern FreeProc* gFreeProc;
extern int gMemoryBlocksCurrentCount;
extern int gMemoryBlockMaximumCount;
extern size_t gMemoryBlocksCurrentSize;
extern size_t gMemoryBlocksMaximumSize;

char* internal_strdup(const char* string);
void* internal_malloc(size_t size);
void* memoryBlockMallocImpl(size_t size);
void* internal_realloc(void* ptr, size_t size);
void* memoryBlockReallocImpl(void* ptr, size_t size);
void internal_free(void* ptr);
void memoryBlockFreeImpl(void* ptr);
void memoryBlockPrintStats();
void memoryBlockValidate(void* block);

#endif /* MEMORY_H */
