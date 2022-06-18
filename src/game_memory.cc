#include "game_memory.h"

#include "db.h"
#include "dictionary.h"
#include "memory.h"
#include "memory_defs.h"
#include "memory_manager.h"

static void* gameMemoryMalloc(size_t size);
static void* gameMemoryRealloc(void* ptr, size_t newSize);
static void gameMemoryFree(void* ptr);

// 0x44B250
int gameMemoryInit()
{
    dictionarySetMemoryProcs(internal_malloc, internal_realloc, internal_free);
    _db_register_mem(internal_malloc, internal_strdup, internal_free);
    memoryManagerSetProcs(gameMemoryMalloc, gameMemoryRealloc, gameMemoryFree);

    return 0;
}

// 0x44B294
static void* gameMemoryMalloc(size_t size)
{
    return internal_malloc(size);
}

// 0x44B29C
static void* gameMemoryRealloc(void* ptr, size_t newSize)
{
    return internal_realloc(ptr, newSize);
}

// 0x44B2A4
static void gameMemoryFree(void* ptr)
{
    internal_free(ptr);
}
