#ifndef SOUND_H
#define SOUND_H

#include "memory_defs.h"
#include "win32.h"

namespace fallout {

#define VOLUME_MIN (0)
#define VOLUME_MAX (0x7FFF)

typedef enum SoundError {
    SOUND_NO_ERROR = 0,
    SOUND_SOS_DRIVER_NOT_LOADED = 1,
    SOUND_SOS_INVALID_POINTER = 2,
    SOUND_SOS_DETECT_INITIALIZED = 3,
    SOUND_SOS_FAIL_ON_FILE_OPEN = 4,
    SOUND_SOS_MEMORY_FAIL = 5,
    SOUND_SOS_INVALID_DRIVER_ID = 6,
    SOUND_SOS_NO_DRIVER_FOUND = 7,
    SOUND_SOS_DETECTION_FAILURE = 8,
    SOUND_SOS_DRIVER_LOADED = 9,
    SOUND_SOS_INVALID_HANDLE = 10,
    SOUND_SOS_NO_HANDLES = 11,
    SOUND_SOS_PAUSED = 12,
    SOUND_SOS_NO_PAUSED = 13,
    SOUND_SOS_INVALID_DATA = 14,
    SOUND_SOS_DRV_FILE_FAIL = 15,
    SOUND_SOS_INVALID_PORT = 16,
    SOUND_SOS_INVALID_IRQ = 17,
    SOUND_SOS_INVALID_DMA = 18,
    SOUND_SOS_INVALID_DMA_IRQ = 19,
    SOUND_NO_DEVICE = 20,
    SOUND_NOT_INITIALIZED = 21,
    SOUND_NO_SOUND = 22,
    SOUND_FUNCTION_NOT_SUPPORTED = 23,
    SOUND_NO_BUFFERS_AVAILABLE = 24,
    SOUND_FILE_NOT_FOUND = 25,
    SOUND_ALREADY_PLAYING = 26,
    SOUND_NOT_PLAYING = 27,
    SOUND_ALREADY_PAUSED = 28,
    SOUND_NOT_PAUSED = 29,
    SOUND_INVALID_HANDLE = 30,
    SOUND_NO_MEMORY_AVAILABLE = 31,
    SOUND_UNKNOWN_ERROR = 32,
    SOUND_ERR_COUNT,
} SoundError;

typedef int SoundOpenProc(const char* filePath, int* sampleRate);
typedef int SoundCloseProc(int fileHandle);
typedef int SoundReadProc(int fileHandle, void* buf, unsigned int size);
typedef int SoundWriteProc(int fileHandle, const void* buf, unsigned int size);
typedef long SoundSeekProc(int fileHandle, long offset, int origin);
typedef long SoundTellProc(int fileHandle);
typedef long SoundFileLengthProc(int fileHandle);

typedef struct SoundFileIO {
    SoundOpenProc* open;
    SoundCloseProc* close;
    SoundReadProc* read;
    SoundWriteProc* write;
    SoundSeekProc* seek;
    SoundTellProc* tell;
    SoundFileLengthProc* filelength;
    int fd;
} SoundFileIO;

typedef enum SoundType {
    SOUND_TYPE_MEMORY = 0x01,
    SOUND_TYPE_STREAMING = 0x02,
    SOUND_TYPE_FIRE_AND_FORGET = 0x04,
    SOUND_TYPE_INFINITE = 0x10,
    SOUND_TYPE_0x20 = 0x20,
} SoundType;

typedef enum SoundFlags {
    SOUND_FLAG_0x02 = 0x02,
    SOUND_FLAG_0x04 = 0x04,
    SOUND_16BIT = 0x08,
    SOUND_8BIT = 0x10,
    SOUND_LOOPING = 0x20,
    SOUND_FLAG_0x80 = 0x80,
    SOUND_FLAG_0x100 = 0x100,
    SOUND_FLAG_0x200 = 0x200,
} SoundFlags;

typedef void SoundCallback(void* userData, int a2);
typedef void SoundDeleteCallback(void* userData);

typedef struct Sound {
    SoundFileIO io;
    unsigned char* data;
    int soundBuffer;
    int bitsPerSample;
    int channels;
    int rate;
    int soundFlags;
    int statusFlags;
    int type;
    int pausePos;
    int volume;
    int loops;
    int field_54;
    int field_58;
    int minReadBuffer;
    int fileSize;
    int numBytesRead;
    int field_68;
    int readLimit;
    int lastUpdate;
    unsigned int lastPosition;
    int numBuffers;
    int dataSize;
    void* userData;
    void* callbackUserData;
    SoundCallback* callback;
    void* deleteUserData;
    SoundDeleteCallback* deleteCallback;
    struct Sound* next;
    struct Sound* prev;
} Sound;

void soundSetMemoryProcs(MallocProc* mallocProc, ReallocProc* reallocProc, FreeProc* freeProc);
const char* soundGetErrorDescription(int err);
int soundInit(int a1, int numBuffers, int a3, int dataSize, int rate);
void soundExit();
Sound* soundAllocate(int type, int soundFlags);
int soundLoad(Sound* sound, char* filePath);
int soundPlay(Sound* sound);
int soundStop(Sound* sound);
int soundDelete(Sound* sound);
bool soundIsPlaying(Sound* sound);
bool _soundDone(Sound* sound);
bool soundIsPaused(Sound* sound);
int _soundType(Sound* sound, int type);
int soundGetDuration(Sound* sound);
int soundSetLooping(Sound* sound, int loops);
int _soundVolumeHMItoDirectSound(int a1);
int soundSetVolume(Sound* sound, int volume);
int soundSetCallback(Sound* sound, SoundCallback* callback, void* userData);
int soundSetChannels(Sound* sound, int channels);
int soundSetReadLimit(Sound* sound, int readLimit);
int soundPause(Sound* sound);
int soundResume(Sound* sound);
int soundSetFileIO(Sound* sound, SoundOpenProc* openProc, SoundCloseProc* closeProc, SoundReadProc* readProc, SoundWriteProc* writeProc, SoundSeekProc* seekProc, SoundTellProc* tellProc, SoundFileLengthProc* fileLengthProc);
int _soundSetMasterVolume(int value);
int _soundGetPosition(Sound* sound);
int _soundSetPosition(Sound* sound, int pos);
int _soundFade(Sound* sound, int duration, int targetVolume);
void soundDeleteAll();
void soundContinueAll();
int soundSetDefaultFileIO(SoundOpenProc* openProc, SoundCloseProc* closeProc, SoundReadProc* readProc, SoundWriteProc* writeProc, SoundSeekProc* seekProc, SoundTellProc* tellProc, SoundFileLengthProc* fileLengthProc);

} // namespace fallout

#endif /* SOUND_H */
