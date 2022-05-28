#ifndef SOUND_EFFECTS_CACHE_H
#define SOUND_EFFECTS_CACHE_H

#include "cache.h"

// The maximum number of sound effects that can be loaded and played
// simultaneously.
#define SOUND_EFFECTS_MAX_COUNT (4)

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

extern const char* off_50DE04;
extern bool gSoundEffectsCacheInitialized;
extern int _sfxc_cmpr;
extern Cache* gSoundEffectsCache;
extern int gSoundEffectsCacheDebugLevel;
extern char* gSoundEffectsCacheEffectsPath;
extern SoundEffect* gSoundEffects;
extern int _sfxc_files_open;

int soundEffectsCacheInit(int cache_size, const char* effectsPath);
void soundEffectsCacheExit();
int soundEffectsCacheInitialized();
void soundEffectsCacheFlush();
int soundEffectsCacheFileOpen(const char* fname, int mode, ...);
int soundEffectsCacheFileClose(int handle);
int soundEffectsCacheFileRead(int handle, void* buf, unsigned int size);
int soundEffectsCacheFileWrite(int handle, const void* buf, unsigned int size);
long soundEffectsCacheFileSeek(int handle, long offset, int origin);
long soundEffectsCacheFileTell(int handle);
long soundEffectsCacheFileLength(int handle);
int soundEffectsCacheGetFileSizeImpl(int tag, int* sizePtr);
int soundEffectsCacheReadDataImpl(int tag, int* sizePtr, unsigned char* data);
void soundEffectsCacheFreeImpl(void* ptr);
int soundEffectsCacheCreateHandles();
void soundEffectsCacheFreeHandles();
int soundEffectsCreate(int* handlePtr, int id, void* data, CacheEntry* cacheHandle);
bool soundEffectsIsValidHandle(int a1);
int soundEffectsCacheFileReadCompressed(int handle, void* buf, unsigned int size);
int _sfxc_ad_reader(int handle, void* buf, unsigned int size);

#endif /* SOUND_EFFECTS_CACHE_H */
