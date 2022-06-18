#ifndef MEMORY_H
#define MEMORY_H

#include "memory_defs.h"

char* internal_strdup(const char* string);
void* internal_malloc(size_t size);
void* internal_realloc(void* ptr, size_t size);
void internal_free(void* ptr);

#endif /* MEMORY_H */
