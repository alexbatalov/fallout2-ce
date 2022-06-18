#include "memory.h"

#include "debug.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static void* memoryBlockMallocImpl(size_t size);
static void* memoryBlockReallocImpl(void* ptr, size_t size);
static void memoryBlockFreeImpl(void* ptr);
static void memoryBlockPrintStats();
static void memoryBlockValidate(void* block);

// 0x51DED0
static MallocProc* gMallocProc = memoryBlockMallocImpl;

// 0x51DED4
static ReallocProc* gReallocProc = memoryBlockReallocImpl;

// 0x51DED8
static FreeProc* gFreeProc = memoryBlockFreeImpl;

// 0x51DEDC
static int gMemoryBlocksCurrentCount = 0;

// 0x51DEE0
static int gMemoryBlockMaximumCount = 0;

// 0x51DEE4
static size_t gMemoryBlocksCurrentSize = 0;

// 0x51DEE8
static size_t gMemoryBlocksMaximumSize = 0;

// 0x4C5A80
char* internal_strdup(const char* string)
{
    char* copy = NULL;
    if (string != NULL) {
        copy = (char*)gMallocProc(strlen(string) + 1);
        strcpy(copy, string);
    }
    return copy;
}

// 0x4C5AD0
void* internal_malloc(size_t size)
{
    return gMallocProc(size);
}

// 0x4C5AD8
static void* memoryBlockMallocImpl(size_t size)
{
    void* ptr = NULL;

    if (size != 0) {
        size += sizeof(MemoryBlockHeader) + sizeof(MemoryBlockFooter);

        unsigned char* block = (unsigned char*)malloc(size);
        if (block != NULL) {
            MemoryBlockHeader* header = (MemoryBlockHeader*)block;
            header->size = size;
            header->guard = MEMORY_BLOCK_HEADER_GUARD;

            MemoryBlockFooter* footer = (MemoryBlockFooter*)(block + size - sizeof(MemoryBlockFooter));
            footer->guard = MEMORY_BLOCK_FOOTER_GUARD;

            gMemoryBlocksCurrentCount++;
            if (gMemoryBlocksCurrentCount > gMemoryBlockMaximumCount) {
                gMemoryBlockMaximumCount = gMemoryBlocksCurrentCount;
            }

            gMemoryBlocksCurrentSize += size;
            if (gMemoryBlocksCurrentSize > gMemoryBlocksMaximumSize) {
                gMemoryBlocksMaximumSize = gMemoryBlocksCurrentSize;
            }

            ptr = block + sizeof(MemoryBlockHeader);
        }
    }

    return ptr;
}

// 0x4C5B50
void* internal_realloc(void* ptr, size_t size)
{
    return gReallocProc(ptr, size);
}

// 0x4C5B58
static void* memoryBlockReallocImpl(void* ptr, size_t size)
{
    if (ptr != NULL) {
        unsigned char* block = (unsigned char*)ptr - sizeof(MemoryBlockHeader);

        MemoryBlockHeader* header = (MemoryBlockHeader*)block;
        size_t oldSize = header->size;

        gMemoryBlocksCurrentSize -= oldSize;

        memoryBlockValidate(block);

        if (size != 0) {
            size += sizeof(MemoryBlockHeader) + sizeof(MemoryBlockFooter);
        }

        unsigned char* newBlock = (unsigned char*)realloc(block, size);
        if (newBlock != NULL) {
            MemoryBlockHeader* newHeader = (MemoryBlockHeader*)newBlock;
            newHeader->size = size;
            newHeader->guard = MEMORY_BLOCK_HEADER_GUARD;

            MemoryBlockFooter* newFooter = (MemoryBlockFooter*)(newBlock + size - sizeof(MemoryBlockFooter));
            newFooter->guard = MEMORY_BLOCK_FOOTER_GUARD;

            gMemoryBlocksCurrentSize += size;
            if (gMemoryBlocksCurrentSize > gMemoryBlocksMaximumSize) {
                gMemoryBlocksMaximumSize = gMemoryBlocksCurrentSize;
            }

            ptr = newBlock + sizeof(MemoryBlockHeader);
        } else {
            if (size != 0) {
                gMemoryBlocksCurrentSize += oldSize;

                debugPrint("%s,%u: ", __FILE__, __LINE__); // "Memory.c", 195
                debugPrint("Realloc failure.\n");
            } else {
                gMemoryBlocksCurrentCount--;
            }
            ptr = NULL;
        }
    } else {
        ptr = gMallocProc(size);
    }

    return ptr;
}

// 0x4C5C24
void internal_free(void* ptr)
{
    gFreeProc(ptr);
}

// 0x4C5C2C
static void memoryBlockFreeImpl(void* ptr)
{
    if (ptr != NULL) {
        void* block = (unsigned char*)ptr - sizeof(MemoryBlockHeader);
        MemoryBlockHeader* header = (MemoryBlockHeader*)block;

        memoryBlockValidate(block);

        gMemoryBlocksCurrentSize -= header->size;
        gMemoryBlocksCurrentCount--;

        free(block);
    }
}

// NOTE: Not used.
//
// 0x4C5C5C
static void memoryBlockPrintStats()
{
    if (gMallocProc == memoryBlockMallocImpl) {
        debugPrint("Current memory allocated: %6d blocks, %9u bytes total\n", gMemoryBlocksCurrentCount, gMemoryBlocksCurrentSize);
        debugPrint("Max memory allocated:     %6d blocks, %9u bytes total\n", gMemoryBlockMaximumCount, gMemoryBlocksMaximumSize);
    }
}

// Validates integrity of the memory block.
//
// [block] is a pointer to the the memory block itself, not it's data.
//
// 0x4C5CE4
static void memoryBlockValidate(void* block)
{
    MemoryBlockHeader* header = (MemoryBlockHeader*)block;
    if (header->guard != MEMORY_BLOCK_HEADER_GUARD) {
        debugPrint("Memory header stomped.\n");
    }

    MemoryBlockFooter* footer = (MemoryBlockFooter*)((unsigned char*)block + header->size - sizeof(MemoryBlockFooter));
    if (footer->guard != MEMORY_BLOCK_FOOTER_GUARD) {
        debugPrint("Memory footer stomped.\n");
    }
}
