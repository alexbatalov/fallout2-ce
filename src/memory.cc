#include "memory.h"

#include "debug.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 0x51DED0
MallocProc* gMallocProc = memoryBlockMallocImpl;

// 0x51DED4
ReallocProc* gReallocProc = memoryBlockReallocImpl;

// 0x51DED8
FreeProc* gFreeProc = memoryBlockFreeImpl;

// 0x51DEDC
int gMemoryBlocksCurrentCount = 0;

// 0x51DEE0
int gMemoryBlockMaximumCount = 0;

// 0x51DEE4
size_t gMemoryBlocksCurrentSize = 0;

// 0x51DEE8
size_t gMemoryBlocksMaximumSize = 0;

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
void* memoryBlockMallocImpl(size_t size)
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
void* memoryBlockReallocImpl(void* ptr, size_t size)
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
void memoryBlockFreeImpl(void* ptr)
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
void memoryBlockPrintStats()
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
void memoryBlockValidate(void* block)
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
