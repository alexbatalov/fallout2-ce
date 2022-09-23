#ifndef HEAP_H
#define HEAP_H

namespace fallout {

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

bool heapInit(Heap* heap, int a2);
bool heapFree(Heap* heap);
bool heapBlockAllocate(Heap* heap, int* handleIndexPtr, int size, int a3);
bool heapBlockDeallocate(Heap* heap, int* handleIndexPtr);
bool heapLock(Heap* heap, int handleIndex, unsigned char** bufferPtr);
bool heapUnlock(Heap* heap, int handleIndex);
bool heapValidate(Heap* heap);

} // namespace fallout

#endif /* HEAP_H */
