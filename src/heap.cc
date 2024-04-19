#include "heap.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "debug.h"
#include "memory.h"

namespace fallout {

#define HEAP_BLOCK_HEADER_GUARD (0xDEADC0DE)
#define HEAP_BLOCK_FOOTER_GUARD (0xACDCACDC)

#define HEAP_BLOCK_HEADER_SIZE (sizeof(HeapBlockHeader))
#define HEAP_BLOCK_FOOTER_SIZE (sizeof(HeapBlockFooter))
#define HEAP_BLOCK_OVERHEAD_SIZE (HEAP_BLOCK_HEADER_SIZE + HEAP_BLOCK_FOOTER_SIZE)

// The initial length of [handles] array within [Heap].
#define HEAP_HANDLES_INITIAL_LENGTH (64)

// The initial length of [gHeapFreeBlocks] array.
#define HEAP_FREE_BLOCKS_INITIAL_LENGTH (128)

// The initial length of [gHeapMoveableExtents] array.
#define HEAP_MOVEABLE_EXTENTS_INITIAL_LENGTH (64)

// The initial length of [gHeapMoveableBlocks] array.
#define HEAP_MOVEABLE_BLOCKS_INITIAL_LENGTH (64)

// The initial length of [gHeapReservedFreeBlockIndexes] array.
#define HEAP_RESERVED_FREE_BLOCK_INDEXES_INITIAL_LENGTH (64)

// The minimum size of block for splitting.
#define HEAP_BLOCK_MIN_SIZE (128 + HEAP_BLOCK_OVERHEAD_SIZE)

#define HEAP_HANDLE_STATE_INVALID (-1)

// The only allowed combination is LOCKED | SYSTEM.
typedef enum HeapBlockState {
    HEAP_BLOCK_STATE_FREE = 0x00,
    HEAP_BLOCK_STATE_MOVABLE = 0x01,
    HEAP_BLOCK_STATE_LOCKED = 0x02,
    HEAP_BLOCK_STATE_SYSTEM = 0x04,
} HeapBlockState;

typedef struct HeapBlockHeader {
    int guard;
    int size;
    unsigned int state;
    int handle_index;
} HeapBlockHeader;

typedef struct HeapBlockFooter {
    int guard;
} HeapBlockFooter;

typedef struct HeapMoveableExtent {
    // Pointer to the first block in the extent.
    unsigned char* data;

    // Total number of free or moveable blocks in the extent.
    int blocksLength;

    // Number of moveable blocks in the extent.
    int moveableBlocksLength;

    // Total data size of blocks in the extent. This value does not include
    // the size of blocks overhead.
    int size;
} HeapMoveableExtent;

static bool heapInternalsInit();
static void heapInternalsFree();
static bool heapHandleListInit(Heap* heap);
static bool heapPrintStats(Heap* heap, char* dest, size_t size);
static bool heapFindFreeHandle(Heap* heap, int* handleIndexPtr);
static bool heapFindFreeBlock(Heap* heap, int size, void** blockPtr, int a4);
static int heapBlockCompareBySize(const void* a1, const void* a2);
static int heapMoveableExtentsCompareBySize(const void* a1, const void* a2);
static bool heapBuildMoveableExtentsList(Heap* heap, int* moveableExtentsLengthPtr, int* maxBlocksLengthPtr);
static bool heapBuildFreeBlocksList(Heap* heap);
static bool heapBuildMoveableBlocksList(int extentIndex);

// An array of pointers to free heap blocks.
//
// 0x518E9C
static unsigned char** gHeapFreeBlocks = nullptr;

// An array of moveable extents in heap.
//
// 0x518EA0
static HeapMoveableExtent* gHeapMoveableExtents = nullptr;

// An array of pointers to moveable heap blocks.
//
// 0x518EA4
static unsigned char** gHeapMoveableBlocks = nullptr;

// An array of indexes into [gHeapFreeBlocks] array to track which free blocks
// were already reserved for subsequent moving.
//
// 0x518EA8
static int* gHeapReservedFreeBlockIndexes = nullptr;

// The length of the [gHeapFreeBlocks] array.
//
// 0x518EAC
static int gHeapFreeBlocksLength = 0;

// The length of [gHeapMoveableExtents] array.
//
// 0x518EB0
static int gHeapMoveableExtentsLength = 0;

// The length of [gHeapMoveableBlocks] array.
//
// 0x518EB4
static int gHeapMoveableBlocksLength = 0;

// The length of [gHeapReservedFreeBlockIndexes] array.
//
// 0x518EB8
static int gHeapReservedFreeBlockIndexesLength = 0;

// The number of heaps.
//
// This value is used to init/free internal temporary buffers
// needed for any heap.
//
// 0x518EBC
static int gHeapsCount = 0;

// 0x453304
static bool heapInternalsInit()
{
    // NOTE: Original code is slightly different. It uses deep nesting or a
    // bunch of goto's to free alloc'ed buffers one by one starting from where
    // it has failed.
    do {
        gHeapFreeBlocks = (unsigned char**)internal_malloc(sizeof(*gHeapFreeBlocks) * HEAP_FREE_BLOCKS_INITIAL_LENGTH);
        if (gHeapFreeBlocks == nullptr) {
            break;
        }

        gHeapFreeBlocksLength = HEAP_FREE_BLOCKS_INITIAL_LENGTH;

        gHeapMoveableExtents = (HeapMoveableExtent*)internal_malloc(sizeof(*gHeapMoveableExtents) * HEAP_MOVEABLE_EXTENTS_INITIAL_LENGTH);
        if (gHeapMoveableExtents == nullptr) {
            break;
        }
        gHeapMoveableExtentsLength = HEAP_MOVEABLE_EXTENTS_INITIAL_LENGTH;

        gHeapMoveableBlocks = (unsigned char**)internal_malloc(sizeof(*gHeapMoveableBlocks) * HEAP_MOVEABLE_BLOCKS_INITIAL_LENGTH);
        if (gHeapMoveableBlocks == nullptr) {
            break;
        }
        gHeapMoveableBlocksLength = HEAP_MOVEABLE_BLOCKS_INITIAL_LENGTH;

        gHeapReservedFreeBlockIndexes = (int*)internal_malloc(sizeof(*gHeapReservedFreeBlockIndexes) * HEAP_RESERVED_FREE_BLOCK_INDEXES_INITIAL_LENGTH);
        if (gHeapReservedFreeBlockIndexes == nullptr) {
            break;
        }
        gHeapReservedFreeBlockIndexesLength = HEAP_RESERVED_FREE_BLOCK_INDEXES_INITIAL_LENGTH;

        return true;
    } while (0);

    // NOTE: Original code frees them one by one without calling this function.
    heapInternalsFree();

    return false;
}

// 0x4533A0
static void heapInternalsFree()
{
    if (gHeapReservedFreeBlockIndexes != nullptr) {
        internal_free(gHeapReservedFreeBlockIndexes);
        gHeapReservedFreeBlockIndexes = nullptr;
    }
    gHeapReservedFreeBlockIndexesLength = 0;

    if (gHeapMoveableBlocks != nullptr) {
        internal_free(gHeapMoveableBlocks);
        gHeapMoveableBlocks = nullptr;
    }
    gHeapMoveableBlocksLength = 0;

    if (gHeapMoveableExtents != nullptr) {
        internal_free(gHeapMoveableExtents);
        gHeapMoveableExtents = nullptr;
    }
    gHeapMoveableExtentsLength = 0;

    if (gHeapFreeBlocks != nullptr) {
        internal_free(gHeapFreeBlocks);
        gHeapFreeBlocks = nullptr;
    }
    gHeapFreeBlocksLength = 0;
}

// 0x452974
bool heapInit(Heap* heap, int a2)
{
    if (heap == nullptr) {
        return false;
    }

    if (gHeapsCount == 0) {
        if (!heapInternalsInit()) {
            return false;
        }
    }

    memset(heap, 0, sizeof(*heap));

    if (heapHandleListInit(heap)) {
        int size = (a2 >> 10) + a2;
        heap->data = (unsigned char*)internal_malloc(size);
        if (heap->data != nullptr) {
            heap->size = size;
            heap->freeBlocks = 1;
            heap->freeSize = heap->size - HEAP_BLOCK_OVERHEAD_SIZE;

            HeapBlockHeader* blockHeader = (HeapBlockHeader*)heap->data;
            blockHeader->guard = HEAP_BLOCK_HEADER_GUARD;
            blockHeader->size = heap->freeSize;
            blockHeader->state = 0;
            blockHeader->handle_index = -1;

            HeapBlockFooter* blockFooter = (HeapBlockFooter*)(heap->data + blockHeader->size + HEAP_BLOCK_HEADER_SIZE);
            blockFooter->guard = HEAP_BLOCK_FOOTER_GUARD;

            gHeapsCount++;

            return true;
        }
    }

    if (gHeapsCount == 0) {
        heapInternalsFree();
    }

    return false;
}

// 0x452A3C
bool heapFree(Heap* heap)
{
    if (heap == nullptr) {
        return false;
    }

    for (int index = 0; index < heap->handlesLength; index++) {
        HeapHandle* handle = &(heap->handles[index]);
        if (handle->state == 4 && handle->data != nullptr) {
            internal_free(handle->data);
        }
    }

    if (heap->handles != nullptr) {
        internal_free(heap->handles);
        heap->handles = nullptr;
        heap->handlesLength = 0;
    }

    if (heap->data != nullptr) {
        internal_free(heap->data);
    }

    memset(heap, 0, sizeof(*heap));

    gHeapsCount--;
    if (gHeapsCount == 0) {
        heapInternalsFree();
    }

    return true;
}

// 0x453430
static bool heapHandleListInit(Heap* heap)
{
    heap->handles = (HeapHandle*)internal_malloc(sizeof(*heap->handles) * HEAP_HANDLES_INITIAL_LENGTH);
    if (heap->handles == nullptr) {
        debugPrint("Heap Error : Could not initialize handles.\n");
        return false;
    }

    for (int index = 0; index < HEAP_HANDLES_INITIAL_LENGTH; index++) {
        HeapHandle* handle = &(heap->handles[index]);
        handle->state = HEAP_HANDLE_STATE_INVALID;
        handle->data = nullptr;
    }

    heap->handlesLength = HEAP_HANDLES_INITIAL_LENGTH;

    return true;
}

// 0x452AD0
bool heapBlockAllocate(Heap* heap, int* handleIndexPtr, int size, int a4)
{
    HeapBlockHeader* blockHeader;
    int state;
    int blockSize;
    HeapHandle* handle;

    size += 4 - size % 4;

    if (heap == nullptr || handleIndexPtr == nullptr || size == 0) {
        goto err;
    }

    if (a4 != 0 && a4 != 1) {
        a4 = 0;
    }

    void* block;
    if (!heapFindFreeBlock(heap, size, &block, a4)) {
        goto err;
    }

    blockHeader = (HeapBlockHeader*)block;
    state = blockHeader->state;

    int handleIndex;
    if (!heapFindFreeHandle(heap, &handleIndex)) {
        goto err_no_handle;
    }

    blockSize = blockHeader->size;
    handle = &(heap->handles[handleIndex]);

    if (state == HEAP_BLOCK_STATE_SYSTEM) {
        // Bind block to handle.
        blockHeader->handle_index = handleIndex;

        // Bind handle to block and mark it as system
        handle->state = HEAP_BLOCK_STATE_SYSTEM;
        handle->data = (unsigned char*)block;

        // Update heap stats
        heap->systemBlocks++;
        heap->systemSize += size;

        *handleIndexPtr = handleIndex;

        return true;
    }

    if (state == HEAP_BLOCK_STATE_FREE) {
        int remainingSize = blockSize - size;
        if (remainingSize > HEAP_BLOCK_MIN_SIZE) {
            // The block we've just found is big enough for splitting, first
            // resize it to take just what was requested.
            blockHeader->size = size;
            blockSize = size;

            //
            HeapBlockFooter* blockFooter = (HeapBlockFooter*)((unsigned char*)block + blockHeader->size + HEAP_BLOCK_HEADER_SIZE);
            blockFooter->guard = HEAP_BLOCK_FOOTER_GUARD;

            // Obtain beginning of the next block.
            unsigned char* nextBlock = (unsigned char*)block + blockHeader->size + HEAP_BLOCK_OVERHEAD_SIZE;

            // Setup next block's header...
            HeapBlockHeader* nextBlockHeader = (HeapBlockHeader*)nextBlock;
            nextBlockHeader->guard = HEAP_BLOCK_HEADER_GUARD;
            nextBlockHeader->size = remainingSize - HEAP_BLOCK_OVERHEAD_SIZE;
            nextBlockHeader->state = HEAP_BLOCK_STATE_FREE;
            nextBlockHeader->handle_index = -1;

            // ... and footer.
            HeapBlockFooter* nextBlockFooter = (HeapBlockFooter*)(nextBlock + nextBlockHeader->size + HEAP_BLOCK_HEADER_SIZE);
            nextBlockFooter->guard = HEAP_BLOCK_FOOTER_GUARD;

            // Update heap stats
            heap->freeBlocks++;
            heap->freeSize -= HEAP_BLOCK_OVERHEAD_SIZE;
        }

        // Bind block to handle and mark it as moveable
        blockHeader->state = HEAP_BLOCK_STATE_MOVABLE;
        blockHeader->handle_index = handleIndex;

        // Bind handle to block and mark it as moveable
        handle->state = HEAP_BLOCK_STATE_MOVABLE;
        handle->data = (unsigned char*)block;

        // Update heap stats
        heap->freeBlocks--;
        heap->moveableBlocks++;
        heap->freeSize -= blockSize;
        heap->moveableSize += blockSize;

        *handleIndexPtr = handleIndex;

        return true;
    }

    handle->state = HEAP_HANDLE_STATE_INVALID;
    handle->data = nullptr;

    debugPrint("Heap Error: Unknown block state during allocation.\n");

err_no_handle:

    debugPrint("Heap Error: Could not acquire handle for new block.\n");
    if (state == HEAP_BLOCK_STATE_SYSTEM) {
        internal_free(block);
    }

err:

    debugPrint("Heap Warning: Could not allocate block of %d bytes.\n", size);
    return false;
}

// heap_block_free
// 0x452CB4
bool heapBlockDeallocate(Heap* heap, int* handleIndexPtr)
{
    if (heap == nullptr || handleIndexPtr == nullptr) {
        debugPrint("Heap Error: Could not deallocate block.\n");
        return false;
    }

    int handleIndex = *handleIndexPtr;

    HeapHandle* handle = &(heap->handles[handleIndex]);

    HeapBlockHeader* blockHeader = (HeapBlockHeader*)handle->data;
    if (blockHeader->guard != HEAP_BLOCK_HEADER_GUARD) {
        debugPrint("Heap Error: Bad guard begin detected during deallocate.\n");
    }

    HeapBlockFooter* blockFooter = (HeapBlockFooter*)(handle->data + blockHeader->size + HEAP_BLOCK_HEADER_SIZE);
    if (blockFooter->guard != HEAP_BLOCK_FOOTER_GUARD) {
        debugPrint("Heap Error: Bad guard end detected during deallocate.\n");
    }

    if (handle->state != blockHeader->state) {
        debugPrint("Heap Error: Mismatched block states detected during deallocate.\n");
    }

    if ((handle->state & HEAP_BLOCK_STATE_LOCKED) != 0) {
        debugPrint("Heap Error: Attempt to deallocate locked block.\n");
        return false;
    }

    int size = blockHeader->size;

    if (handle->state == HEAP_BLOCK_STATE_MOVABLE) {
        // Unbind block from handle and mark it as free.
        blockHeader->handle_index = -1;
        blockHeader->state = HEAP_BLOCK_STATE_FREE;

        // Update heap stats
        heap->freeBlocks++;
        heap->moveableBlocks--;

        heap->freeSize += size;
        heap->moveableSize -= size;

        // Reset handle
        handle->state = HEAP_HANDLE_STATE_INVALID;
        handle->data = nullptr;

        return true;
    }

    if (handle->state == HEAP_BLOCK_STATE_SYSTEM) {
        // Release system memory
        internal_free(handle->data);

        // Update heap stats
        heap->systemBlocks--;
        heap->systemSize -= size;

        // Reset handle
        handle->state = HEAP_HANDLE_STATE_INVALID;
        handle->data = nullptr;

        return true;
    }

    debugPrint("Heap Error: Unknown block state during deallocation.\n");
    return false;
}

// 0x452DE0
bool heapLock(Heap* heap, int handleIndex, unsigned char** bufferPtr)
{
    if (heap == nullptr) {
        debugPrint("Heap Error: Could not lock block");
        return false;
    }

    HeapHandle* handle = &(heap->handles[handleIndex]);

    HeapBlockHeader* blockHeader = (HeapBlockHeader*)handle->data;
    if (blockHeader->guard != HEAP_BLOCK_HEADER_GUARD) {
        debugPrint("Heap Error: Bad guard begin detected during lock.\n");
        return false;
    }

    HeapBlockFooter* blockFooter = (HeapBlockFooter*)(handle->data + blockHeader->size + HEAP_BLOCK_HEADER_SIZE);
    if (blockFooter->guard != HEAP_BLOCK_FOOTER_GUARD) {
        debugPrint("Heap Error: Bad guard end detected during lock.\n");
        return false;
    }

    if (handle->state != blockHeader->state) {
        debugPrint("Heap Error: Mismatched block states detected during lock.\n");
        return false;
    }

    if ((handle->state & HEAP_BLOCK_STATE_LOCKED) != 0) {
        debugPrint("Heap Error: Attempt to lock a previously locked block.");
        return false;
    }

    if (handle->state == HEAP_BLOCK_STATE_MOVABLE) {
        blockHeader->state = HEAP_BLOCK_STATE_LOCKED;
        handle->state = HEAP_BLOCK_STATE_LOCKED;

        heap->moveableBlocks--;
        heap->lockedBlocks++;

        int size = blockHeader->size;
        heap->moveableSize -= size;
        heap->lockedSize += size;

        *bufferPtr = handle->data + HEAP_BLOCK_HEADER_SIZE;

        return true;
    }

    if (handle->state == HEAP_BLOCK_STATE_SYSTEM) {
        blockHeader->state |= HEAP_BLOCK_STATE_LOCKED;
        handle->state |= HEAP_BLOCK_STATE_LOCKED;

        *bufferPtr = handle->data + HEAP_BLOCK_HEADER_SIZE;

        return true;
    }

    debugPrint("Heap Error: Unknown block state during lock.\n");
    return false;
}

// 0x452EE4
bool heapUnlock(Heap* heap, int handleIndex)
{
    if (heap == nullptr) {
        debugPrint("Heap Error: Could not unlock block.\n");
        return false;
    }

    HeapHandle* handle = &(heap->handles[handleIndex]);

    HeapBlockHeader* blockHeader = (HeapBlockHeader*)handle->data;
    if (blockHeader->guard != HEAP_BLOCK_HEADER_GUARD) {
        debugPrint("Heap Error: Bad guard begin detected during unlock.\n");
    }

    HeapBlockFooter* blockFooter = (HeapBlockFooter*)(handle->data + blockHeader->size + HEAP_BLOCK_HEADER_SIZE);
    if (blockFooter->guard != HEAP_BLOCK_FOOTER_GUARD) {
        debugPrint("Heap Error: Bad guard end detected during unlock.\n");
    }

    if (handle->state != blockHeader->state) {
        debugPrint("Heap Error: Mismatched block states detected during unlock.\n");
    }

    if ((handle->state & HEAP_BLOCK_STATE_LOCKED) == 0) {
        debugPrint("Heap Error: Attempt to unlock a previously unlocked block.\n");
        debugPrint("Heap Error: Could not unlock block.\n");
        return false;
    }

    if ((handle->state & HEAP_BLOCK_STATE_SYSTEM) != 0) {
        blockHeader->state = HEAP_BLOCK_STATE_SYSTEM;
        handle->state = HEAP_BLOCK_STATE_SYSTEM;
        return true;
    }

    blockHeader->state = HEAP_BLOCK_STATE_MOVABLE;
    handle->state = HEAP_BLOCK_STATE_MOVABLE;

    heap->moveableBlocks++;
    heap->lockedBlocks--;

    int size = blockHeader->size;
    heap->moveableSize += size;
    heap->lockedSize -= size;

    return true;
}

// 0x4532AC
static bool heapPrintStats(Heap* heap, char* dest, size_t size)
{
    if (heap == nullptr || dest == nullptr) {
        return false;
    }

    const char* format = "[Heap]\n"
                         "Total free blocks: %d\n"
                         "Total free size: %d\n"
                         "Total moveable blocks: %d\n"
                         "Total moveable size: %d\n"
                         "Total locked blocks: %d\n"
                         "Total locked size: %d\n"
                         "Total system blocks: %d\n"
                         "Total system size: %d\n"
                         "Total handles: %d\n"
                         "Total heaps: %d";

    snprintf(dest, size, format,
        heap->freeBlocks,
        heap->freeSize,
        heap->moveableBlocks,
        heap->moveableSize,
        heap->lockedBlocks,
        heap->lockedSize,
        heap->systemBlocks,
        heap->systemSize,
        heap->handlesLength,
        gHeapsCount);

    return true;
}

// 0x4534B0
static bool heapFindFreeHandle(Heap* heap, int* handleIndexPtr)
{
    // Loop thru already available handles and find first that is not currently
    // used.
    for (int index = 0; index < heap->handlesLength; index++) {
        HeapHandle* handle = &(heap->handles[index]);
        if (handle->state == HEAP_HANDLE_STATE_INVALID) {
            *handleIndexPtr = index;
            return true;
        }
    }

    // If we're here the search above failed, we have to allocate more handles.
    HeapHandle* handles = (HeapHandle*)internal_realloc(heap->handles, sizeof(*handles) * (heap->handlesLength + HEAP_HANDLES_INITIAL_LENGTH));
    if (handles == nullptr) {
        return false;
    }

    heap->handles = handles;

    // Loop thru new handles and reset them to default state.
    for (int index = heap->handlesLength; index < heap->handlesLength + HEAP_HANDLES_INITIAL_LENGTH; index++) {
        HeapHandle* handle = &(heap->handles[index]);
        handle->state = HEAP_HANDLE_STATE_INVALID;
        handle->data = nullptr;
    }

    *handleIndexPtr = heap->handlesLength;

    heap->handlesLength += HEAP_HANDLES_INITIAL_LENGTH;

    return true;
}

// heap_find_free_block
// 0x453588
static bool heapFindFreeBlock(Heap* heap, int size, void** blockPtr, int a4)
{
    unsigned char* biggestFreeBlock;
    HeapBlockHeader* biggestFreeBlockHeader;
    int biggestFreeBlockSize;
    HeapMoveableExtent* extent;
    int reservedFreeBlockIndex;
    HeapBlockHeader* blockHeader;
    HeapBlockFooter* blockFooter;

    if (!heapBuildFreeBlocksList(heap)) {
        goto system;
    }

    if (size > heap->freeSize) {
        goto system;
    }

    if (heap->freeBlocks > 1) {
        qsort(gHeapFreeBlocks, heap->freeBlocks, sizeof(*gHeapFreeBlocks), heapBlockCompareBySize);
    }

    // Take last free block (the biggest one).
    biggestFreeBlock = gHeapFreeBlocks[heap->freeBlocks - 1];
    biggestFreeBlockHeader = (HeapBlockHeader*)biggestFreeBlock;
    biggestFreeBlockSize = biggestFreeBlockHeader->size;

    // Make sure it can encompass new block of given size.
    if (biggestFreeBlockSize >= size) {
        // Now loop thru all free blocks and find the first one that's at least
        // as large as what was required.
        int index;
        for (index = 0; index < heap->freeBlocks; index++) {
            unsigned char* block = gHeapFreeBlocks[index];
            HeapBlockHeader* blockHeader = (HeapBlockHeader*)block;
            if (blockHeader->size >= size) {
                break;
            }
        }

        *blockPtr = gHeapFreeBlocks[index];
        return true;
    }

    int moveableExtentsCount;
    int maxBlocksCount;
    if (!heapBuildMoveableExtentsList(heap, &moveableExtentsCount, &maxBlocksCount)) {
        goto system;
    }

    // Ensure the length of [gHeapReservedFreeBlockIndexes] array is big enough
    // to index all blocks for longest moveable extent.
    if (maxBlocksCount > gHeapReservedFreeBlockIndexesLength) {
        int* indexes = (int*)internal_realloc(gHeapReservedFreeBlockIndexes, sizeof(*gHeapReservedFreeBlockIndexes) * maxBlocksCount);
        if (indexes == nullptr) {
            goto system;
        }

        gHeapReservedFreeBlockIndexesLength = maxBlocksCount;
        gHeapReservedFreeBlockIndexes = indexes;
    }

    qsort(gHeapMoveableExtents, moveableExtentsCount, sizeof(*gHeapMoveableExtents), heapMoveableExtentsCompareBySize);

    if (moveableExtentsCount == 0) {
        goto system;
    }

    // Loop thru moveable extents and find first one which is big enough for new
    // block and for which we can move every block somewhere.
    int extentIndex;
    for (extentIndex = 0; extentIndex < moveableExtentsCount; extentIndex++) {
        HeapMoveableExtent* extent = &(gHeapMoveableExtents[extentIndex]);

        // Calculate extent size including the size of the overhead. Exclude the
        // size of one overhead for current block.
        int extentSize = extent->size + HEAP_BLOCK_OVERHEAD_SIZE * extent->blocksLength - HEAP_BLOCK_OVERHEAD_SIZE;

        // Make sure current extent is worth moving which means there will be
        // enough size for new block of given size after moving current extent.
        if (extentSize < size) {
            continue;
        }

        if (!heapBuildMoveableBlocksList(extentIndex)) {
            continue;
        }

        // Sort moveable blocks by size (smallest -> largest)
        qsort(gHeapMoveableBlocks, extent->moveableBlocksLength, sizeof(*gHeapMoveableBlocks), heapBlockCompareBySize);

        int reservedBlocksLength = 0;

        // Loop thru sorted moveable blocks and build array of reservations.
        for (int moveableBlockIndex = 0; moveableBlockIndex < extent->moveableBlocksLength; moveableBlockIndex++) {
            // Grab current moveable block.
            unsigned char* moveableBlock = gHeapMoveableBlocks[moveableBlockIndex];
            HeapBlockHeader* moveableBlockHeader = (HeapBlockHeader*)moveableBlock;

            // Make sure there is at least one free block that's big enough
            // to encompass it.
            if (biggestFreeBlockSize < moveableBlockHeader->size) {
                continue;
            }

            // Loop thru sorted free blocks (smallest -> largest) and find
            // first unreserved free block that can encompass current moveable
            // block.
            int freeBlockIndex;
            for (freeBlockIndex = 0; freeBlockIndex < heap->freeBlocks; freeBlockIndex++) {
                // Grab current free block.
                unsigned char* freeBlock = gHeapFreeBlocks[freeBlockIndex];
                HeapBlockHeader* freeBlockHeader = (HeapBlockHeader*)freeBlock;

                // Make sure it's size is enough for current moveable block.
                if (freeBlockHeader->size < moveableBlockHeader->size) {
                    continue;
                }

                // Make sure it's outside of the current extent, because free
                // blocks inside it is already taken into account in
                // `extentSize`.
                if (freeBlock >= extent->data && freeBlock < extent->data + extentSize + HEAP_BLOCK_OVERHEAD_SIZE) {
                    continue;
                }

                // Loop thru reserved free blocks to make to make sure we
                // can take it.
                int freeBlocksIndexesIndex;
                for (freeBlocksIndexesIndex = 0; freeBlocksIndexesIndex < reservedBlocksLength; freeBlocksIndexesIndex++) {
                    if (freeBlockIndex == gHeapReservedFreeBlockIndexes[freeBlocksIndexesIndex]) {
                        // This free block was already reserved, there is no
                        // need to continue.
                        break;
                    }
                }

                if (freeBlocksIndexesIndex == reservedBlocksLength) {
                    // We've looked thru entire reserved free blocks array
                    // and haven't found resevation. That means we can
                    // reseve current free block, so stop further search.
                    break;
                }
            }

            if (freeBlockIndex == heap->freeBlocks) {
                // We've looked thru entire free blocks array and haven't
                // found suitable free block for current moveable block.
                // Skip the rest of the search, since we want to move the
                // entire extent and just found out that at least one block
                // cannot be moved.
                break;
            }

            // If we get this far, we've found suitable free block for
            // current moveable block, save it for later usage.
            gHeapReservedFreeBlockIndexes[reservedBlocksLength++] = freeBlockIndex;
        }

        if (reservedBlocksLength == extent->moveableBlocksLength) {
            // We've reserved free block for every movable block in current
            // extent.
            break;
        }
    }

    if (extentIndex == moveableExtentsCount) {
        // We've looked thru entire moveable extents and haven't found one
        // suitable for moving.
        goto system;
    }

    extent = &(gHeapMoveableExtents[extentIndex]);
    reservedFreeBlockIndex = 0;
    for (int moveableBlockIndex = 0; moveableBlockIndex < extent->moveableBlocksLength; moveableBlockIndex++) {
        unsigned char* moveableBlock = gHeapMoveableBlocks[moveableBlockIndex];
        HeapBlockHeader* moveableBlockHeader = (HeapBlockHeader*)moveableBlock;
        int moveableBlockSize = moveableBlockHeader->size;
        if (biggestFreeBlockSize < moveableBlockSize) {
            continue;
        }

        unsigned char* freeBlock = gHeapFreeBlocks[gHeapReservedFreeBlockIndexes[reservedFreeBlockIndex++]];
        HeapBlockHeader* freeBlockHeader = (HeapBlockHeader*)freeBlock;
        int freeBlockSize = freeBlockHeader->size;

        memcpy(freeBlock, moveableBlock, moveableBlockSize + HEAP_BLOCK_OVERHEAD_SIZE);
        heap->handles[freeBlockHeader->handle_index].data = freeBlock;

        // Calculate remaining size of the free block after moving.
        int remainingSize = freeBlockSize - moveableBlockSize;
        if (remainingSize != 0) {
            if (remainingSize < HEAP_BLOCK_MIN_SIZE) {
                // The remaining size of the former free block is too small to
                // become a new free block, merge it into the current one.
                freeBlockHeader->size += remainingSize;
                HeapBlockFooter* freeBlockFooter = (HeapBlockFooter*)(freeBlock + freeBlockHeader->size + HEAP_BLOCK_HEADER_SIZE);
                freeBlockFooter->guard = HEAP_BLOCK_FOOTER_GUARD;

                // The remaining size of the free block was merged into moveable
                // block, update heap stats accordingly.
                heap->freeSize -= remainingSize;
                heap->moveableSize += remainingSize;
            } else {
                // The remaining size is enough for a new block. The current
                // block is already properly formatted - it's header and
                // footer was copied from moveable block. Since this was a valid
                // free block it also has it's footer already in place. So the
                // only thing left is header.
                unsigned char* nextFreeBlock = freeBlock + freeBlockHeader->size + HEAP_BLOCK_OVERHEAD_SIZE;
                HeapBlockHeader* nextFreeBlockHeader = (HeapBlockHeader*)nextFreeBlock;
                nextFreeBlockHeader->state = HEAP_BLOCK_STATE_FREE;
                nextFreeBlockHeader->handle_index = -1;
                nextFreeBlockHeader->size = remainingSize - HEAP_BLOCK_OVERHEAD_SIZE;
                nextFreeBlockHeader->guard = HEAP_BLOCK_HEADER_GUARD;

                heap->freeBlocks++;
                heap->freeSize -= HEAP_BLOCK_OVERHEAD_SIZE;
            }
        }
    }

    heap->freeBlocks -= extent->blocksLength - 1;
    heap->freeSize += (extent->blocksLength - 1) * HEAP_BLOCK_OVERHEAD_SIZE;

    // Create one free block from entire moveable extent.
    blockHeader = (HeapBlockHeader*)extent->data;
    blockHeader->guard = HEAP_BLOCK_HEADER_GUARD;
    blockHeader->size = extent->size + (extent->blocksLength - 1) * HEAP_BLOCK_OVERHEAD_SIZE;
    blockHeader->state = HEAP_BLOCK_STATE_FREE;
    blockHeader->handle_index = -1;

    blockFooter = (HeapBlockFooter*)(extent->data + blockHeader->size + HEAP_BLOCK_HEADER_SIZE);
    blockFooter->guard = HEAP_BLOCK_FOOTER_GUARD;

    *blockPtr = extent->data;

    return true;

system:

    if (1) {
        char stats[512];
        if (heapPrintStats(heap, stats, sizeof(stats))) {
            debugPrint("\n%s\n", stats);
        }

        if (a4 == 0) {
            debugPrint("Allocating block from system memory...\n");
            unsigned char* block = (unsigned char*)internal_malloc(size + HEAP_BLOCK_OVERHEAD_SIZE);
            if (block == nullptr) {
                debugPrint("fatal error: internal_malloc() failed in heap_find_free_block()!\n");
                return false;
            }

            HeapBlockHeader* blockHeader = (HeapBlockHeader*)block;
            blockHeader->guard = HEAP_BLOCK_HEADER_GUARD;
            blockHeader->size = size;
            blockHeader->state = HEAP_BLOCK_STATE_SYSTEM;
            blockHeader->handle_index = -1;

            HeapBlockFooter* blockFooter = (HeapBlockFooter*)(block + blockHeader->size + HEAP_BLOCK_HEADER_SIZE);
            blockFooter->guard = HEAP_BLOCK_FOOTER_GUARD;

            *blockPtr = block;

            return true;
        }
    }

    return false;
}

// Build list of pointers to moveable blocks in given extent.
//
// 0x453E80
static bool heapBuildMoveableBlocksList(int extentIndex)
{
    HeapMoveableExtent* extent = &(gHeapMoveableExtents[extentIndex]);
    if (extent->moveableBlocksLength > gHeapMoveableBlocksLength) {
        unsigned char** moveableBlocks = (unsigned char**)internal_realloc(gHeapMoveableBlocks, sizeof(*gHeapMoveableBlocks) * extent->moveableBlocksLength);
        if (moveableBlocks == nullptr) {
            return false;
        }

        gHeapMoveableBlocks = moveableBlocks;
        gHeapMoveableBlocksLength = extent->moveableBlocksLength;
    }

    unsigned char* ptr = extent->data;
    int moveableBlockIndex = 0;
    for (int index = 0; index < extent->blocksLength; index++) {
        HeapBlockHeader* blockHeader = (HeapBlockHeader*)ptr;
        if (blockHeader->state == HEAP_BLOCK_STATE_MOVABLE) {
            gHeapMoveableBlocks[moveableBlockIndex++] = ptr;
        }
        ptr += blockHeader->size + HEAP_BLOCK_OVERHEAD_SIZE;
    }

    return moveableBlockIndex == extent->moveableBlocksLength;
}

// 0x453E74
static int heapMoveableExtentsCompareBySize(const void* a1, const void* a2)
{
    HeapMoveableExtent* v1 = (HeapMoveableExtent*)a1;
    HeapMoveableExtent* v2 = (HeapMoveableExtent*)a2;
    return v1->size - v2->size;
}

// 0x453BC4
static bool heapBuildFreeBlocksList(Heap* heap)
{
    if (heap->freeBlocks == 0) {
        return false;
    }

    if (heap->freeBlocks > gHeapFreeBlocksLength) {
        unsigned char** freeBlocks = (unsigned char**)internal_realloc(gHeapFreeBlocks, sizeof(*freeBlocks) * heap->freeBlocks);
        if (freeBlocks == nullptr) {
            return false;
        }

        gHeapFreeBlocks = (unsigned char**)freeBlocks;
        gHeapFreeBlocksLength = heap->freeBlocks;
    }

    int blocksLength = heap->moveableBlocks + heap->freeBlocks + heap->lockedBlocks;

    unsigned char* ptr = heap->data;

    int freeBlockIndex = 0;
    while (blocksLength != 0) {
        if (freeBlockIndex >= heap->freeBlocks) {
            break;
        }

        HeapBlockHeader* blockHeader = (HeapBlockHeader*)ptr;
        if (blockHeader->state == HEAP_BLOCK_STATE_FREE) {
            // Join consecutive free blocks if any.
            while (blocksLength > 1) {
                // Grab next block and check if's a free block.
                HeapBlockHeader* nextBlockHeader = (HeapBlockHeader*)(ptr + blockHeader->size + HEAP_BLOCK_OVERHEAD_SIZE);
                if (nextBlockHeader->state != HEAP_BLOCK_STATE_FREE) {
                    break;
                }

                // Accumulate it's size plus size of the overhead in the main
                // block.
                blockHeader->size += nextBlockHeader->size + HEAP_BLOCK_OVERHEAD_SIZE;

                // Update heap stats, the free size increased because we've just
                // remove overhead for one block.
                heap->freeBlocks--;
                heap->freeSize += HEAP_BLOCK_OVERHEAD_SIZE;

                blocksLength--;
            }

            gHeapFreeBlocks[freeBlockIndex++] = ptr;
        }

        // Move pointer to the header of the next block.
        ptr += blockHeader->size + HEAP_BLOCK_OVERHEAD_SIZE;

        blocksLength--;
    }

    return true;
}

// 0x453CC4
static int heapBlockCompareBySize(const void* a1, const void* a2)
{
    HeapBlockHeader* header1 = *(HeapBlockHeader**)a1;
    HeapBlockHeader* header2 = *(HeapBlockHeader**)a2;
    return header1->size - header2->size;
}

// 0x453CD0
static bool heapBuildMoveableExtentsList(Heap* heap, int* moveableExtentsLengthPtr, int* maxBlocksLengthPtr)
{
    // Calculate max number of extents. It's only possible when every
    // free or moveable block is followed by locked block.
    int maxExtentsCount = heap->moveableBlocks + heap->freeBlocks;
    if (maxExtentsCount <= 2) {
        debugPrint("<[couldn't build moveable list]>\n");
        return false;
    }

    if (maxExtentsCount > gHeapMoveableExtentsLength) {
        HeapMoveableExtent* moveableExtents = (HeapMoveableExtent*)internal_realloc(gHeapMoveableExtents, sizeof(*gHeapMoveableExtents) * maxExtentsCount);
        if (moveableExtents == nullptr) {
            return false;
        }

        gHeapMoveableExtents = moveableExtents;
        gHeapMoveableExtentsLength = maxExtentsCount;
    }

    unsigned char* ptr = heap->data;
    int blocksLength = heap->moveableBlocks + heap->freeBlocks + heap->lockedBlocks;
    int maxBlocksLength = 0;
    int extentIndex = 0;

    while (blocksLength != 0) {
        if (extentIndex >= maxExtentsCount) {
            break;
        }

        HeapBlockHeader* blockHeader = (HeapBlockHeader*)ptr;
        if (blockHeader->state == HEAP_BLOCK_STATE_FREE || blockHeader->state == HEAP_BLOCK_STATE_MOVABLE) {
            HeapMoveableExtent* extent = &(gHeapMoveableExtents[extentIndex++]);
            extent->data = ptr;
            extent->blocksLength = 1;
            extent->moveableBlocksLength = 0;
            extent->size = blockHeader->size;

            if (blockHeader->state == HEAP_BLOCK_STATE_MOVABLE) {
                extent->moveableBlocksLength = 1;
            }

            // Calculate moveable extent stats from consecutive blocks.
            while (blocksLength > 1) {
                // Grab next block and check if's a free or moveable block.
                HeapBlockHeader* blockHeader = (HeapBlockHeader*)ptr;
                HeapBlockHeader* nextBlockHeader = (HeapBlockHeader*)(ptr + blockHeader->size + HEAP_BLOCK_OVERHEAD_SIZE);
                if (nextBlockHeader->state != HEAP_BLOCK_STATE_FREE && nextBlockHeader->state != HEAP_BLOCK_STATE_MOVABLE) {
                    break;
                }

                // Update extent stats.
                extent->blocksLength++;
                extent->size += nextBlockHeader->size;

                if (nextBlockHeader->state == HEAP_BLOCK_STATE_MOVABLE) {
                    extent->moveableBlocksLength++;
                }

                // Move pointer to the beginning of the next block.
                ptr += (blockHeader->size + HEAP_BLOCK_OVERHEAD_SIZE);

                blocksLength--;
            }

            if (extent->blocksLength > maxBlocksLength) {
                maxBlocksLength = extent->blocksLength;
            }
        }

        // ptr might have been advanced during the loop above.
        blockHeader = (HeapBlockHeader*)ptr;
        ptr += blockHeader->size + HEAP_BLOCK_OVERHEAD_SIZE;

        blocksLength--;
    };

    *moveableExtentsLengthPtr = extentIndex;
    *maxBlocksLengthPtr = maxBlocksLength;

    return true;
}

// 0x452FC4
bool heapValidate(Heap* heap)
{
    debugPrint("Validating heap...\n");

    int blocksCount = heap->freeBlocks + heap->moveableBlocks + heap->lockedBlocks;
    unsigned char* ptr = heap->data;

    int freeBlocks = 0;
    int freeSize = 0;
    int moveableBlocks = 0;
    int moveableSize = 0;
    int lockedBlocks = 0;
    int lockedSize = 0;

    for (int index = 0; index < blocksCount; index++) {
        HeapBlockHeader* blockHeader = (HeapBlockHeader*)ptr;
        if (blockHeader->guard != HEAP_BLOCK_HEADER_GUARD) {
            debugPrint("Bad guard begin detected during validate.\n");
            return false;
        }

        HeapBlockFooter* blockFooter = (HeapBlockFooter*)(ptr + blockHeader->size + HEAP_BLOCK_HEADER_SIZE);
        if (blockFooter->guard != HEAP_BLOCK_FOOTER_GUARD) {
            debugPrint("Bad guard end detected during validate.\n");
            return false;
        }

        if (blockHeader->state == HEAP_BLOCK_STATE_FREE) {
            freeBlocks++;
            freeSize += blockHeader->size;
        } else if (blockHeader->state == HEAP_BLOCK_STATE_MOVABLE) {
            moveableBlocks++;
            moveableSize += blockHeader->size;
        } else if (blockHeader->state == HEAP_BLOCK_STATE_LOCKED) {
            lockedBlocks++;
            lockedSize += blockHeader->size;
        }

        if (index != blocksCount - 1) {
            ptr += blockHeader->size + HEAP_BLOCK_OVERHEAD_SIZE;
            if (ptr > (heap->data + heap->size)) {
                debugPrint("Ran off end of heap during validate!\n");
                return false;
            }
        }
    }

    if (freeBlocks != heap->freeBlocks) {
        debugPrint("Invalid number of free blocks.\n");
        return false;
    }

    if (freeSize != heap->freeSize) {
        debugPrint("Invalid size of free blocks.\n");
        return false;
    }

    if (moveableBlocks != heap->moveableBlocks) {
        debugPrint("Invalid number of moveable blocks.\n");
        return false;
    }

    if (moveableSize != heap->moveableSize) {
        debugPrint("Invalid size of moveable blocks.\n");
        return false;
    }

    if (lockedBlocks != heap->lockedBlocks) {
        debugPrint("Invalid number of locked blocks.\n");
        return false;
    }

    if (lockedSize != heap->lockedSize) {
        debugPrint("Invalid size of locked blocks.\n");
        return false;
    }

    debugPrint("Heap is O.K.\n");

    int systemBlocks = 0;
    int systemSize = 0;

    for (int handleIndex = 0; handleIndex < heap->handlesLength; handleIndex++) {
        HeapHandle* handle = &(heap->handles[handleIndex]);
        if (handle->state != HEAP_HANDLE_STATE_INVALID && (handle->state & HEAP_BLOCK_STATE_SYSTEM) != 0) {
            HeapBlockHeader* blockHeader = (HeapBlockHeader*)handle->data;
            if (blockHeader->guard != HEAP_BLOCK_HEADER_GUARD) {
                debugPrint("Bad guard begin detected in system block during validate.\n");
                return false;
            }

            HeapBlockFooter* blockFooter = (HeapBlockFooter*)(handle->data + blockHeader->size + HEAP_BLOCK_HEADER_SIZE);
            if (blockFooter->guard != HEAP_BLOCK_FOOTER_GUARD) {
                debugPrint("Bad guard end detected in system block during validate.\n");
                return false;
            }

            systemBlocks++;
            systemSize += blockHeader->size;
        }
    }

    if (systemBlocks != heap->systemBlocks) {
        debugPrint("Invalid number of system blocks.\n");
        return false;
    }

    if (systemSize != heap->systemSize) {
        debugPrint("Invalid size of system blocks.\n");
        return false;
    }

    return true;
}

} // namespace fallout
