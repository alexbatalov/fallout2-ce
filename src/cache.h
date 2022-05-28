#ifndef CACHE_H
#define CACHE_H

#include "heap.h"

#define INVALID_CACHE_ENTRY ((CacheEntry*)-1)

// The initial number of cache entries in new cache.
#define CACHE_ENTRIES_INITIAL_CAPACITY (100)

// The number of cache entries added when cache capacity is reached.
#define CACHE_ENTRIES_GROW_CAPACITY (50)

typedef enum CacheEntryFlags {
    // Specifies that cache entry has no references as should be evicted during
    // the next sweep operation.
    CACHE_ENTRY_MARKED_FOR_EVICTION = 0x01,
} CacheEntryFlags;

typedef int CacheSizeProc(int key, int* sizePtr);
typedef int CacheReadProc(int key, int* sizePtr, unsigned char* buffer);
typedef void CacheFreeProc(void* ptr);

typedef struct CacheEntry {
    int key;
    int size;
    unsigned char* data;
    unsigned int referenceCount;

    // Total number of hits that this cache entry received during it's
    // lifetime.
    unsigned int hits;

    unsigned int flags;

    // The most recent hit in terms of cache hit counter. Used to track most
    // recently used entries in eviction strategy.
    unsigned int mru;

    int heapHandleIndex;
} CacheEntry;

typedef struct Cache {
    // Current size of entries in cache.
    int size;

    // Maximum size of entries in cache.
    int maxSize;

    // The length of `entries` array.
    int entriesLength;

    // The capacity of `entries` array.
    int entriesCapacity;

    // Total number of hits during cache lifetime.
    unsigned int hits;

    // List of cache entries.
    CacheEntry** entries;

    CacheSizeProc* sizeProc;
    CacheReadProc* readProc;
    CacheFreeProc* freeProc;
    Heap heap;
} Cache;

bool cacheInit(Cache* cache, CacheSizeProc* sizeProc, CacheReadProc* readProc, CacheFreeProc* freeProc, int maxSize);
bool cacheFree(Cache* cache);
bool cacheLock(Cache* cache, int key, void** data, CacheEntry** cacheEntryPtr);
bool cacheUnlock(Cache* cache, CacheEntry* cacheEntry);
bool cacheFlush(Cache* cache);
bool cachePrintStats(Cache* cache, char* dest);
bool cacheFetchEntryForKey(Cache* cache, int key, int* indexPtr);
bool cacheInsertEntryAtIndex(Cache* cache, CacheEntry* cacheEntry, int index);
int cacheFindIndexForKey(Cache* cache, int key, int* indexPtr);
bool cacheEntryInit(CacheEntry* cacheEntry);
bool cacheEntryFree(Cache* cache, CacheEntry* cacheEntry);
bool cacheClean(Cache* cache);
bool cacheResetStatistics(Cache* cache);
bool cacheEnsureSize(Cache* cache, int size);
bool cacheSweep(Cache* cache);
bool cacheSetCapacity(Cache* cache, int newCapacity);
int cacheEntriesCompareByUsage(const void* a1, const void* a2);
int cacheEntriesCompareByMostRecentHit(const void* a1, const void* a2);

#endif /* CACHE_H */
