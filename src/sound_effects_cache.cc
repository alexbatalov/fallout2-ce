#include "sound_effects_cache.h"

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cache.h"
#include "db.h"
#include "memory.h"
#include "settings.h"
#include "sound_decoder.h"
#include "sound_effects_list.h"

namespace fallout {

#define SOUND_EFFECTS_CACHE_MIN_SIZE (0x40000)

typedef struct SoundEffect {
    // NOTE: This field is only 1 byte, likely unsigned char. It always uses
    // cmp for checking implying it's not bitwise flags. Therefore it's better
    // to express it as boolean.
    bool used;
    CacheEntry* cacheHandle;
    int tag;
    int dataSize;
    int fileSize;
    // TODO: Make size_t.
    int position;
    int dataPosition;
    unsigned char* data;
} SoundEffect;

static int soundEffectsCacheGetFileSizeImpl(int tag, int* sizePtr);
static int soundEffectsCacheReadDataImpl(int tag, int* sizePtr, unsigned char* data);
static void soundEffectsCacheFreeImpl(void* ptr);
static int soundEffectsCacheCreateHandles();
static void soundEffectsCacheFreeHandles();
static int soundEffectsCreate(int* handlePtr, int id, void* data, CacheEntry* cacheHandle);
static bool soundEffectsIsValidHandle(int a1);
static int soundEffectsCacheFileReadCompressed(int handle, void* buf, unsigned int size);
static int soundEffectsCacheSoundDecoderReadHandler(void* data, void* buf, unsigned int size);

// 0x50DE04
static const char* off_50DE04 = "";

// sfxc_initialized
// 0x51C8F0
static bool gSoundEffectsCacheInitialized = false;

// 0x51C8F4
static int _sfxc_cmpr = 1;

// sfxc_pcache
// 0x51C8EC
static Cache* gSoundEffectsCache = nullptr;

// sfxc_dlevel
// 0x51C8DC
static int gSoundEffectsCacheDebugLevel = INT_MAX;

// 0x51C8E0
static char* gSoundEffectsCacheEffectsPath = nullptr;

// 0x51C8E4
static SoundEffect* gSoundEffects = nullptr;

// 0x51C8E8
static int _sfxc_files_open = 0;

// sfxc_init
// 0x4A8FC0
int soundEffectsCacheInit(int cacheSize, const char* effectsPath)
{
    gSoundEffectsCacheDebugLevel = settings.sound.debug_sfxc;

    if (cacheSize <= SOUND_EFFECTS_CACHE_MIN_SIZE) {
        return -1;
    }

    if (effectsPath == nullptr) {
        effectsPath = off_50DE04;
    }

    gSoundEffectsCacheEffectsPath = internal_strdup(effectsPath);
    if (gSoundEffectsCacheEffectsPath == nullptr) {
        return -1;
    }

    if (soundEffectsListInit(gSoundEffectsCacheEffectsPath, _sfxc_cmpr, gSoundEffectsCacheDebugLevel) != SFXL_OK) {
        internal_free(gSoundEffectsCacheEffectsPath);
        return -1;
    }

    if (soundEffectsCacheCreateHandles() != 0) {
        soundEffectsListExit();
        internal_free(gSoundEffectsCacheEffectsPath);
        return -1;
    }

    gSoundEffectsCache = (Cache*)internal_malloc(sizeof(*gSoundEffectsCache));
    if (gSoundEffectsCache == nullptr) {
        soundEffectsCacheFreeHandles();
        soundEffectsListExit();
        internal_free(gSoundEffectsCacheEffectsPath);
        return -1;
    }

    if (!cacheInit(gSoundEffectsCache, soundEffectsCacheGetFileSizeImpl, soundEffectsCacheReadDataImpl, soundEffectsCacheFreeImpl, cacheSize)) {
        internal_free(gSoundEffectsCache);
        soundEffectsCacheFreeHandles();
        soundEffectsListExit();
        internal_free(gSoundEffectsCacheEffectsPath);
        return -1;
    }

    gSoundEffectsCacheInitialized = true;

    return 0;
}

// 0x4A90FC
void soundEffectsCacheExit()
{
    if (gSoundEffectsCacheInitialized) {
        cacheFree(gSoundEffectsCache);
        internal_free(gSoundEffectsCache);
        gSoundEffectsCache = nullptr;

        soundEffectsCacheFreeHandles();

        soundEffectsListExit();

        internal_free(gSoundEffectsCacheEffectsPath);

        gSoundEffectsCacheInitialized = false;
    }
}

// 0x4A9140
int soundEffectsCacheInitialized()
{
    return gSoundEffectsCacheInitialized;
}

// 0x4A9148
void soundEffectsCacheFlush()
{
    if (gSoundEffectsCacheInitialized) {
        cacheFlush(gSoundEffectsCache);
    }
}

// sfxc_cached_open
// 0x4A915C
int soundEffectsCacheFileOpen(const char* fname, int* sampleRate)
{
    if (_sfxc_files_open >= SOUND_EFFECTS_MAX_COUNT) {
        return -1;
    }

    char* copy = internal_strdup(fname);
    if (copy == nullptr) {
        return -1;
    }

    int tag;
    int err = soundEffectsListGetTag(copy, &tag);

    internal_free(copy);

    if (err != SFXL_OK) {
        return -1;
    }

    void* data;
    CacheEntry* cacheHandle;
    if (!cacheLock(gSoundEffectsCache, tag, &data, &cacheHandle)) {
        return -1;
    }

    int handle;
    if (soundEffectsCreate(&handle, tag, data, cacheHandle) != 0) {
        cacheUnlock(gSoundEffectsCache, cacheHandle);
        return -1;
    }

    return handle;
}

// sfxc_cached_close
// 0x4A9220
int soundEffectsCacheFileClose(int handle)
{
    if (!soundEffectsIsValidHandle(handle)) {
        return -1;
    }

    SoundEffect* soundEffect = &(gSoundEffects[handle]);
    if (!cacheUnlock(gSoundEffectsCache, soundEffect->cacheHandle)) {
        return -1;
    }

    // FIXME: This is check is redundant and implemented incorrectly. There is
    // an overflow when handle == SOUND_EFFECTS_MAX_COUNT, but thanks to
    // [soundEffectsIsValidHandle] handle will always be less than
    // [SOUND_EFFECTS_MAX_COUNT].
    if (handle <= SOUND_EFFECTS_MAX_COUNT) {
        soundEffect->used = false;
    }

    return 0;
}

// 0x4A9274
int soundEffectsCacheFileRead(int handle, void* buf, unsigned int size)
{
    if (!soundEffectsIsValidHandle(handle)) {
        return -1;
    }

    if (size == 0) {
        return 0;
    }

    SoundEffect* soundEffect = &(gSoundEffects[handle]);
    if (soundEffect->dataSize - soundEffect->position <= 0) {
        return 0;
    }

    size_t bytesToRead;
    // NOTE: Original code uses signed comparison.
    if ((int)size < (soundEffect->dataSize - soundEffect->position)) {
        bytesToRead = size;
    } else {
        bytesToRead = soundEffect->dataSize - soundEffect->position;
    }

    switch (_sfxc_cmpr) {
    case 0:
        memcpy(buf, soundEffect->data + soundEffect->position, bytesToRead);
        break;
    case 1:
        if (soundEffectsCacheFileReadCompressed(handle, buf, bytesToRead) != 0) {
            return -1;
        }
        break;
    default:
        return -1;
    }

    soundEffect->position += bytesToRead;

    return bytesToRead;
}

// 0x4A9350
int soundEffectsCacheFileWrite(int handle, const void* buf, unsigned int size)
{
    return -1;
}

// 0x4A9358
long soundEffectsCacheFileSeek(int handle, long offset, int origin)
{
    if (!soundEffectsIsValidHandle(handle)) {
        return -1;
    }

    SoundEffect* soundEffect = &(gSoundEffects[handle]);

    int position;
    switch (origin) {
    case SEEK_SET:
        position = 0;
        break;
    case SEEK_CUR:
        position = soundEffect->position;
        break;
    case SEEK_END:
        position = soundEffect->dataSize;
        break;
    default:
        assert(false && "Should be unreachable");
    }

    long normalizedOffset = abs(offset);

    if (offset >= 0) {
        long remainingSize = soundEffect->dataSize - soundEffect->position;
        if (normalizedOffset > remainingSize) {
            normalizedOffset = remainingSize;
        }
        offset = position + normalizedOffset;
    } else {
        if (normalizedOffset > position) {
            return -1;
        }

        offset = position - normalizedOffset;
    }

    soundEffect->position = offset;

    return offset;
}

// 0x4A93F4
long soundEffectsCacheFileTell(int handle)
{
    if (!soundEffectsIsValidHandle(handle)) {
        return -1;
    }

    SoundEffect* soundEffect = &(gSoundEffects[handle]);
    return soundEffect->position;
}

// 0x4A9418
long soundEffectsCacheFileLength(int handle)
{
    if (!soundEffectsIsValidHandle(handle)) {
        return 0;
    }

    SoundEffect* soundEffect = &(gSoundEffects[handle]);
    return soundEffect->dataSize;
}

// sfxc_cached_file_size
// 0x4A9434
static int soundEffectsCacheGetFileSizeImpl(int tag, int* sizePtr)
{
    int size;
    if (soundEffectsListGetFileSize(tag, &size) == -1) {
        return -1;
    }

    *sizePtr = size;

    return 0;
}

// 0x4A945C
static int soundEffectsCacheReadDataImpl(int tag, int* sizePtr, unsigned char* data)
{
    if (!soundEffectsListIsValidTag(tag)) {
        return -1;
    }

    int size;
    soundEffectsListGetFileSize(tag, &size);

    char* name;
    soundEffectsListGetFilePath(tag, &name);

    if (dbGetFileContents(name, data)) {
        internal_free(name);
        return -1;
    }

    internal_free(name);

    *sizePtr = size;

    return 0;
}

// 0x4A94CC
static void soundEffectsCacheFreeImpl(void* ptr)
{
    internal_free(ptr);
}

// 0x4A94D4
static int soundEffectsCacheCreateHandles()
{
    gSoundEffects = (SoundEffect*)internal_malloc(sizeof(*gSoundEffects) * SOUND_EFFECTS_MAX_COUNT);
    if (gSoundEffects == nullptr) {
        return -1;
    }

    for (int index = 0; index < SOUND_EFFECTS_MAX_COUNT; index++) {
        SoundEffect* soundEffect = &(gSoundEffects[index]);
        soundEffect->used = false;
    }

    _sfxc_files_open = 0;

    return 0;
}

// 0x4A9518
static void soundEffectsCacheFreeHandles()
{
    if (_sfxc_files_open) {
        for (int index = 0; index < SOUND_EFFECTS_MAX_COUNT; index++) {
            SoundEffect* soundEffect = &(gSoundEffects[index]);
            if (!soundEffect->used) {
                soundEffectsCacheFileClose(index);
            }
        }
    }

    internal_free(gSoundEffects);
}

// 0x4A9550
static int soundEffectsCreate(int* handlePtr, int tag, void* data, CacheEntry* cacheHandle)
{
    if (_sfxc_files_open >= SOUND_EFFECTS_MAX_COUNT) {
        return -1;
    }

    SoundEffect* soundEffect;
    int index;
    for (index = 0; index < SOUND_EFFECTS_MAX_COUNT; index++) {
        soundEffect = &(gSoundEffects[index]);
        if (!soundEffect->used) {
            break;
        }
    }

    if (index == SOUND_EFFECTS_MAX_COUNT) {
        return -1;
    }

    soundEffect->used = true;
    soundEffect->cacheHandle = cacheHandle;
    soundEffect->tag = tag;

    soundEffectsListGetDataSize(tag, &(soundEffect->dataSize));
    soundEffectsListGetFileSize(tag, &(soundEffect->fileSize));

    soundEffect->position = 0;
    soundEffect->dataPosition = 0;

    soundEffect->data = (unsigned char*)data;

    *handlePtr = index;

    return 0;
}

// 0x4A961C
static bool soundEffectsIsValidHandle(int handle)
{
    if (handle >= SOUND_EFFECTS_MAX_COUNT) {
        return false;
    }

    SoundEffect* soundEffect = &gSoundEffects[handle];

    if (!soundEffect->used) {
        return false;
    }

    if (soundEffect->dataSize < soundEffect->position) {
        return false;
    }

    return soundEffectsListIsValidTag(soundEffect->tag);
}

// 0x4A967C
static int soundEffectsCacheFileReadCompressed(int handle, void* buf, unsigned int size)
{
    if (!soundEffectsIsValidHandle(handle)) {
        return -1;
    }

    SoundEffect* soundEffect = &(gSoundEffects[handle]);
    soundEffect->dataPosition = 0;

    int channels;
    int sampleRate;
    int sampleCount;
    SoundDecoder* soundDecoder = soundDecoderInit(soundEffectsCacheSoundDecoderReadHandler, &handle, &channels, &sampleRate, &sampleCount);

    if (soundEffect->position != 0) {
        void* temp = internal_malloc(soundEffect->position);
        if (temp == nullptr) {
            soundDecoderFree(soundDecoder);
            return -1;
        }

        size_t bytesRead = soundDecoderDecode(soundDecoder, temp, soundEffect->position);
        internal_free(temp);

        if (bytesRead != soundEffect->position) {
            soundDecoderFree(soundDecoder);
            return -1;
        }
    }

    size_t bytesRead = soundDecoderDecode(soundDecoder, buf, size);
    soundDecoderFree(soundDecoder);

    if (bytesRead != size) {
        return -1;
    }

    return 0;
}

// 0x4A9774
static int soundEffectsCacheSoundDecoderReadHandler(void* data, void* buf, unsigned int size)
{
    if (size == 0) {
        return 0;
    }

    int handle = *reinterpret_cast<int*>(data);
    SoundEffect* soundEffect = &(gSoundEffects[handle]);

    unsigned int bytesToRead = soundEffect->fileSize - soundEffect->dataPosition;
    if (size <= bytesToRead) {
        bytesToRead = size;
    }

    memcpy(buf, soundEffect->data + soundEffect->dataPosition, bytesToRead);

    soundEffect->dataPosition += bytesToRead;

    return bytesToRead;
}

} // namespace fallout
