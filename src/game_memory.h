#ifndef GAME_MEMORY_H
#define GAME_MEMORY_H

#include "memory_defs.h"

int gameMemoryInit();
void* gameMemoryMalloc(size_t size);
void* gameMemoryRealloc(void* ptr, size_t newSize);
void gameMemoryFree(void* ptr);

#endif /* GAME_MEMORY_H */
