#include "sound_effects_list.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "db.h"
#include "debug.h"
#include "memory.h"
#include "platform_compat.h"
#include "sound_decoder.h"

namespace fallout {

typedef struct SoundEffectsListEntry {
    char* name;
    int dataSize;
    int fileSize;
    int tag;
} SoundEffectsListEntry;

static int soundEffectsListTagToIndex(int tag, int* indexPtr);
static void soundEffectsListClear();
static int soundEffectsListPopulateFileNames();
static int soundEffectsListCopyFileNames(char** fileNameList);
static int soundEffectsListPopulateFileSizes();
static int soundEffectsListSort();
static int soundEffectsListCompareByName(const void* a1, const void* a2);
static int soundEffectsListSoundDecoderReadHandler(void* data, void* buf, unsigned int size);

// 0x51C8F8
static bool gSoundEffectsListInitialized = false;

// 0x51C8FC
static int gSoundEffectsListDebugLevel = INT_MAX;

// sfxl_effect_path
// 0x51C900
static char* gSoundEffectsListPath = nullptr;

// sfxl_effect_path_len
// 0x51C904
static int gSoundEffectsListPathLength = 0;

// sndlist.lst
//
// sfxl_list
// 0x51C908
static SoundEffectsListEntry* gSoundEffectsListEntries = nullptr;

// The length of [gSoundEffectsListEntries] array.
//
// 0x51C90C
static int gSoundEffectsListEntriesLength = 0;

// 0x667F94
static int _sfxl_compression;

// sfxl_tag_is_legal
// 0x4A98E0
bool soundEffectsListIsValidTag(int a1)
{
    return soundEffectsListTagToIndex(a1, nullptr) == SFXL_OK;
}

// sfxl_init
// 0x4A98F4
int soundEffectsListInit(const char* soundEffectsPath, int a2, int debugLevel)
{
    char path[COMPAT_MAX_PATH];

    // TODO: What for?
    // memcpy(path, byte_4A97E0, 0xFF);

    gSoundEffectsListDebugLevel = debugLevel;
    _sfxl_compression = a2;
    gSoundEffectsListEntriesLength = 0;

    gSoundEffectsListPath = internal_strdup(soundEffectsPath);
    if (gSoundEffectsListPath == nullptr) {
        return SFXL_ERR;
    }

    gSoundEffectsListPathLength = strlen(gSoundEffectsListPath);

    if (gSoundEffectsListPathLength == 0 || soundEffectsPath[gSoundEffectsListPathLength - 1] == '\\') {
        snprintf(path, sizeof(path), "%sSNDLIST.LST", soundEffectsPath);
    } else {
        snprintf(path, sizeof(path), "%s\\SNDLIST.LST", soundEffectsPath);
    }

    File* stream = fileOpen(path, "rt");
    if (stream != nullptr) {
        fileReadString(path, 255, stream);
        gSoundEffectsListEntriesLength = atoi(path);

        gSoundEffectsListEntries = (SoundEffectsListEntry*)internal_malloc(sizeof(*gSoundEffectsListEntries) * gSoundEffectsListEntriesLength);
        for (int index = 0; index < gSoundEffectsListEntriesLength; index++) {
            SoundEffectsListEntry* entry = &(gSoundEffectsListEntries[index]);

            fileReadString(path, 255, stream);

            // Remove trailing newline.
            *(path + strlen(path) - 1) = '\0';
            entry->name = internal_strdup(path);

            fileReadString(path, 255, stream);
            entry->dataSize = atoi(path);

            fileReadString(path, 255, stream);
            entry->fileSize = atoi(path);

            fileReadString(path, 255, stream);
            entry->tag = atoi(path);
        }

        fileClose(stream);

        debugPrint("Reading SNDLIST.LST Sound FX Count: %d", gSoundEffectsListEntriesLength);
    } else {
        int err;

        err = soundEffectsListPopulateFileNames();
        if (err != SFXL_OK) {
            internal_free(gSoundEffectsListPath);
            return err;
        }

        err = soundEffectsListPopulateFileSizes();
        if (err != SFXL_OK) {
            soundEffectsListClear();
            internal_free(gSoundEffectsListPath);
            return err;
        }

        // NOTE: For unknown reason tag generation functionality is missing.
        // You won't be able to produce the same SNDLIST.LST as the game have.
        // All tags will be 0 (see [soundEffectsListPopulateFileNames]).
        //
        // On the other hand, tags read from the SNDLIST.LST are not used in
        // the game. Instead tag is automatically determined from entry's
        // index (see [soundEffectsListGetTag]).

        // NOTE: Uninline.
        soundEffectsListSort();

        File* stream = fileOpen(path, "wt");
        if (stream != nullptr) {
            filePrintFormatted(stream, "%d\n", gSoundEffectsListEntriesLength);

            for (int index = 0; index < gSoundEffectsListEntriesLength; index++) {
                SoundEffectsListEntry* entry = &(gSoundEffectsListEntries[index]);

                filePrintFormatted(stream, "%s\n", entry->name);
                filePrintFormatted(stream, "%d\n", entry->dataSize);
                filePrintFormatted(stream, "%d\n", entry->fileSize);
                filePrintFormatted(stream, "%d\n", entry->tag);
            }

            fileClose(stream);
        } else {
            debugPrint("SFXLIST: Can't open file for write %s\n", path);
        }
    }

    gSoundEffectsListInitialized = true;

    return SFXL_OK;
}

// 0x4A9C04
void soundEffectsListExit()
{
    if (gSoundEffectsListInitialized) {
        soundEffectsListClear();
        internal_free(gSoundEffectsListPath);
        gSoundEffectsListInitialized = false;
    }
}

// sfxl_name_to_tag
// 0x4A9C28
int soundEffectsListGetTag(char* name, int* tagPtr)
{
    if (compat_strnicmp(gSoundEffectsListPath, name, gSoundEffectsListPathLength) != 0) {
        return SFXL_ERR;
    }

    SoundEffectsListEntry dummy;
    dummy.name = name + gSoundEffectsListPathLength;

    SoundEffectsListEntry* entry = (SoundEffectsListEntry*)bsearch(&dummy, gSoundEffectsListEntries, gSoundEffectsListEntriesLength, sizeof(*gSoundEffectsListEntries), soundEffectsListCompareByName);
    if (entry == nullptr) {
        return SFXL_ERR;
    }

    int index = entry - gSoundEffectsListEntries;
    if (index < 0 || index >= gSoundEffectsListEntriesLength) {
        return SFXL_ERR;
    }

    *tagPtr = 2 * index + 2;

    return SFXL_OK;
}

// sfxl_name
// 0x4A9CD8
int soundEffectsListGetFilePath(int tag, char** pathPtr)
{
    int index;
    int err = soundEffectsListTagToIndex(tag, &index);
    if (err != SFXL_OK) {
        return err;
    }

    char* name = gSoundEffectsListEntries[index].name;

    char* path = (char*)internal_malloc(strlen(gSoundEffectsListPath) + strlen(name) + 1);
    if (path == nullptr) {
        return SFXL_ERR;
    }

    strcpy(path, gSoundEffectsListPath);
    strcat(path, name);

    *pathPtr = path;

    return SFXL_OK;
}

// 0x4A9D90
int soundEffectsListGetDataSize(int tag, int* sizePtr)
{
    int index;
    int rc = soundEffectsListTagToIndex(tag, &index);
    if (rc != SFXL_OK) {
        return rc;
    }

    SoundEffectsListEntry* entry = &(gSoundEffectsListEntries[index]);
    *sizePtr = entry->dataSize;

    return SFXL_OK;
}

// 0x4A9DBC
int soundEffectsListGetFileSize(int tag, int* sizePtr)
{
    int index;
    int err = soundEffectsListTagToIndex(tag, &index);
    if (err != SFXL_OK) {
        return err;
    }

    SoundEffectsListEntry* entry = &(gSoundEffectsListEntries[index]);
    *sizePtr = entry->fileSize;

    return SFXL_OK;
}

// sfxl_tag_to_index
// 0x4A9DE8
static int soundEffectsListTagToIndex(int tag, int* indexPtr)
{
    if (tag <= 0) {
        return SFXL_ERR_TAG_INVALID;
    }

    if ((tag & 1) != 0) {
        return SFXL_ERR_TAG_INVALID;
    }

    int index = (tag / 2) - 1;
    if (index >= gSoundEffectsListEntriesLength) {
        return SFXL_ERR_TAG_INVALID;
    }

    if (indexPtr != nullptr) {
        *indexPtr = index;
    }

    return SFXL_OK;
}

// 0x4A9E44
static void soundEffectsListClear()
{
    if (gSoundEffectsListEntriesLength < 0) {
        return;
    }

    if (gSoundEffectsListEntries == nullptr) {
        return;
    }

    for (int index = 0; index < gSoundEffectsListEntriesLength; index++) {
        SoundEffectsListEntry* entry = &(gSoundEffectsListEntries[index]);
        if (entry->name != nullptr) {
            internal_free(entry->name);
        }
    }

    internal_free(gSoundEffectsListEntries);
    gSoundEffectsListEntries = nullptr;

    gSoundEffectsListEntriesLength = 0;
}

// sfxl_get_names
// 0x4A9EA0
static int soundEffectsListPopulateFileNames()
{
    const char* extension;
    switch (_sfxl_compression) {
    case 0:
        extension = "*.SND";
        break;
    case 1:
        extension = "*.ACM";
        break;
    default:
        return SFXL_ERR;
    }

    char* pattern = (char*)internal_malloc(strlen(gSoundEffectsListPath) + strlen(extension) + 1);
    if (pattern == nullptr) {
        return SFXL_ERR;
    }

    strcpy(pattern, gSoundEffectsListPath);
    strcat(pattern, extension);

    char** fileNameList;
    gSoundEffectsListEntriesLength = fileNameListInit(pattern, &fileNameList, 0, 0);
    internal_free(pattern);

    if (gSoundEffectsListEntriesLength > 10000) {
        fileNameListFree(&fileNameList, 0);
        return SFXL_ERR;
    }

    if (gSoundEffectsListEntriesLength <= 0) {
        return SFXL_ERR;
    }

    gSoundEffectsListEntries = (SoundEffectsListEntry*)internal_malloc(sizeof(*gSoundEffectsListEntries) * gSoundEffectsListEntriesLength);
    if (gSoundEffectsListEntries == nullptr) {
        fileNameListFree(&fileNameList, 0);
        return SFXL_ERR;
    }

    memset(gSoundEffectsListEntries, 0, sizeof(*gSoundEffectsListEntries) * gSoundEffectsListEntriesLength);

    int err = soundEffectsListCopyFileNames(fileNameList);

    fileNameListFree(&fileNameList, 0);

    if (err != SFXL_OK) {
        soundEffectsListClear();
        return err;
    }

    return SFXL_OK;
}

// sfxl_copy_names
// 0x4AA000
static int soundEffectsListCopyFileNames(char** fileNameList)
{
    for (int index = 0; index < gSoundEffectsListEntriesLength; index++) {
        SoundEffectsListEntry* entry = &(gSoundEffectsListEntries[index]);
        entry->name = internal_strdup(*fileNameList++);
        if (entry->name == nullptr) {
            soundEffectsListClear();
            return SFXL_ERR;
        }
    }

    return SFXL_OK;
}

// 0x4AA050
static int soundEffectsListPopulateFileSizes()
{

    char* path = (char*)internal_malloc(gSoundEffectsListPathLength + 13);
    if (path == nullptr) {
        return SFXL_ERR;
    }

    strcpy(path, gSoundEffectsListPath);

    char* fileName = path + gSoundEffectsListPathLength;

    for (int index = 0; index < gSoundEffectsListEntriesLength; index++) {
        SoundEffectsListEntry* entry = &(gSoundEffectsListEntries[index]);
        strcpy(fileName, entry->name);

        int fileSize;
        if (dbGetFileSize(path, &fileSize) != 0) {
            internal_free(path);
            return SFXL_ERR;
        }

        if (fileSize <= 0) {
            internal_free(path);
            return SFXL_ERR;
        }

        entry->fileSize = fileSize;

        switch (_sfxl_compression) {
        case 0:
            entry->dataSize = fileSize;
            break;
        case 1:
            if (1) {
                File* stream = fileOpen(path, "rb");
                if (stream == nullptr) {
                    internal_free(path);
                    return 1;
                }

                int channels;
                int sampleRate;
                int sampleCount;
                SoundDecoder* soundDecoder = soundDecoderInit(soundEffectsListSoundDecoderReadHandler, stream, &channels, &sampleRate, &sampleCount);
                entry->dataSize = 2 * sampleCount;
                soundDecoderFree(soundDecoder);
                fileClose(stream);
            }
            break;
        default:
            internal_free(path);
            return SFXL_ERR;
        }
    }

    internal_free(path);

    return SFXL_OK;
}

// NOTE: Inlined.
//
// 0x4AA200
static int soundEffectsListSort()
{
    if (gSoundEffectsListEntriesLength != 1) {
        qsort(gSoundEffectsListEntries, gSoundEffectsListEntriesLength, sizeof(*gSoundEffectsListEntries), soundEffectsListCompareByName);
    }
    return 0;
}

// 0x4AA228
static int soundEffectsListCompareByName(const void* a1, const void* a2)
{
    SoundEffectsListEntry* v1 = (SoundEffectsListEntry*)a1;
    SoundEffectsListEntry* v2 = (SoundEffectsListEntry*)a2;

    return compat_stricmp(v1->name, v2->name);
}

// read via xfile
static int soundEffectsListSoundDecoderReadHandler(void* data, void* buf, unsigned int size)
{
    return fileRead(buf, 1, size, reinterpret_cast<File*>(data));
}

} // namespace fallout
