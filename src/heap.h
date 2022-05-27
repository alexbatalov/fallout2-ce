#ifndef HEAP_H
#define HEAP_H

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

typedef struct HeapHandle {
    unsigned int state;
    unsigned char* data;
} HeapHandle;

typedef struct Heap {
    int size;
    int freeBlocks;
    int moveableBlocks;
    int lockedBlocks;
    int systemBlocks;
    int handlesLength;
    int freeSize;
    int moveableSize;
    int lockedSize;
    int systemSize;
    HeapHandle* handles;
    unsigned char* data;
} Heap;

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

extern unsigned char** gHeapFreeBlocks;
extern int gHeapFreeBlocksLength;
extern HeapMoveableExtent* gHeapMoveableExtents;
extern int gHeapMoveableExtentsLength;
extern unsigned char** gHeapMoveableBlocks;
extern int gHeapMoveableBlocksLength;
extern int* gHeapReservedFreeBlockIndexes;
extern int gHeapReservedFreeBlockIndexesLength;
extern int gHeapsCount;

bool heapInternalsInit();
void heapInternalsFree();
bool heapInit(Heap* heap, int a2);
bool heapFree(Heap* heap);
bool heapHandleListInit(Heap* heap);
bool heapBlockAllocate(Heap* heap, int* handleIndexPtr, int size, int a3);
bool heapBlockDeallocate(Heap* heap, int* handleIndexPtr);
bool heapLock(Heap* heap, int handleIndex, unsigned char** bufferPtr);
bool heapUnlock(Heap* heap, int handleIndex);
bool heapPrintStats(Heap* heap, char* dest);
bool heapFindFreeHandle(Heap* heap, int* handleIndexPtr);
bool heapFindFreeBlock(Heap* heap, int size, void** blockPtr, int a4);
int heapBlockCompareBySize(const void* a1, const void* a2);
int heapMoveableExtentsCompareBySize(const void* a1, const void* a2);
bool heapBuildMoveableExtentsList(Heap* heap, int* moveableExtentsLengthPtr, int* maxBlocksLengthPtr);
bool heapBuildFreeBlocksList(Heap* heap);
bool heapBuildMoveableBlocksList(int extentIndex);
bool heapValidate(Heap* heap);

#endif /* HEAP_H */
