#include "memory.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "debug.h"

namespace fallout {

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
static void* mem_prep_block(void* block, size_t size);
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
    char* copy = nullptr;
    if (string != nullptr) {
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
    void* ptr = nullptr;

    if (size != 0) {
        size += sizeof(MemoryBlockHeader) + sizeof(MemoryBlockFooter);
        size += sizeof(int) - size % sizeof(int);

        unsigned char* block = (unsigned char*)malloc(size);
        if (block != nullptr) {
            // NOTE: Uninline.
            ptr = mem_prep_block(block, size);

            gMemoryBlocksCurrentCount++;
            if (gMemoryBlocksCurrentCount > gMemoryBlockMaximumCount) {
                gMemoryBlockMaximumCount = gMemoryBlocksCurrentCount;
            }

            gMemoryBlocksCurrentSize += size;
            if (gMemoryBlocksCurrentSize > gMemoryBlocksMaximumSize) {
                gMemoryBlocksMaximumSize = gMemoryBlocksCurrentSize;
            }
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
    if (ptr != nullptr) {
        unsigned char* block = (unsigned char*)ptr - sizeof(MemoryBlockHeader);

        MemoryBlockHeader* header = (MemoryBlockHeader*)block;
        size_t oldSize = header->size;

        gMemoryBlocksCurrentSize -= oldSize;

        memoryBlockValidate(block);

        if (size != 0) {
            size += sizeof(MemoryBlockHeader) + sizeof(MemoryBlockFooter);
            size += sizeof(int) - size % sizeof(int);
        }

        unsigned char* newBlock = (unsigned char*)realloc(block, size);
        if (newBlock != nullptr) {
            gMemoryBlocksCurrentSize += size;
            if (gMemoryBlocksCurrentSize > gMemoryBlocksMaximumSize) {
                gMemoryBlocksMaximumSize = gMemoryBlocksCurrentSize;
            }

            // NOTE: Uninline.
            ptr = mem_prep_block(newBlock, size);
        } else {
            if (size != 0) {
                gMemoryBlocksCurrentSize += oldSize;

                debugPrint("%s,%u: ", __FILE__, __LINE__); // "Memory.c", 195
                debugPrint("Realloc failure.\n");
            } else {
                gMemoryBlocksCurrentCount--;
            }
            ptr = nullptr;
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
    if (ptr != nullptr) {
        void* block = (unsigned char*)ptr - sizeof(MemoryBlockHeader);
        MemoryBlockHeader* header = (MemoryBlockHeader*)block;

        memoryBlockValidate(block);

        gMemoryBlocksCurrentSize -= header->size;
        gMemoryBlocksCurrentCount--;

        free(block);
    }
}

// 0x4C5C5C
void mem_check()
{
    if (gMallocProc == memoryBlockMallocImpl) {
        debugPrint("Current memory allocated: %6d blocks, %9u bytes total\n", gMemoryBlocksCurrentCount, gMemoryBlocksCurrentSize);
        debugPrint("Max memory allocated:     %6d blocks, %9u bytes total\n", gMemoryBlockMaximumCount, gMemoryBlocksMaximumSize);
    }
}

// NOTE: Inlined.
//
// 0x4C5CC4
static void* mem_prep_block(void* block, size_t size)
{
    MemoryBlockHeader* header;
    MemoryBlockFooter* footer;

    header = (MemoryBlockHeader*)block;
    header->guard = MEMORY_BLOCK_HEADER_GUARD;
    header->size = size;

    footer = (MemoryBlockFooter*)((unsigned char*)block + size - sizeof(*footer));
    footer->guard = MEMORY_BLOCK_FOOTER_GUARD;

    return (unsigned char*)block + sizeof(*header);
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

} // namespace fallout
