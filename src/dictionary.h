#ifndef DICTIONARY_H
#define DICTIONARY_H

#include "memory_defs.h"

// A tuple containing individual key-value pair of a dictionary.
typedef struct DictionaryEntry {
    char* key;
    void* value;
} DictionaryEntry;

// A collection of key/value pairs.
//
// The keys in dictionary are always strings. Internally dictionary entries
// are kept sorted by the key. Both keys and values are copied when new entry
// is added to dictionary. For this reason the size of the value's type is
// provided during dictionary initialization.
typedef struct Dictionary {
    int marker;

    // The number of key/value pairs in the dictionary.
    int entriesLength;

    // The capacity of key/value pairs in [entries] array.
    int entriesCapacity;

    // The size of the dictionary values in bytes.
    size_t valueSize;

    int field_10;
    int field_14;
    int field_18;
    int field_1C;
    int field_20;

    // The array of key-value pairs.
    DictionaryEntry* entries;
} Dictionary;

int dictionaryInit(Dictionary* dictionary, int initialCapacity, size_t valueSize, void* a4);
int dictionarySetCapacity(Dictionary* dictionary, int newCapacity);
int dictionaryFree(Dictionary* dictionary);
int dictionaryGetIndexByKey(Dictionary* dictionary, const char* key);
int dictionaryAddValue(Dictionary* dictionary, const char* key, const void* value);
int dictionaryRemoveValue(Dictionary* dictionary, const char* key);
void dictionarySetMemoryProcs(MallocProc* mallocProc, ReallocProc* reallocProc, FreeProc* freeProc);

#endif /* DICTIONARY_H */
