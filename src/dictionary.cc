#include "dictionary.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "platform_compat.h"

namespace fallout {

// NOTE: I guess this marker is used as a type discriminator for implementing
// nested dictionaries. That's why every dictionary-related function starts
// with a check for this value.
#define DICTIONARY_MARKER 0xFEBAFEBA

static void* dictionaryMallocDefaultImpl(size_t size);
static void* dictionaryReallocDefaultImpl(void* ptr, size_t newSize);
static void dictionaryFreeDefaultImpl(void* ptr);
static int dictionaryFindIndexForKey(Dictionary* dictionary, const char* key, int* index);

// 0x51E408
static MallocProc* gDictionaryMallocProc = dictionaryMallocDefaultImpl;

// 0x51E40C
static ReallocProc* gDictionaryReallocProc = dictionaryReallocDefaultImpl;

// 0x51E410
static FreeProc* gDictionaryFreeProc = dictionaryFreeDefaultImpl;

// 0x4D9B90
static void* dictionaryMallocDefaultImpl(size_t size)
{
    return malloc(size);
}

// 0x4D9B98
static void* dictionaryReallocDefaultImpl(void* ptr, size_t newSize)
{
    return realloc(ptr, newSize);
}

// 0x4D9BA0
static void dictionaryFreeDefaultImpl(void* ptr)
{
    free(ptr);
}

// 0x4D9BA8
int dictionaryInit(Dictionary* dictionary, int initialCapacity, size_t valueSize, DictionaryIO* io)
{
    dictionary->entriesCapacity = initialCapacity;
    dictionary->valueSize = valueSize;
    dictionary->entriesLength = 0;

    if (io != nullptr) {
        memcpy(&(dictionary->io), io, sizeof(*io));
    } else {
        dictionary->io.readProc = nullptr;
        dictionary->io.writeProc = nullptr;
        dictionary->io.field_8 = 0;
        dictionary->io.field_C = 0;
    }

    int rc = 0;

    if (initialCapacity != 0) {
        dictionary->entries = (DictionaryEntry*)gDictionaryMallocProc(sizeof(*dictionary->entries) * initialCapacity);
        if (dictionary->entries == nullptr) {
            rc = -1;
        }
    } else {
        dictionary->entries = nullptr;
    }

    if (rc != -1) {
        dictionary->marker = DICTIONARY_MARKER;
    }

    return rc;
}

// 0x4D9C0C
int dictionarySetCapacity(Dictionary* dictionary, int newCapacity)
{
    if (dictionary->marker != DICTIONARY_MARKER) {
        return -1;
    }

    if (newCapacity < dictionary->entriesLength) {
        return -1;
    }

    DictionaryEntry* entries = (DictionaryEntry*)gDictionaryReallocProc(dictionary->entries, sizeof(*dictionary->entries) * newCapacity);
    if (entries == nullptr) {
        return -1;
    }

    dictionary->entriesCapacity = newCapacity;
    dictionary->entries = entries;

    return 0;
}

// 0x4D9C48
int dictionaryFree(Dictionary* dictionary)
{
    if (dictionary->marker != DICTIONARY_MARKER) {
        return -1;
    }

    for (int index = 0; index < dictionary->entriesLength; index++) {
        DictionaryEntry* entry = &(dictionary->entries[index]);
        if (entry->key != nullptr) {
            gDictionaryFreeProc(entry->key);
        }

        if (entry->value != nullptr) {
            gDictionaryFreeProc(entry->value);
        }
    }

    if (dictionary->entries != nullptr) {
        gDictionaryFreeProc(dictionary->entries);
    }

    memset(dictionary, 0, sizeof(*dictionary));

    return 0;
}

// Finds index for the given key.
//
// Returns 0 if key is found. Otherwise returns -1, in this case [indexPtr]
// specifies an insertion point for given key.
//
// 0x4D9CC4
static int dictionaryFindIndexForKey(Dictionary* dictionary, const char* key, int* indexPtr)
{
    if (dictionary->marker != DICTIONARY_MARKER) {
        return -1;
    }

    if (dictionary->entriesLength == 0) {
        *indexPtr = 0;
        return -1;
    }

    int r = dictionary->entriesLength - 1;
    int l = 0;
    int mid = 0;
    int cmp = 0;
    while (r >= l) {
        mid = (l + r) / 2;

        cmp = compat_stricmp(key, dictionary->entries[mid].key);
        if (cmp == 0) {
            break;
        }

        if (cmp > 0) {
            l = l + 1;
        } else {
            r = r - 1;
        }
    }

    if (cmp == 0) {
        *indexPtr = mid;
        return 0;
    }

    if (cmp < 0) {
        *indexPtr = mid;
    } else {
        *indexPtr = mid + 1;
    }

    return -1;
}

// Returns the index of the entry for the specified key, or -1 if it's not
// present in the dictionary.
//
// 0x4D9D5C
int dictionaryGetIndexByKey(Dictionary* dictionary, const char* key)
{
    if (dictionary->marker != DICTIONARY_MARKER) {
        return -1;
    }

    int index;
    if (dictionaryFindIndexForKey(dictionary, key, &index) != 0) {
        return -1;
    }

    return index;
}

// Adds key-value pair to the dictionary if the specified key is not already
// present.
//
// Returns 0 on success, or -1 on any error (including key already exists
// error).
//
// 0x4D9D88
int dictionaryAddValue(Dictionary* dictionary, const char* key, const void* value)
{
    if (dictionary->marker != DICTIONARY_MARKER) {
        return -1;
    }

    int newElementIndex;
    if (dictionaryFindIndexForKey(dictionary, key, &newElementIndex) == 0) {
        // Element for this key is already exists.
        return -1;
    }

    if (dictionary->entriesLength == dictionary->entriesCapacity) {
        // Dictionary reached it's capacity and needs to be enlarged.
        if (dictionarySetCapacity(dictionary, 2 * (dictionary->entriesCapacity + 1)) == -1) {
            return -1;
        }
    }

    // Make a copy of the key.
    char* keyCopy = (char*)gDictionaryMallocProc(strlen(key) + 1);
    if (keyCopy == nullptr) {
        return -1;
    }

    strcpy(keyCopy, key);

    // Make a copy of the value.
    void* valueCopy = nullptr;
    if (value != nullptr && dictionary->valueSize != 0) {
        valueCopy = gDictionaryMallocProc(dictionary->valueSize);
        if (valueCopy == nullptr) {
            gDictionaryFreeProc(keyCopy);
            return -1;
        }
    }

    if (valueCopy != nullptr && dictionary->valueSize != 0) {
        memcpy(valueCopy, value, dictionary->valueSize);
    }

    // Starting at the end of entries array loop backwards and move entries down
    // one by one until we reach insertion point.
    for (int index = dictionary->entriesLength; index > newElementIndex; index--) {
        DictionaryEntry* src = &(dictionary->entries[index - 1]);
        DictionaryEntry* dest = &(dictionary->entries[index]);
        memcpy(dest, src, sizeof(*dictionary->entries));
    }

    DictionaryEntry* entry = &(dictionary->entries[newElementIndex]);
    entry->key = keyCopy;
    entry->value = valueCopy;

    dictionary->entriesLength++;

    return 0;
}

// Removes key-value pair from the dictionary if specified key is present in
// the dictionary.
//
// Returns 0 on success, -1 on any error (including key not present error).
//
// 0x4D9EE8
int dictionaryRemoveValue(Dictionary* dictionary, const char* key)
{
    if (dictionary->marker != DICTIONARY_MARKER) {
        return -1;
    }

    int indexToRemove;
    if (dictionaryFindIndexForKey(dictionary, key, &indexToRemove) == -1) {
        return -1;
    }

    DictionaryEntry* entry = &(dictionary->entries[indexToRemove]);

    // Free key and value (which are copies).
    gDictionaryFreeProc(entry->key);
    if (entry->value != nullptr) {
        gDictionaryFreeProc(entry->value);
    }

    dictionary->entriesLength--;

    // Starting from the index of the entry we've just removed, loop thru the
    // remaining of the array and move entries up one by one.
    for (int index = indexToRemove; index < dictionary->entriesLength; index++) {
        DictionaryEntry* src = &(dictionary->entries[index + 1]);
        DictionaryEntry* dest = &(dictionary->entries[index]);
        memcpy(dest, src, sizeof(*dictionary->entries));
    }

    return 0;
}

// NOTE: Unused.
//
// 0x4D9F84
int dictionaryCopy(Dictionary* dest, Dictionary* src)
{
    if (src->marker != DICTIONARY_MARKER) {
        return -1;
    }

    if (dictionaryInit(dest, src->entriesCapacity, src->valueSize, &(src->io)) != 0) {
        // FIXME: Should return -1, as we were unable to initialize dictionary.
        return 0;
    }

    for (int index = 0; index < src->entriesLength; index++) {
        DictionaryEntry* entry = &(src->entries[index]);
        if (dictionaryAddValue(dest, entry->key, entry->value) == -1) {
            return -1;
        }
    }

    return 0;
}

// NOTE: Unused.
//
// 0x4DA090
int dictionaryReadInt(FILE* stream, int* valuePtr)
{
    int ch;
    int value;

    ch = fgetc(stream);
    if (ch == -1) {
        return -1;
    }

    value = (ch & 0xFF);

    ch = fgetc(stream);
    if (ch == -1) {
        return -1;
    }

    value = (value << 8) | (ch & 0xFF);

    ch = fgetc(stream);
    if (ch == -1) {
        return -1;
    }

    value = (value << 8) | (ch & 0xFF);

    ch = fgetc(stream);
    if (ch == -1) {
        return -1;
    }

    value = (value << 8) | (ch & 0xFF);

    *valuePtr = value;

    return 0;
}

// NOTE: Unused.
//
// 0x4DA0F4
int dictionaryReadHeader(FILE* stream, Dictionary* dictionary)
{
    int value;

    if (dictionaryReadInt(stream, &value) != 0) return -1;
    dictionary->entriesLength = value;

    if (dictionaryReadInt(stream, &value) != 0) return -1;
    dictionary->entriesCapacity = value;

    if (dictionaryReadInt(stream, &value) != 0) return -1;
    dictionary->valueSize = value;

    // NOTE: Originally reads `values` pointer.
    if (dictionaryReadInt(stream, &value) != 0) return -1;

    return 0;
}

// NOTE: Unused.
//
// 0x4DA158
int dictionaryLoad(FILE* stream, Dictionary* dictionary, int a3)
{
    if (dictionary->marker != DICTIONARY_MARKER) {
        return -1;
    }

    for (int index = 0; index < dictionary->entriesLength; index++) {
        DictionaryEntry* entry = &(dictionary->entries[index]);
        if (entry->key != nullptr) {
            gDictionaryFreeProc(entry->key);
        }

        if (entry->value != nullptr) {
            gDictionaryFreeProc(entry->value);
        }
    }

    if (dictionary->entries != nullptr) {
        gDictionaryFreeProc(dictionary->entries);
    }

    if (dictionaryReadHeader(stream, dictionary) != 0) {
        return -1;
    }

    dictionary->entries = nullptr;

    if (dictionary->entriesCapacity <= 0) {
        return 0;
    }

    dictionary->entries = (DictionaryEntry*)gDictionaryMallocProc(sizeof(*dictionary->entries) * dictionary->entriesCapacity);
    if (dictionary->entries == nullptr) {
        return -1;
    }

    for (int index = 0; index < dictionary->entriesLength; index++) {
        DictionaryEntry* entry = &(dictionary->entries[index]);
        entry->key = nullptr;
        entry->value = nullptr;
    }

    if (dictionary->entriesLength <= 0) {
        return 0;
    }

    for (int index = 0; index < dictionary->entriesLength; index++) {
        DictionaryEntry* entry = &(dictionary->entries[index]);
        int keyLength = fgetc(stream);
        if (keyLength == -1) {
            return -1;
        }

        entry->key = (char*)gDictionaryMallocProc(keyLength + 1);
        if (entry->key == nullptr) {
            return -1;
        }

        if (compat_fgets(entry->key, keyLength + 1, stream) == nullptr) {
            return -1;
        }

        if (dictionary->valueSize != 0) {
            entry->value = gDictionaryMallocProc(dictionary->valueSize);
            if (entry->value == nullptr) {
                return -1;
            }

            if (dictionary->io.readProc != nullptr) {
                if (dictionary->io.readProc(stream, entry->value, dictionary->valueSize, a3) != 0) {
                    return -1;
                }
            } else {
                if (fread(entry->value, dictionary->valueSize, 1, stream) != 1) {
                    return -1;
                }
            }
        }
    }

    return 0;
}

// NOTE: Unused.
//
// 0x4DA2EC
int dictionaryWriteInt(FILE* stream, int value)
{
    if (fputc((value >> 24) & 0xFF, stream) == -1) return -1;
    if (fputc((value >> 16) & 0xFF, stream) == -1) return -1;
    if (fputc((value >> 8) & 0xFF, stream) == -1) return -1;
    if (fputc(value & 0xFF, stream) == -1) return -1;

    return 0;
}

// NOTE: Unused.
//
// 0x4DA360
int dictionaryWriteHeader(FILE* stream, Dictionary* dictionary)
{
    if (dictionaryWriteInt(stream, dictionary->entriesLength) != 0) return -1;
    if (dictionaryWriteInt(stream, dictionary->entriesCapacity) != 0) return -1;
    if (dictionaryWriteInt(stream, dictionary->valueSize) != 0) return -1;
    // NOTE: Originally writes `entries` pointer.
    if (dictionaryWriteInt(stream, 0) != 0) return -1;

    return 0;
}

// NOTE: Unused.
//
// 0x4DA3A4
int dictionaryWrite(FILE* stream, Dictionary* dictionary, int a3)
{
    if (dictionary->marker != DICTIONARY_MARKER) {
        return -1;
    }

    if (dictionaryWriteHeader(stream, dictionary) != 0) {
        return -1;
    }

    for (int index = 0; index < dictionary->entriesLength; index++) {
        DictionaryEntry* entry = &(dictionary->entries[index]);
        int keyLength = strlen(entry->key);
        if (fputc(keyLength, stream) == -1) {
            return -1;
        }

        if (fputs(entry->key, stream) == -1) {
            return -1;
        }

        if (dictionary->io.writeProc != nullptr) {
            if (dictionary->valueSize != 0) {
                if (dictionary->io.writeProc(stream, entry->value, dictionary->valueSize, a3) != 0) {
                    return -1;
                }
            }
        } else {
            if (dictionary->valueSize != 0) {
                if (fwrite(entry->value, dictionary->valueSize, 1, stream) != 1) {
                    return -1;
                }
            }
        }
    }

    return 0;
}

// 0x4DA498
void dictionarySetMemoryProcs(MallocProc* mallocProc, ReallocProc* reallocProc, FreeProc* freeProc)
{
    if (mallocProc != nullptr && reallocProc != nullptr && freeProc != nullptr) {
        gDictionaryMallocProc = mallocProc;
        gDictionaryReallocProc = reallocProc;
        gDictionaryFreeProc = freeProc;
    } else {
        gDictionaryMallocProc = dictionaryMallocDefaultImpl;
        gDictionaryReallocProc = dictionaryReallocDefaultImpl;
        gDictionaryFreeProc = dictionaryFreeDefaultImpl;
    }
}

} // namespace fallout
