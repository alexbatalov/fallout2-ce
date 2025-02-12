#include "cache.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "debug.h"
#include "memory.h"
#include "sound.h"

namespace fallout {

// The initial number of cache entries in new cache.
#define CACHE_ENTRIES_INITIAL_CAPACITY (100)

// The number of cache entries added when cache capacity is reached.
#define CACHE_ENTRIES_GROW_CAPACITY (50)

static bool cacheFetchEntryForKey(Cache* cache, int key, int* indexPtr);
static bool cacheInsertEntryAtIndex(Cache* cache, CacheEntry* cacheEntry, int index);
static int cacheFindIndexForKey(Cache* cache, int key, int* indexPtr);
static bool cacheEntryInit(CacheEntry* cacheEntry);
static bool cacheEntryFree(Cache* cache, CacheEntry* cacheEntry);
static bool cacheClean(Cache* cache);
static bool cacheResetStatistics(Cache* cache);
static bool cacheEnsureSize(Cache* cache, int size);
static bool cacheSweep(Cache* cache);
static bool cacheSetCapacity(Cache* cache, int newCapacity);
static int cacheEntriesCompareByUsage(const void* a1, const void* a2);
static int cacheEntriesCompareByMostRecentHit(const void* a1, const void* a2);

// 0x510938
static int _lock_sound_ticker = 0;

// cache_init
// 0x41FCC0
bool cacheInit(Cache* cache, CacheSizeProc* sizeProc, CacheReadProc* readProc, CacheFreeProc* freeProc, int maxSize)
{
    if (!heapInit(&(cache->heap), maxSize)) {
        return false;
    }

    cache->size = 0;
    cache->maxSize = maxSize;
    cache->entriesLength = 0;
    cache->entriesCapacity = CACHE_ENTRIES_INITIAL_CAPACITY;
    cache->hits = 0;
    cache->entries = (CacheEntry**)internal_malloc(sizeof(*cache->entries) * cache->entriesCapacity);
    cache->sizeProc = sizeProc;
    cache->readProc = readProc;
    cache->freeProc = freeProc;

    if (cache->entries == nullptr) {
        return false;
    }

    memset(cache->entries, 0, sizeof(*cache->entries) * cache->entriesCapacity);

    return true;
}

// cache_exit
// 0x41FD50
bool cacheFree(Cache* cache)
{
    if (cache == nullptr) {
        return false;
    }

    cacheClean(cache);
    cacheFlush(cache);
    heapFree(&(cache->heap));

    cache->size = 0;
    cache->maxSize = 0;
    cache->entriesLength = 0;
    cache->entriesCapacity = 0;
    cache->hits = 0;

    if (cache->entries != nullptr) {
        internal_free(cache->entries);
        cache->entries = nullptr;
    }

    cache->sizeProc = nullptr;
    cache->readProc = nullptr;
    cache->freeProc = nullptr;

    return true;
}

// 0x41FDE8
bool cacheLock(Cache* cache, int key, void** data, CacheEntry** cacheEntryPtr)
{
    if (cache == nullptr || data == nullptr || cacheEntryPtr == nullptr) {
        return false;
    }

    *cacheEntryPtr = nullptr;

    int index;
    int rc = cacheFindIndexForKey(cache, key, &index);
    if (rc == 2) {
        // Use existing cache entry.
        CacheEntry* cacheEntry = cache->entries[index];
        cacheEntry->hits++;
    } else if (rc == 3) {
        // New cache entry is required.
        if (cache->entriesLength >= INT_MAX) {
            return false;
        }

        if (!cacheFetchEntryForKey(cache, key, &index)) {
            return false;
        }

        _lock_sound_ticker %= 4;
        if (_lock_sound_ticker == 0) {
            soundContinueAll();
        }
    } else {
        return false;
    }

    CacheEntry* cacheEntry = cache->entries[index];
    if (cacheEntry->referenceCount == 0) {
        if (!heapLock(&(cache->heap), cacheEntry->heapHandleIndex, &(cacheEntry->data))) {
            return false;
        }
    }

    cacheEntry->referenceCount++;

    cache->hits++;
    cacheEntry->mru = cache->hits;

    if (cache->hits == UINT_MAX) {
        cacheResetStatistics(cache);
    }

    *data = cacheEntry->data;
    *cacheEntryPtr = cacheEntry;

    return true;
}

// 0x4200B8
bool cacheUnlock(Cache* cache, CacheEntry* cacheEntry)
{
    if (cache == nullptr || cacheEntry == nullptr) {
        return false;
    }

    if (cacheEntry->referenceCount == 0) {
        return false;
    }

    cacheEntry->referenceCount--;

    if (cacheEntry->referenceCount == 0) {
        heapUnlock(&(cache->heap), cacheEntry->heapHandleIndex);
    }

    return true;
}

// cache_flush
// 0x42012C
bool cacheFlush(Cache* cache)
{
    if (cache == nullptr) {
        return false;
    }

    // Loop thru cache entries and mark those with no references for eviction.
    for (int index = 0; index < cache->entriesLength; index++) {
        CacheEntry* cacheEntry = cache->entries[index];
        if (cacheEntry->referenceCount == 0) {
            cacheEntry->flags |= CACHE_ENTRY_MARKED_FOR_EVICTION;
        }
    }

    // Sweep cache entries marked earlier.
    cacheSweep(cache);

    // Shrink cache entries array if it's too big.
    int optimalCapacity = cache->entriesLength + CACHE_ENTRIES_GROW_CAPACITY;
    if (optimalCapacity < cache->entriesCapacity) {
        cacheSetCapacity(cache, optimalCapacity);
    }

    return true;
}

// 0x42019C
bool cachePrintStats(Cache* cache, char* dest, size_t size)
{
    if (cache == nullptr || dest == nullptr) {
        return false;
    }

    snprintf(dest, size, "Cache stats are disabled.%s", "\n");

    return true;
}

// Fetches entry for the specified key into the cache.
//
// 0x4203AC
static bool cacheFetchEntryForKey(Cache* cache, int key, int* indexPtr)
{
    CacheEntry* cacheEntry = (CacheEntry*)internal_malloc(sizeof(*cacheEntry));
    if (cacheEntry == nullptr) {
        return false;
    }

    if (!cacheEntryInit(cacheEntry)) {
        return false;
    }

    do {
        int size;
        if (cache->sizeProc(key, &size) != 0) {
            break;
        }

        if (!cacheEnsureSize(cache, size)) {
            break;
        }

        bool allocated = false;
        int cacheEntrySize = size;
        for (int attempt = 0; attempt < 10; attempt++) {
            if (heapBlockAllocate(&(cache->heap), &(cacheEntry->heapHandleIndex), size, 1)) {
                allocated = true;
                break;
            }

            cacheEntrySize = (int)((double)cacheEntrySize + (double)size * 0.25);
            if (cacheEntrySize > cache->maxSize) {
                break;
            }

            if (!cacheEnsureSize(cache, cacheEntrySize)) {
                break;
            }
        }

        if (!allocated) {
            cacheFlush(cache);

            allocated = true;
            if (!heapBlockAllocate(&(cache->heap), &(cacheEntry->heapHandleIndex), size, 1)) {
                if (!heapBlockAllocate(&(cache->heap), &(cacheEntry->heapHandleIndex), size, 0)) {
                    allocated = false;
                }
            }
        }

        if (!allocated) {
            break;
        }

        do {
            if (!heapLock(&(cache->heap), cacheEntry->heapHandleIndex, &(cacheEntry->data))) {
                break;
            }

            if (cache->readProc(key, &size, cacheEntry->data) != 0) {
                break;
            }

            heapUnlock(&(cache->heap), cacheEntry->heapHandleIndex);

            cacheEntry->size = size;
            cacheEntry->key = key;

            bool isNewKey = true;
            if (*indexPtr < cache->entriesLength) {
                if (key < cache->entries[*indexPtr]->key) {
                    if (*indexPtr == 0 || key > cache->entries[*indexPtr - 1]->key) {
                        isNewKey = false;
                    }
                }
            }

            if (isNewKey) {
                if (cacheFindIndexForKey(cache, key, indexPtr) != 3) {
                    break;
                }
            }

            if (!cacheInsertEntryAtIndex(cache, cacheEntry, *indexPtr)) {
                break;
            }

            return true;
        } while (0);

        heapUnlock(&(cache->heap), cacheEntry->heapHandleIndex);
    } while (0);

    // NOTE: Uninline.
    cacheEntryFree(cache, cacheEntry);

    return false;
}

// 0x4205E8
static bool cacheInsertEntryAtIndex(Cache* cache, CacheEntry* cacheEntry, int index)
{
    // Ensure cache have enough space for new entry.
    if (cache->entriesLength == cache->entriesCapacity - 1) {
        if (!cacheSetCapacity(cache, cache->entriesCapacity + CACHE_ENTRIES_GROW_CAPACITY)) {
            return false;
        }
    }

    // Move entries below insertion point.
    memmove(&(cache->entries[index + 1]), &(cache->entries[index]), sizeof(*cache->entries) * (cache->entriesLength - index));

    cache->entries[index] = cacheEntry;
    cache->entriesLength++;
    cache->size += cacheEntry->size;

    return true;
}

// Finds index for given key.
//
// Returns 2 if entry already exists in cache, or 3 if entry does not exist. In
// this case indexPtr represents insertion point.
//
// 0x420654
static int cacheFindIndexForKey(Cache* cache, int key, int* indexPtr)
{
    int length = cache->entriesLength;
    if (length == 0) {
        *indexPtr = 0;
        return 3;
    }

    int r = length - 1;
    int l = 0;
    int mid;
    int cmp;

    do {
        mid = (l + r) / 2;

        cmp = key - cache->entries[mid]->key;
        if (cmp == 0) {
            *indexPtr = mid;
            return 2;
        }

        if (cmp > 0) {
            l = l + 1;
        } else {
            r = r - 1;
        }
    } while (r >= l);

    if (cmp < 0) {
        *indexPtr = mid;
    } else {
        *indexPtr = mid + 1;
    }

    return 3;
}

// 0x420708
static bool cacheEntryInit(CacheEntry* cacheEntry)
{
    cacheEntry->key = 0;
    cacheEntry->size = 0;
    cacheEntry->data = nullptr;
    cacheEntry->referenceCount = 0;
    cacheEntry->hits = 0;
    cacheEntry->flags = 0;
    cacheEntry->mru = 0;
    return true;
}

// NOTE: Inlined.
//
// 0x420740
static bool cacheEntryFree(Cache* cache, CacheEntry* cacheEntry)
{
    if (cacheEntry->data != nullptr) {
        heapBlockDeallocate(&(cache->heap), &(cacheEntry->heapHandleIndex));
    }

    internal_free(cacheEntry);

    return true;
}

// 0x420764
static bool cacheClean(Cache* cache)
{
    Heap* heap = &(cache->heap);
    for (int index = 0; index < cache->entriesLength; index++) {
        CacheEntry* cacheEntry = cache->entries[index];

        // NOTE: Original code is slightly different. For unknown reason it uses
        // inner loop to decrement `referenceCount` one by one. Probably using
        // some inlined function.
        if (cacheEntry->referenceCount != 0) {
            heapUnlock(heap, cacheEntry->heapHandleIndex);
            cacheEntry->referenceCount = 0;
        }
    }

    return true;
}

// 0x4207D4
static bool cacheResetStatistics(Cache* cache)
{
    if (cache == nullptr) {
        return false;
    }

    CacheEntry** entries = (CacheEntry**)internal_malloc(sizeof(*entries) * cache->entriesLength);
    if (entries == nullptr) {
        return false;
    }

    memcpy(entries, cache->entries, sizeof(*entries) * cache->entriesLength);

    qsort(entries, cache->entriesLength, sizeof(*entries), cacheEntriesCompareByMostRecentHit);

    for (int index = 0; index < cache->entriesLength; index++) {
        CacheEntry* cacheEntry = entries[index];
        cacheEntry->mru = index;
    }

    cache->hits = cache->entriesLength;

    // FIXME: Obviously leak `entries`.

    return true;
}

// Prepare cache for storing new entry with the specified size.
//
// 0x42084C
static bool cacheEnsureSize(Cache* cache, int size)
{
    if (size > cache->maxSize) {
        // The entry of given size is too big for caching, no matter what.
        return false;
    }

    if (cache->maxSize - cache->size >= size) {
        // There is space available for entry of given size, there is no need to
        // evict anything.
        return true;
    }

    CacheEntry** entries = (CacheEntry**)internal_malloc(sizeof(*entries) * cache->entriesLength);
    if (entries != nullptr) {
        memcpy(entries, cache->entries, sizeof(*entries) * cache->entriesLength);
        qsort(entries, cache->entriesLength, sizeof(*entries), cacheEntriesCompareByUsage);

        // The sweeping threshold is 20% of cache size plus size for the new
        // entry. Once the threshold is reached the marking process stops.
        int threshold = size + (int)((double)cache->size * 0.2);

        int accum = 0;
        int index;
        for (index = 0; index < cache->entriesLength; index++) {
            CacheEntry* entry = entries[index];
            if (entry->referenceCount == 0) {
                if (entry->size >= threshold) {
                    entry->flags |= CACHE_ENTRY_MARKED_FOR_EVICTION;

                    // We've just found one huge entry, there is no point to
                    // mark individual smaller entries in the code path below,
                    // reset the accumulator to skip it entirely.
                    accum = 0;
                    break;
                } else {
                    accum += entry->size;

                    if (accum >= threshold) {
                        break;
                    }
                }
            }
        }

        if (accum != 0) {
            // The loop below assumes index to be positioned on the entry, where
            // accumulator stopped. If we've reached the end, reposition
            // it to the last entry.
            if (index == cache->entriesLength) {
                index -= 1;
            }

            // Loop backwards from the point we've stopped and mark all
            // unreferenced entries for sweeping.
            for (; index >= 0; index--) {
                CacheEntry* entry = entries[index];
                if (entry->referenceCount == 0) {
                    entry->flags |= CACHE_ENTRY_MARKED_FOR_EVICTION;
                }
            }
        }

        internal_free(entries);
    }

    cacheSweep(cache);

    if (cache->maxSize - cache->size >= size) {
        return true;
    }

    return false;
}

// 0x42099C
static bool cacheSweep(Cache* cache)
{
    for (int index = 0; index < cache->entriesLength; index++) {
        CacheEntry* cacheEntry = cache->entries[index];
        if ((cacheEntry->flags & CACHE_ENTRY_MARKED_FOR_EVICTION) != 0) {
            if (cacheEntry->referenceCount != 0) {
                // Entry was marked for eviction but still has references,
                // unmark it.
                cacheEntry->flags &= ~CACHE_ENTRY_MARKED_FOR_EVICTION;
            } else {
                int cacheEntrySize = cacheEntry->size;

                // NOTE: Uninline.
                cacheEntryFree(cache, cacheEntry);

                // Move entries up.
                memmove(&(cache->entries[index]), &(cache->entries[index + 1]), sizeof(*cache->entries) * ((cache->entriesLength - index) - 1));

                cache->entriesLength--;
                cache->size -= cacheEntrySize;

                // The entry was removed, compensate index.
                index--;
            }
        }
    }

    return true;
}

// 0x420A40
static bool cacheSetCapacity(Cache* cache, int newCapacity)
{
    if (newCapacity < cache->entriesLength) {
        return false;
    }

    CacheEntry** entries = (CacheEntry**)internal_realloc(cache->entries, sizeof(*cache->entries) * newCapacity);
    if (entries == nullptr) {
        return false;
    }

    cache->entries = entries;
    cache->entriesCapacity = newCapacity;

    return true;
}

// 0x420A74
static int cacheEntriesCompareByUsage(const void* a1, const void* a2)
{
    CacheEntry* v1 = *(CacheEntry**)a1;
    CacheEntry* v2 = *(CacheEntry**)a2;

    if (v1->referenceCount != 0 && v2->referenceCount == 0) {
        return 1;
    }

    if (v2->referenceCount != 0 && v1->referenceCount == 0) {
        return -1;
    }

    if (v1->hits < v2->hits) {
        return -1;
    } else if (v1->hits > v2->hits) {
        return 1;
    }

    if (v1->mru < v2->mru) {
        return -1;
    } else if (v1->mru > v2->mru) {
        return 1;
    }

    return 0;
}

// 0x420AE8
static int cacheEntriesCompareByMostRecentHit(const void* a1, const void* a2)
{
    CacheEntry* v1 = *(CacheEntry**)a1;
    CacheEntry* v2 = *(CacheEntry**)a2;

    if (v1->mru < v2->mru) {
        return 1;
    } else if (v1->mru > v2->mru) {
        return -1;
    } else {
        return 0;
    }
}

} // namespace fallout
