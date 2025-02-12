#include "sound.h"

#include <fcntl.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#include <algorithm>

#include <SDL.h>

#include "audio_engine.h"
#include "debug.h"
#include "platform_compat.h"

namespace fallout {

typedef enum SoundStatusFlags {
    SOUND_STATUS_DONE = 0x01,
    SOUND_STATUS_IS_PLAYING = 0x02,
    SOUND_STATUS_IS_FADING = 0x04,
    SOUND_STATUS_IS_PAUSED = 0x08,
} SoundStatusFlags;

typedef char*(SoundFileNameMangler)(char*);

typedef struct FadeSound {
    Sound* sound;
    int deltaVolume;
    int targetVolume;
    int initialVolume;
    int currentVolume;
    int pause;
    struct FadeSound* prev;
    struct FadeSound* next;
} FadeSound;

static void* soundMallocProcDefaultImpl(size_t size);
static void* soundReallocProcDefaultImpl(void* ptr, size_t size);
static void soundFreeProcDefaultImpl(void* ptr);
static long soundFileSize(int fileHandle);
static long soundTellData(int fileHandle);
static int soundWriteData(int fileHandle, const void* buf, unsigned int size);
static int soundReadData(int fileHandle, void* buf, unsigned int size);
static int soundOpenData(const char* filePath, int* sampleRate);
static long soundSeekData(int fileHandle, long offset, int origin);
static int soundCloseData(int fileHandle);
static char* soundFileManglerDefaultImpl(char* fname);
static void _refreshSoundBuffers(Sound* sound);
static int _preloadBuffers(Sound* sound);
static int _soundRewind(Sound* sound);
static int _addSoundData(Sound* sound, unsigned char* buf, int size);
static int _soundSetData(Sound* sound, unsigned char* buf, int size);
static int soundContinue(Sound* sound);
static int _soundGetVolume(Sound* sound);
static void soundDeleteInternal(Sound* sound);
static Uint32 _doTimerEvent(Uint32 interval, void* param);
static void _removeTimedEvent(SDL_TimerID* timerId);
static void _removeFadeSound(FadeSound* fadeSound);
static void _fadeSounds();
static int _internalSoundFade(Sound* sound, int duration, int targetVolume, bool pause);

// 0x51D478
static FadeSound* _fadeHead = nullptr;

// 0x51D47C
static FadeSound* _fadeFreeList = nullptr;

// 0x51D488
static MallocProc* gSoundMallocProc = soundMallocProcDefaultImpl;

// 0x51D48C
static ReallocProc* gSoundReallocProc = soundReallocProcDefaultImpl;

// 0x51D490
static FreeProc* gSoundFreeProc = soundFreeProcDefaultImpl;

// 0x51D494
static SoundFileIO gSoundDefaultFileIO = {
    soundOpenData,
    soundCloseData,
    soundReadData,
    soundWriteData,
    soundSeekData,
    soundTellData,
    soundFileSize,
    -1,
};

// 0x51D4B4
static SoundFileNameMangler* gSoundFileNameMangler = soundFileManglerDefaultImpl;

// 0x51D4B8
static const char* gSoundErrorDescriptions[SOUND_ERR_COUNT] = {
    "sound.c: No error",
    "sound.c: SOS driver not loaded",
    "sound.c: SOS invalid pointer",
    "sound.c: SOS detect initialized",
    "sound.c: SOS fail on file open",
    "sound.c: SOS memory fail",
    "sound.c: SOS invalid driver ID",
    "sound.c: SOS no driver found",
    "sound.c: SOS detection failure",
    "sound.c: SOS driver loaded",
    "sound.c: SOS invalid handle",
    "sound.c: SOS no handles",
    "sound.c: SOS paused",
    "sound.c: SOS not paused",
    "sound.c: SOS invalid data",
    "sound.c: SOS drv file fail",
    "sound.c: SOS invalid port",
    "sound.c: SOS invalid IRQ",
    "sound.c: SOS invalid DMA",
    "sound.c: SOS invalid DMA IRQ",
    "sound.c: no device",
    "sound.c: not initialized",
    "sound.c: no sound",
    "sound.c: function not supported",
    "sound.c: no buffers available",
    "sound.c: file not found",
    "sound.c: already playing",
    "sound.c: not playing",
    "sound.c: already paused",
    "sound.c: not paused",
    "sound.c: invalid handle",
    "sound.c: no memory available",
    "sound.c: unknown error",
};

// 0x668150
static int gSoundLastError;

// 0x668154
static int _masterVol;

// 0x66815C
static int _sampleRate;

// Number of sounds currently playing.
//
// 0x668160
static int _numSounds;

// 0x668164
static int _deviceInit;

// 0x668168
static int _dataSize;

// 0x66816C
static int _numBuffers;

// 0x668170
static bool gSoundInitialized;

// 0x668174
static Sound* gSoundListHead;

static SDL_TimerID gFadeSoundsTimerId = 0;

// 0x4AC6F0
void* soundMallocProcDefaultImpl(size_t size)
{
    return malloc(size);
}

// 0x4AC6F8
void* soundReallocProcDefaultImpl(void* ptr, size_t size)
{
    return realloc(ptr, size);
}

// 0x4AC700
void soundFreeProcDefaultImpl(void* ptr)
{
    free(ptr);
}

// 0x4AC708
void soundSetMemoryProcs(MallocProc* mallocProc, ReallocProc* reallocProc, FreeProc* freeProc)
{
    gSoundMallocProc = mallocProc;
    gSoundReallocProc = reallocProc;
    gSoundFreeProc = freeProc;
}

// 0x4AC71C
static long soundFileSize(int fileHandle)
{
    long pos;
    long size;

    pos = compat_tell(fileHandle);
    size = lseek(fileHandle, 0, SEEK_END);
    lseek(fileHandle, pos, SEEK_SET);

    return size;
}

// 0x4AC750
static long soundTellData(int fileHandle)
{
    return compat_tell(fileHandle);
}

// 0x4AC758
static int soundWriteData(int fileHandle, const void* buf, unsigned int size)
{
    return write(fileHandle, buf, size);
}

// 0x4AC760
static int soundReadData(int fileHandle, void* buf, unsigned int size)
{
    return read(fileHandle, buf, size);
}

// 0x4AC768
static int soundOpenData(const char* filePath, int* sampleRate)
{
    int flags;

#ifdef _WIN32
    flags = _O_RDONLY | _O_BINARY;
#else
    flags = O_RDONLY;
#endif

    return open(filePath, flags);
}

// 0x4AC774
static long soundSeekData(int fileHandle, long offset, int origin)
{
    return lseek(fileHandle, offset, origin);
}

// 0x4AC77C
static int soundCloseData(int fileHandle)
{
    return close(fileHandle);
}

// 0x4AC78C
char* soundFileManglerDefaultImpl(char* fname)
{
    return fname;
}

// 0x4AC790
const char* soundGetErrorDescription(int err)
{
    if (err == -1) {
        err = gSoundLastError;
    }

    if (err < 0 || err > SOUND_UNKNOWN_ERROR) {
        err = SOUND_UNKNOWN_ERROR;
    }

    return gSoundErrorDescriptions[err];
}

// 0x4AC7B0
void _refreshSoundBuffers(Sound* sound)
{
    if ((sound->soundFlags & SOUND_FLAG_0x80) != 0) {
        return;
    }

    unsigned int readPos;
    unsigned int writePos;
    bool hr = audioEngineSoundBufferGetCurrentPosition(sound->soundBuffer, &readPos, &writePos);
    if (!hr) {
        return;
    }

    if (readPos < sound->lastPosition) {
        sound->numBytesRead += readPos + sound->numBuffers * sound->dataSize - sound->lastPosition;
    } else {
        sound->numBytesRead += readPos - sound->lastPosition;
    }

    if ((sound->soundFlags & SOUND_FLAG_0x100) != 0) {
        if ((sound->type & SOUND_TYPE_0x20) != 0) {
            if ((sound->soundFlags & SOUND_FLAG_0x200) != 0) {
                sound->soundFlags |= SOUND_FLAG_0x80;
            }
        } else {
            if (sound->fileSize <= sound->numBytesRead) {
                sound->soundFlags |= SOUND_FLAG_0x200 | SOUND_FLAG_0x80;
            }
        }
    }
    sound->lastPosition = readPos;

    if (sound->fileSize < sound->numBytesRead) {
        int v3;
        do {
            v3 = sound->numBytesRead - sound->fileSize;
            sound->numBytesRead = v3;
        } while (v3 > sound->fileSize);
    }

    int v6 = readPos / sound->dataSize;
    if (sound->lastUpdate == v6) {
        return;
    }

    int v53;
    if (sound->lastUpdate > v6) {
        v53 = v6 + sound->numBuffers - sound->lastUpdate;
    } else {
        v53 = v6 - sound->lastUpdate;
    }

    if (sound->dataSize * v53 >= sound->readLimit) {
        v53 = (sound->readLimit + sound->dataSize - 1) / sound->dataSize;
    }

    if (v53 < sound->minReadBuffer) {
        return;
    }

    void* audioPtr1;
    void* audioPtr2;
    unsigned int audioBytes1;
    unsigned int audioBytes2;
    hr = audioEngineSoundBufferLock(sound->soundBuffer, sound->dataSize * sound->lastUpdate, sound->dataSize * v53, &audioPtr1, &audioBytes1, &audioPtr2, &audioBytes2, 0);
    if (!hr) {
        return;
    }

    if (audioBytes1 + audioBytes2 != sound->dataSize * v53) {
        debugPrint("locked memory region not big enough, wanted %d (%d * %d), got %d (%d + %d)\n", sound->dataSize * v53, v53, sound->dataSize, audioBytes1 + audioBytes2, audioBytes1, audioBytes2);
        debugPrint("Resetting readBuffers from %d to %d\n", v53, (audioBytes1 + audioBytes2) / sound->dataSize);

        v53 = (audioBytes1 + audioBytes2) / sound->dataSize;
        if (v53 < sound->minReadBuffer) {
            debugPrint("No longer above read buffer size, returning\n");
            return;
        }
    }
    unsigned char* audioPtr = (unsigned char*)audioPtr1;
    int audioBytes = audioBytes1;
    while (--v53 != -1) {
        int bytesRead;
        if ((sound->soundFlags & SOUND_FLAG_0x200) != 0) {
            bytesRead = sound->dataSize;
            memset(sound->data, 0, bytesRead);
        } else {
            int bytesToRead = sound->dataSize;
            if (sound->field_58 != -1) {
                int pos = sound->io.tell(sound->io.fd);
                if (bytesToRead + pos > sound->field_58) {
                    bytesToRead = sound->field_58 - pos;
                }
            }

            bytesRead = sound->io.read(sound->io.fd, sound->data, bytesToRead);
            if (bytesRead < sound->dataSize) {
                if ((sound->soundFlags & SOUND_LOOPING) == 0 || (sound->soundFlags & SOUND_FLAG_0x100) != 0) {
                    memset(sound->data + bytesRead, 0, sound->dataSize - bytesRead);
                    sound->soundFlags |= SOUND_FLAG_0x200;
                    bytesRead = sound->dataSize;
                } else {
                    while (bytesRead < sound->dataSize) {
                        if (sound->loops == -1) {
                            sound->io.seek(sound->io.fd, sound->field_54, SEEK_SET);
                            if (sound->callback != nullptr) {
                                sound->callback(sound->callbackUserData, 0x0400);
                            }
                        } else {
                            if (sound->loops <= 0) {
                                sound->field_58 = -1;
                                sound->field_54 = 0;
                                sound->loops = 0;
                                sound->soundFlags &= ~SOUND_LOOPING;
                                bytesRead += sound->io.read(sound->io.fd, sound->data + bytesRead, sound->dataSize - bytesRead);
                                break;
                            }

                            sound->loops--;
                            sound->io.seek(sound->io.fd, sound->field_54, SEEK_SET);

                            if (sound->callback != nullptr) {
                                sound->callback(sound->callbackUserData, 0x400);
                            }
                        }

                        if (sound->field_58 == -1) {
                            bytesToRead = sound->dataSize - bytesRead;
                        } else {
                            int pos = sound->io.tell(sound->io.fd);
                            if (sound->dataSize + bytesRead + pos <= sound->field_58) {
                                bytesToRead = sound->dataSize - bytesRead;
                            } else {
                                bytesToRead = sound->field_58 - bytesRead - pos;
                            }
                        }

                        int v20 = sound->io.read(sound->io.fd, sound->data + bytesRead, bytesToRead);
                        bytesRead += v20;
                        if (v20 < bytesToRead) {
                            break;
                        }
                    }
                }
            }
        }

        if (bytesRead > audioBytes) {
            if (audioBytes != 0) {
                memcpy(audioPtr, sound->data, audioBytes);
            }

            if (audioPtr2 != nullptr) {
                memcpy(audioPtr2, sound->data + audioBytes, bytesRead - audioBytes);
                audioPtr = (unsigned char*)audioPtr2 + bytesRead - audioBytes;
                audioBytes = audioBytes2 - bytesRead;
            } else {
                debugPrint("Hm, no second write pointer, but buffer not big enough, this shouldn't happen\n");
            }
        } else {
            memcpy(audioPtr, sound->data, bytesRead);
            audioPtr += bytesRead;
            audioBytes -= bytesRead;
        }
    }

    audioEngineSoundBufferUnlock(sound->soundBuffer, audioPtr1, audioBytes1, audioPtr2, audioBytes2);

    sound->lastUpdate = v6;
}

// 0x4ACC58
int soundInit(int a1, int numBuffers, int a3, int dataSize, int rate)
{
    if (!audioEngineInit()) {
        debugPrint("soundInit: Unable to init audio engine\n");

        gSoundLastError = SOUND_SOS_DETECTION_FAILURE;
        return gSoundLastError;
    }

    _sampleRate = rate;
    _dataSize = dataSize;
    _numBuffers = numBuffers;
    gSoundInitialized = true;
    _deviceInit = 1;

    _soundSetMasterVolume(VOLUME_MAX);

    gSoundLastError = SOUND_NO_ERROR;
    return gSoundLastError;
}

// 0x4AD04C
void soundExit()
{
    while (gSoundListHead != nullptr) {
        Sound* next = gSoundListHead->next;
        soundDelete(gSoundListHead);
        gSoundListHead = next;
    }

    if (gFadeSoundsTimerId != 0) {
        _removeTimedEvent(&gFadeSoundsTimerId);
    }

    while (_fadeFreeList != nullptr) {
        FadeSound* next = _fadeFreeList->next;
        gSoundFreeProc(_fadeFreeList);
        _fadeFreeList = next;
    }

    audioEngineExit();

    gSoundLastError = SOUND_NO_ERROR;
    gSoundInitialized = false;
}

// 0x4AD0FC
Sound* soundAllocate(int type, int soundFlags)
{
    if (!gSoundInitialized) {
        gSoundLastError = SOUND_NOT_INITIALIZED;
        return nullptr;
    }

    Sound* sound = (Sound*)gSoundMallocProc(sizeof(*sound));
    memset(sound, 0, sizeof(*sound));

    memcpy(&(sound->io), &gSoundDefaultFileIO, sizeof(gSoundDefaultFileIO));

    if ((soundFlags & SOUND_FLAG_0x02) == 0) {
        soundFlags |= SOUND_FLAG_0x02;
    }

    sound->bitsPerSample = (soundFlags & SOUND_16BIT) != 0 ? 16 : 8;
    sound->channels = 1;
    sound->rate = _sampleRate;

    sound->soundFlags = soundFlags;
    sound->type = type;
    sound->dataSize = _dataSize;
    sound->numBytesRead = 0;
    sound->soundBuffer = -1;
    sound->statusFlags = 0;
    sound->numBuffers = _numBuffers;
    sound->readLimit = sound->dataSize * _numBuffers;

    if ((type & SOUND_TYPE_INFINITE) != 0) {
        sound->loops = -1;
        sound->soundFlags |= SOUND_LOOPING;
    }

    sound->field_58 = -1;
    sound->minReadBuffer = 1;
    sound->volume = VOLUME_MAX;
    sound->prev = nullptr;
    sound->field_54 = 0;
    sound->next = gSoundListHead;

    if (gSoundListHead != nullptr) {
        gSoundListHead->prev = sound;
    }

    gSoundListHead = sound;

    return sound;
}

// 0x4AD308
int _preloadBuffers(Sound* sound)
{
    unsigned char* buf;
    int bytes_read;
    int result;
    int v15;
    unsigned char* v14;
    int size;

    size = sound->io.filelength(sound->io.fd);
    sound->fileSize = size;

    if ((sound->type & SOUND_TYPE_STREAMING) != 0) {
        if ((sound->soundFlags & SOUND_LOOPING) == 0) {
            sound->soundFlags |= SOUND_FLAG_0x100 | SOUND_LOOPING;
        }

        if (sound->numBuffers * sound->dataSize >= size) {
            if (size / sound->dataSize * sound->dataSize != size) {
                size = (size / sound->dataSize + 1) * sound->dataSize;
            }
        } else {
            size = sound->numBuffers * sound->dataSize;
        }
    } else {
        sound->type &= ~(SOUND_TYPE_MEMORY | SOUND_TYPE_STREAMING);
        sound->type |= SOUND_TYPE_MEMORY;
    }

    buf = (unsigned char*)gSoundMallocProc(size);
    bytes_read = sound->io.read(sound->io.fd, buf, size);
    if (bytes_read != size) {
        if ((sound->soundFlags & SOUND_LOOPING) == 0 || (sound->soundFlags & SOUND_FLAG_0x100) != 0) {
            memset(buf + bytes_read, 0, size - bytes_read);
        } else {
            v14 = buf + bytes_read;
            v15 = bytes_read;
            while (size - v15 > bytes_read) {
                memcpy(v14, buf, bytes_read);
                v15 += bytes_read;
                v14 += bytes_read;
            }

            if (v15 < size) {
                memcpy(v14, buf, size - v15);
            }
        }
    }

    result = _soundSetData(sound, buf, size);
    gSoundFreeProc(buf);

    if ((sound->type & SOUND_TYPE_MEMORY) != 0) {
        sound->io.close(sound->io.fd);
        sound->io.fd = -1;
    } else {
        if (sound->data == nullptr) {
            sound->data = (unsigned char*)gSoundMallocProc(sound->dataSize);
        }
    }

    return result;
}

// 0x4AD498
int soundLoad(Sound* sound, char* filePath)
{
    if (!gSoundInitialized) {
        gSoundLastError = SOUND_NOT_INITIALIZED;
        return gSoundLastError;
    }

    if (sound == nullptr) {
        gSoundLastError = SOUND_NO_SOUND;
        return gSoundLastError;
    }

    sound->io.fd = sound->io.open(gSoundFileNameMangler(filePath), &(sound->rate));
    if (sound->io.fd == -1) {
        gSoundLastError = SOUND_FILE_NOT_FOUND;
        return gSoundLastError;
    }

    return _preloadBuffers(sound);
}

// 0x4AD504
int _soundRewind(Sound* sound)
{
    bool hr;

    if (!gSoundInitialized) {
        gSoundLastError = SOUND_NOT_INITIALIZED;
        return gSoundLastError;
    }

    if (sound == nullptr || sound->soundBuffer == -1) {
        gSoundLastError = SOUND_NO_SOUND;
        return gSoundLastError;
    }

    if ((sound->type & SOUND_TYPE_STREAMING) != 0) {
        sound->io.seek(sound->io.fd, 0, SEEK_SET);
        sound->lastUpdate = 0;
        sound->lastPosition = 0;
        sound->numBytesRead = 0;
        sound->soundFlags &= ~(SOUND_FLAG_0x200 | SOUND_FLAG_0x80);
        hr = audioEngineSoundBufferSetCurrentPosition(sound->soundBuffer, 0);
        _preloadBuffers(sound);
    } else {
        hr = audioEngineSoundBufferSetCurrentPosition(sound->soundBuffer, 0);
    }

    if (!hr) {
        gSoundLastError = SOUND_UNKNOWN_ERROR;
        return gSoundLastError;
    }

    sound->statusFlags &= ~SOUND_STATUS_DONE;

    gSoundLastError = SOUND_NO_ERROR;
    return gSoundLastError;
}

// 0x4AD5C8
int _addSoundData(Sound* sound, unsigned char* buf, int size)
{
    bool hr;
    void* audioPtr1;
    unsigned int audioBytes1;
    void* audioPtr2;
    unsigned int audioBytes2;

    hr = audioEngineSoundBufferLock(sound->soundBuffer, 0, size, &audioPtr1, &audioBytes1, &audioPtr2, &audioBytes2, AUDIO_ENGINE_SOUND_BUFFER_LOCK_FROM_WRITE_POS);
    if (!hr) {
        gSoundLastError = SOUND_UNKNOWN_ERROR;
        return gSoundLastError;
    }

    memcpy(audioPtr1, buf, audioBytes1);

    if (audioPtr2 != nullptr) {
        memcpy(audioPtr2, buf + audioBytes1, audioBytes2);
    }

    hr = audioEngineSoundBufferUnlock(sound->soundBuffer, audioPtr1, audioBytes1, audioPtr2, audioBytes2);
    if (!hr) {
        gSoundLastError = SOUND_UNKNOWN_ERROR;
        return gSoundLastError;
    }

    gSoundLastError = SOUND_NO_ERROR;
    return gSoundLastError;
}

// 0x4AD6C0
int _soundSetData(Sound* sound, unsigned char* buf, int size)
{
    if (!gSoundInitialized) {
        gSoundLastError = SOUND_NOT_INITIALIZED;
        return gSoundLastError;
    }

    if (sound == nullptr) {
        gSoundLastError = SOUND_NO_SOUND;
        return gSoundLastError;
    }

    if (sound->soundBuffer == -1) {
        sound->soundBuffer = audioEngineCreateSoundBuffer(size, sound->bitsPerSample, sound->channels, sound->rate);
        if (sound->soundBuffer == -1) {
            gSoundLastError = SOUND_UNKNOWN_ERROR;
            return gSoundLastError;
        }
    }

    return _addSoundData(sound, buf, size);
}

// 0x4AD73C
int soundPlay(Sound* sound)
{
    bool hr;
    unsigned int readPos;
    unsigned int writePos;

    if (!gSoundInitialized) {
        gSoundLastError = SOUND_NOT_INITIALIZED;
        return gSoundLastError;
    }

    if (sound == nullptr || sound->soundBuffer == -1) {
        gSoundLastError = SOUND_NO_SOUND;
        return gSoundLastError;
    }

    if ((sound->statusFlags & SOUND_STATUS_DONE) != 0) {
        _soundRewind(sound);
    }

    soundSetVolume(sound, sound->volume);

    hr = audioEngineSoundBufferPlay(sound->soundBuffer, sound->soundFlags & SOUND_LOOPING ? AUDIO_ENGINE_SOUND_BUFFER_PLAY_LOOPING : 0);

    audioEngineSoundBufferGetCurrentPosition(sound->soundBuffer, &readPos, &writePos);
    sound->lastUpdate = readPos / sound->dataSize;

    if (!hr) {
        gSoundLastError = SOUND_UNKNOWN_ERROR;
        return gSoundLastError;
    }

    sound->statusFlags |= SOUND_STATUS_IS_PLAYING;

    ++_numSounds;

    gSoundLastError = SOUND_NO_ERROR;
    return gSoundLastError;
}

// 0x4AD828
int soundStop(Sound* sound)
{
    bool hr;

    if (!gSoundInitialized) {
        gSoundLastError = SOUND_NOT_INITIALIZED;
        return gSoundLastError;
    }

    if (sound == nullptr || sound->soundBuffer == -1) {
        gSoundLastError = SOUND_NO_SOUND;
        return gSoundLastError;
    }

    if ((sound->statusFlags & SOUND_STATUS_IS_PLAYING) == 0) {
        gSoundLastError = SOUND_NOT_PLAYING;
        return gSoundLastError;
    }

    hr = audioEngineSoundBufferStop(sound->soundBuffer);
    if (!hr) {
        gSoundLastError = SOUND_UNKNOWN_ERROR;
        return gSoundLastError;
    }

    sound->statusFlags &= ~SOUND_STATUS_IS_PLAYING;
    _numSounds--;

    gSoundLastError = SOUND_NO_ERROR;
    return gSoundLastError;
}

// 0x4AD8DC
int soundDelete(Sound* sample)
{
    if (!gSoundInitialized) {
        gSoundLastError = SOUND_NOT_INITIALIZED;
        return gSoundLastError;
    }

    if (sample == nullptr) {
        gSoundLastError = SOUND_NO_SOUND;
        return gSoundLastError;
    }

    if (sample->io.fd != -1) {
        sample->io.close(sample->io.fd);
        sample->io.fd = -1;
    }

    soundDeleteInternal(sample);

    gSoundLastError = SOUND_NO_ERROR;
    return gSoundLastError;
}

// 0x4AD948
int soundContinue(Sound* sound)
{
    bool hr;
    unsigned int status;

    if (!gSoundInitialized) {
        gSoundLastError = SOUND_NOT_INITIALIZED;
        return gSoundLastError;
    }

    if (sound == nullptr) {
        gSoundLastError = SOUND_NO_SOUND;
        return gSoundLastError;
    }

    if (sound->soundBuffer == -1) {
        gSoundLastError = SOUND_NO_SOUND;
        return gSoundLastError;
    }

    if ((sound->statusFlags & SOUND_STATUS_IS_PLAYING) == 0) {
        gSoundLastError = SOUND_NOT_PLAYING;
        return gSoundLastError;
    }

    if ((sound->statusFlags & SOUND_STATUS_IS_PAUSED) != 0) {
        gSoundLastError = SOUND_NOT_PLAYING;
        return gSoundLastError;
    }

    if ((sound->statusFlags & SOUND_STATUS_DONE) != 0) {
        gSoundLastError = SOUND_UNKNOWN_ERROR;
        return gSoundLastError;
    }

    hr = audioEngineSoundBufferGetStatus(sound->soundBuffer, &status);
    if (!hr) {
        debugPrint("Error in soundContinue, %x\n", hr);

        gSoundLastError = SOUND_UNKNOWN_ERROR;
        return gSoundLastError;
    }

    if ((sound->soundFlags & SOUND_FLAG_0x80) == 0 && (status & (AUDIO_ENGINE_SOUND_BUFFER_STATUS_PLAYING | AUDIO_ENGINE_SOUND_BUFFER_STATUS_LOOPING)) != 0) {
        if ((sound->statusFlags & SOUND_STATUS_IS_PAUSED) == 0 && (sound->type & SOUND_TYPE_STREAMING) != 0) {
            _refreshSoundBuffers(sound);
        }
    } else if ((sound->statusFlags & SOUND_STATUS_IS_PAUSED) == 0) {
        if (sound->callback != nullptr) {
            sound->callback(sound->callbackUserData, 1);
            sound->callback = nullptr;
        }

        if ((sound->type & SOUND_TYPE_FIRE_AND_FORGET) != 0) {
            sound->callback = nullptr;
            soundDelete(sound);
        } else {
            sound->statusFlags |= SOUND_STATUS_DONE;

            if ((sound->statusFlags & SOUND_STATUS_IS_PLAYING) != 0) {
                --_numSounds;
            }

            soundStop(sound);

            sound->statusFlags &= ~(SOUND_STATUS_DONE | SOUND_STATUS_IS_PLAYING);
        }
    }

    gSoundLastError = SOUND_NO_ERROR;
    return gSoundLastError;
}

// 0x4ADA84
bool soundIsPlaying(Sound* sound)
{
    if (!gSoundInitialized) {
        gSoundLastError = SOUND_NOT_INITIALIZED;
        return false;
    }

    if (sound == nullptr || sound->soundBuffer == -1) {
        gSoundLastError = SOUND_NO_SOUND;
        return false;
    }

    return (sound->statusFlags & SOUND_STATUS_IS_PLAYING) != 0;
}

// 0x4ADAC4
bool _soundDone(Sound* sound)
{
    if (!gSoundInitialized) {
        gSoundLastError = SOUND_NOT_INITIALIZED;
        return false;
    }

    if (sound == nullptr || sound->soundBuffer == -1) {
        gSoundLastError = SOUND_NO_SOUND;
        return false;
    }

    return (sound->statusFlags & SOUND_STATUS_DONE) != 0;
}

// 0x4ADB44
bool soundIsPaused(Sound* sound)
{
    if (!gSoundInitialized) {
        gSoundLastError = SOUND_NOT_INITIALIZED;
        return false;
    }

    if (sound == nullptr || sound->soundBuffer == -1) {
        gSoundLastError = SOUND_NO_SOUND;
        return false;
    }

    return (sound->statusFlags & SOUND_STATUS_IS_PAUSED) != 0;
}

// 0x4ADBC4
int _soundType(Sound* sound, int type)
{
    if (!gSoundInitialized) {
        gSoundLastError = SOUND_NOT_INITIALIZED;
        return 0;
    }

    if (sound == nullptr || sound->soundBuffer == -1) {
        gSoundLastError = SOUND_NO_SOUND;
        return 0;
    }

    return sound->type & type;
}

// 0x4ADC04
int soundGetDuration(Sound* sound)
{
    if (!gSoundInitialized) {
        gSoundLastError = SOUND_NOT_INITIALIZED;
        return gSoundLastError;
    }

    if (sound == nullptr || sound->soundBuffer == -1) {
        gSoundLastError = SOUND_NO_SOUND;
        return gSoundLastError;
    }

    int bytesPerSec = sound->bitsPerSample / 8 * sound->rate;
    int v3 = sound->fileSize;
    int v4 = v3 % bytesPerSec;
    int result = v3 / bytesPerSec;
    if (v4 != 0) {
        result += 1;
    }

    return result;
}

// 0x4ADD00
int soundSetLooping(Sound* sound, int loops)
{
    if (!gSoundInitialized) {
        gSoundLastError = SOUND_NOT_INITIALIZED;
        return gSoundLastError;
    }

    if (sound == nullptr) {
        gSoundLastError = SOUND_NO_SOUND;
        return gSoundLastError;
    }

    if (loops != 0) {
        sound->soundFlags |= SOUND_LOOPING;
        sound->loops = loops;
    } else {
        sound->loops = 0;
        sound->field_58 = -1;
        sound->field_54 = 0;
        sound->soundFlags &= ~SOUND_LOOPING;
    }

    gSoundLastError = SOUND_NO_ERROR;
    return gSoundLastError;
}

// 0x4ADD68
int _soundVolumeHMItoDirectSound(int volume)
{
    double normalizedVolume;

    if (volume > VOLUME_MAX) {
        volume = VOLUME_MAX;
    }

    // Normalize volume to SDL (0-128).
    normalizedVolume = (double)(volume - VOLUME_MIN) / (double)(VOLUME_MAX - VOLUME_MIN) * 128;

    return (int)normalizedVolume;
}

// 0x4ADE0C
int soundSetVolume(Sound* sound, int volume)
{
    int normalizedVolume;
    bool hr;

    if (!gSoundInitialized) {
        gSoundLastError = SOUND_NOT_INITIALIZED;
        return gSoundLastError;
    }

    if (sound == nullptr) {
        gSoundLastError = SOUND_NO_SOUND;
        return gSoundLastError;
    }

    sound->volume = volume;

    if (sound->soundBuffer == -1) {
        gSoundLastError = SOUND_NO_ERROR;
        return gSoundLastError;
    }

    normalizedVolume = _soundVolumeHMItoDirectSound(_masterVol * volume / VOLUME_MAX);

    hr = audioEngineSoundBufferSetVolume(sound->soundBuffer, normalizedVolume);
    if (!hr) {
        gSoundLastError = SOUND_UNKNOWN_ERROR;
        return gSoundLastError;
    }

    gSoundLastError = SOUND_NO_ERROR;
    return gSoundLastError;
}

// 0x4ADE80
int _soundGetVolume(Sound* sound)
{
    if (!_deviceInit) {
        gSoundLastError = SOUND_NOT_INITIALIZED;
        return gSoundLastError;
    }

    if (sound == nullptr || sound->soundBuffer == -1) {
        gSoundLastError = SOUND_NO_SOUND;
        return gSoundLastError;
    }

    return sound->volume;
}

// 0x4ADFF0
int soundSetCallback(Sound* sound, SoundCallback* callback, void* userData)
{
    if (!gSoundInitialized) {
        gSoundLastError = SOUND_NOT_INITIALIZED;
        return gSoundLastError;
    }

    if (sound == nullptr) {
        gSoundLastError = SOUND_NO_SOUND;
        return gSoundLastError;
    }

    sound->callback = callback;
    sound->callbackUserData = userData;

    gSoundLastError = SOUND_NO_ERROR;
    return gSoundLastError;
}

// 0x4AE02C
int soundSetChannels(Sound* sound, int channels)
{
    if (!gSoundInitialized) {
        gSoundLastError = SOUND_NOT_INITIALIZED;
        return gSoundLastError;
    }

    if (sound == nullptr) {
        gSoundLastError = SOUND_NO_SOUND;
        return gSoundLastError;
    }

    if (channels == 3) {
        sound->channels = 2;
    }

    gSoundLastError = SOUND_NO_ERROR;
    return gSoundLastError;
}

// 0x4AE0B0
int soundSetReadLimit(Sound* sound, int readLimit)
{
    if (!gSoundInitialized) {
        gSoundLastError = SOUND_NOT_INITIALIZED;
        return gSoundLastError;
    }

    if (sound == nullptr) {
        gSoundLastError = SOUND_NO_DEVICE;
        return gSoundLastError;
    }

    sound->readLimit = readLimit;

    gSoundLastError = SOUND_NO_ERROR;
    return gSoundLastError;
}

// TODO: Check, looks like it uses couple of inlined functions.
//
// 0x4AE0E4
int soundPause(Sound* sound)
{
    bool hr;
    unsigned int readPos;
    unsigned int writePos;

    if (!gSoundInitialized) {
        gSoundLastError = SOUND_NOT_INITIALIZED;
        return gSoundLastError;
    }

    if (sound == nullptr) {
        gSoundLastError = SOUND_NO_SOUND;
        return gSoundLastError;
    }

    if (sound->soundBuffer == -1) {
        gSoundLastError = SOUND_NO_SOUND;
        return gSoundLastError;
    }

    if ((sound->statusFlags & SOUND_STATUS_IS_PLAYING) == 0) {
        gSoundLastError = SOUND_NOT_PLAYING;
        return gSoundLastError;
    }

    if ((sound->statusFlags & SOUND_STATUS_IS_PAUSED) != 0) {
        gSoundLastError = SOUND_ALREADY_PAUSED;
        return gSoundLastError;
    }

    hr = audioEngineSoundBufferGetCurrentPosition(sound->soundBuffer, &readPos, &writePos);
    if (!hr) {
        gSoundLastError = SOUND_UNKNOWN_ERROR;
        return gSoundLastError;
    }

    sound->pausePos = readPos;
    sound->statusFlags |= SOUND_STATUS_IS_PAUSED;

    return soundStop(sound);
}

// TODO: Check, looks like it uses couple of inlined functions.
//
// 0x4AE1F0
int soundResume(Sound* sound)
{
    bool hr;

    if (!gSoundInitialized) {
        gSoundLastError = SOUND_NOT_INITIALIZED;
        return gSoundLastError;
    }

    if (sound == nullptr || sound->soundBuffer == -1) {
        gSoundLastError = SOUND_NO_SOUND;
        return gSoundLastError;
    }

    if ((sound->statusFlags & SOUND_STATUS_IS_PLAYING) != 0) {
        gSoundLastError = SOUND_NOT_PAUSED;
        return gSoundLastError;
    }

    if ((sound->statusFlags & SOUND_STATUS_IS_PAUSED) == 0) {
        gSoundLastError = SOUND_NOT_PAUSED;
        return gSoundLastError;
    }

    hr = audioEngineSoundBufferSetCurrentPosition(sound->soundBuffer, sound->pausePos);
    if (!hr) {
        gSoundLastError = SOUND_UNKNOWN_ERROR;
        return gSoundLastError;
    }

    sound->statusFlags &= ~SOUND_STATUS_IS_PAUSED;
    sound->pausePos = 0;

    return soundPlay(sound);
}

// 0x4AE2FC
int soundSetFileIO(Sound* sound, SoundOpenProc* openProc, SoundCloseProc* closeProc, SoundReadProc* readProc, SoundWriteProc* writeProc, SoundSeekProc* seekProc, SoundTellProc* tellProc, SoundFileLengthProc* fileLengthProc)
{
    if (!gSoundInitialized) {
        gSoundLastError = SOUND_NOT_INITIALIZED;
        return gSoundLastError;
    }

    if (sound == nullptr) {
        gSoundLastError = SOUND_NO_SOUND;
        return gSoundLastError;
    }

    if (openProc != nullptr) {
        sound->io.open = openProc;
    }

    if (closeProc != nullptr) {
        sound->io.close = closeProc;
    }

    if (readProc != nullptr) {
        sound->io.read = readProc;
    }

    if (writeProc != nullptr) {
        sound->io.write = writeProc;
    }

    if (seekProc != nullptr) {
        sound->io.seek = seekProc;
    }

    if (tellProc != nullptr) {
        sound->io.tell = tellProc;
    }

    if (fileLengthProc != nullptr) {
        sound->io.filelength = fileLengthProc;
    }

    gSoundLastError = SOUND_NO_ERROR;
    return gSoundLastError;
}

// 0x4AE378
void soundDeleteInternal(Sound* sound)
{
    Sound* next;
    Sound* prev;

    if ((sound->statusFlags & SOUND_STATUS_IS_FADING) != 0) {
        FadeSound* fadeSound = _fadeHead;

        while (fadeSound != nullptr) {
            if (sound == fadeSound->sound) {
                break;
            }

            fadeSound = fadeSound->next;
        }

        _removeFadeSound(fadeSound);
    }

    if (sound->soundBuffer != -1) {
        // NOTE: Uninline.
        if (!soundIsPlaying(sound)) {
            soundStop(sound);
        }

        if (sound->callback != nullptr) {
            sound->callback(sound->callbackUserData, 1);
        }

        audioEngineSoundBufferRelease(sound->soundBuffer);
        sound->soundBuffer = -1;
    }

    if (sound->deleteCallback != nullptr) {
        sound->deleteCallback(sound->deleteUserData);
    }

    if (sound->data != nullptr) {
        gSoundFreeProc(sound->data);
        sound->data = nullptr;
    }

    next = sound->next;
    if (next != nullptr) {
        next->prev = sound->prev;
    }

    prev = sound->prev;
    if (prev != nullptr) {
        prev->next = sound->next;
    } else {
        gSoundListHead = sound->next;
    }

    gSoundFreeProc(sound);
}

// 0x4AE578
int _soundSetMasterVolume(int volume)
{
    if (volume < VOLUME_MIN || volume > VOLUME_MAX) {
        gSoundLastError = SOUND_UNKNOWN_ERROR;
        return gSoundLastError;
    }

    _masterVol = volume;

    Sound* curr = gSoundListHead;
    while (curr != nullptr) {
        soundSetVolume(curr, curr->volume);
        curr = curr->next;
    }

    gSoundLastError = SOUND_NO_ERROR;
    return gSoundLastError;
}

// 0x4AE5C8
Uint32 _doTimerEvent(Uint32 interval, void* param)
{
    void (*fn)();

    if (param != nullptr) {
        fn = (void (*)())param;
        fn();
    }

    return 40;
}

// 0x4AE614
void _removeTimedEvent(SDL_TimerID* timerId)
{
    if (*timerId != 0) {
        SDL_RemoveTimer(*timerId);
        *timerId = 0;
    }
}

// 0x4AE634
int _soundGetPosition(Sound* sound)
{
    if (!gSoundInitialized) {
        gSoundLastError = SOUND_NOT_INITIALIZED;
        return gSoundLastError;
    }

    if (sound == nullptr) {
        gSoundLastError = SOUND_NO_SOUND;
        return gSoundLastError;
    }

    unsigned int readPos;
    unsigned int writePos;
    audioEngineSoundBufferGetCurrentPosition(sound->soundBuffer, &readPos, &writePos);

    if ((sound->type & SOUND_TYPE_STREAMING) != 0) {
        if (readPos < sound->lastPosition) {
            readPos += sound->numBytesRead + sound->numBuffers * sound->dataSize - sound->lastPosition;
        } else {
            readPos -= sound->lastPosition + sound->numBytesRead;
        }
    }

    return readPos;
}

// 0x4AE6CC
int _soundSetPosition(Sound* sound, int pos)
{
    if (!gSoundInitialized) {
        gSoundLastError = SOUND_NOT_INITIALIZED;
        return gSoundLastError;
    }

    if (sound == nullptr) {
        gSoundLastError = SOUND_NO_SOUND;
        return gSoundLastError;
    }

    if (sound->soundBuffer == -1) {
        gSoundLastError = SOUND_NO_SOUND;
        return gSoundLastError;
    }

    if ((sound->type & SOUND_TYPE_STREAMING) != 0) {
        int section = pos / sound->dataSize % sound->numBuffers;

        audioEngineSoundBufferSetCurrentPosition(sound->soundBuffer, section * sound->dataSize + pos % sound->dataSize);

        sound->io.seek(sound->io.fd, section * sound->dataSize, SEEK_SET);
        int bytesRead = sound->io.read(sound->io.fd, sound->data, sound->dataSize);
        if (bytesRead < sound->dataSize) {
            if ((sound->type & SOUND_TYPE_STREAMING) != 0) {
                sound->io.seek(sound->io.fd, 0, SEEK_SET);
                sound->io.read(sound->io.fd, sound->data + bytesRead, sound->dataSize - bytesRead);
            } else {
                memset(sound->data + bytesRead, 0, sound->dataSize - bytesRead);
            }
        }

        int nextSection = section + 1;
        sound->numBytesRead = pos;

        if (nextSection < sound->numBuffers) {
            sound->lastUpdate = nextSection;
        } else {
            sound->lastUpdate = 0;
        }

        soundContinue(sound);
    } else {
        audioEngineSoundBufferSetCurrentPosition(sound->soundBuffer, pos);
    }

    gSoundLastError = SOUND_NO_ERROR;
    return gSoundLastError;
}

// 0x4AE830
void _removeFadeSound(FadeSound* fadeSound)
{
    FadeSound* prev;
    FadeSound* next;
    FadeSound* tmp;

    if (fadeSound == nullptr) {
        return;
    }

    if (fadeSound->sound == nullptr) {
        return;
    }

    if ((fadeSound->sound->statusFlags & SOUND_STATUS_IS_FADING) == 0) {
        return;
    }

    prev = fadeSound->prev;
    if (prev != nullptr) {
        prev->next = fadeSound->next;
    } else {
        _fadeHead = fadeSound->next;
    }

    next = fadeSound->next;
    if (next != nullptr) {
        next->prev = fadeSound->prev;
    }

    fadeSound->sound->statusFlags &= ~SOUND_STATUS_IS_FADING;
    fadeSound->sound = nullptr;

    tmp = _fadeFreeList;
    _fadeFreeList = fadeSound;
    fadeSound->next = tmp;
}

// 0x4AE8B0
void _fadeSounds()
{
    FadeSound* fadeSound;

    fadeSound = _fadeHead;
    while (fadeSound != nullptr) {
        if ((fadeSound->currentVolume > fadeSound->targetVolume || fadeSound->currentVolume + fadeSound->deltaVolume < fadeSound->targetVolume) && (fadeSound->currentVolume < fadeSound->targetVolume || fadeSound->currentVolume + fadeSound->deltaVolume > fadeSound->targetVolume)) {
            fadeSound->currentVolume += fadeSound->deltaVolume;
            soundSetVolume(fadeSound->sound, fadeSound->currentVolume);
        } else {
            if (fadeSound->targetVolume == 0) {
                if (fadeSound->pause) {
                    soundPause(fadeSound->sound);
                    soundSetVolume(fadeSound->sound, fadeSound->initialVolume);
                } else {
                    if ((fadeSound->sound->type & SOUND_TYPE_FIRE_AND_FORGET) != 0) {
                        soundDelete(fadeSound->sound);
                    } else {
                        soundStop(fadeSound->sound);

                        fadeSound->initialVolume = fadeSound->targetVolume;
                        fadeSound->currentVolume = fadeSound->targetVolume;
                        fadeSound->deltaVolume = 0;

                        soundSetVolume(fadeSound->sound, fadeSound->targetVolume);
                    }
                }
            }

            _removeFadeSound(fadeSound);
        }
    }

    if (_fadeHead == nullptr) {
        // NOTE: Uninline.
        _removeTimedEvent(&gFadeSoundsTimerId);
    }
}

// 0x4AE988
int _internalSoundFade(Sound* sound, int duration, int targetVolume, bool pause)
{
    FadeSound* fadeSound;

    if (!_deviceInit) {
        gSoundLastError = SOUND_NOT_INITIALIZED;
        return gSoundLastError;
    }

    if (sound == nullptr) {
        gSoundLastError = SOUND_NO_SOUND;
        return gSoundLastError;
    }

    fadeSound = nullptr;
    if ((sound->statusFlags & SOUND_STATUS_IS_FADING) != 0) {
        fadeSound = _fadeHead;
        while (fadeSound != nullptr) {
            if (fadeSound->sound == sound) {
                break;
            }

            fadeSound = fadeSound->next;
        }
    }

    if (fadeSound == nullptr) {
        if (_fadeFreeList != nullptr) {
            fadeSound = _fadeFreeList;
            _fadeFreeList = _fadeFreeList->next;
        } else {
            fadeSound = (FadeSound*)gSoundMallocProc(sizeof(FadeSound));
        }

        if (fadeSound != nullptr) {
            if (_fadeHead != nullptr) {
                _fadeHead->prev = fadeSound;
            }

            fadeSound->sound = sound;
            fadeSound->prev = nullptr;
            fadeSound->next = _fadeHead;
            _fadeHead = fadeSound;
        }
    }

    if (fadeSound == nullptr) {
        gSoundLastError = SOUND_NO_MEMORY_AVAILABLE;
        return gSoundLastError;
    }

    fadeSound->targetVolume = targetVolume;
    fadeSound->initialVolume = _soundGetVolume(sound);
    fadeSound->currentVolume = fadeSound->initialVolume;
    fadeSound->pause = pause;
    // TODO: Check.
    fadeSound->deltaVolume = 8 * (125 * (targetVolume - fadeSound->initialVolume)) / (40 * duration);

    sound->statusFlags |= SOUND_STATUS_IS_FADING;

    bool shouldPlay;
    if (gSoundInitialized) {
        if (sound->soundBuffer != -1) {
            shouldPlay = (sound->statusFlags & SOUND_STATUS_IS_PLAYING) == 0;
        } else {
            gSoundLastError = SOUND_NO_SOUND;
            shouldPlay = true;
        }
    } else {
        gSoundLastError = SOUND_NOT_INITIALIZED;
        shouldPlay = true;
    }

    if (shouldPlay) {
        soundPlay(sound);
    }

    if (gFadeSoundsTimerId != 0) {
        gSoundLastError = SOUND_NO_ERROR;
        return gSoundLastError;
    }

    gFadeSoundsTimerId = SDL_AddTimer(40, _doTimerEvent, (void*)_fadeSounds);
    if (gFadeSoundsTimerId == 0) {
        gSoundLastError = SOUND_UNKNOWN_ERROR;
        return gSoundLastError;
    }

    gSoundLastError = SOUND_NO_ERROR;
    return gSoundLastError;
}

// 0x4AEB0C
int _soundFade(Sound* sound, int duration, int targetVolume)
{
    return _internalSoundFade(sound, duration, targetVolume, false);
}

// 0x4AEB54
void soundDeleteAll()
{
    while (gSoundListHead != nullptr) {
        soundDelete(gSoundListHead);
    }
}

// 0x4AEBE0
void soundContinueAll()
{
    Sound* curr = gSoundListHead;
    while (curr != nullptr) {
        // Sound can be deallocated in `soundContinue`.
        Sound* next = curr->next;
        soundContinue(curr);
        curr = next;
    }
}

// 0x4AEC00
int soundSetDefaultFileIO(SoundOpenProc* openProc, SoundCloseProc* closeProc, SoundReadProc* readProc, SoundWriteProc* writeProc, SoundSeekProc* seekProc, SoundTellProc* tellProc, SoundFileLengthProc* fileLengthProc)
{
    if (openProc != nullptr) {
        gSoundDefaultFileIO.open = openProc;
    }

    if (closeProc != nullptr) {
        gSoundDefaultFileIO.close = closeProc;
    }

    if (readProc != nullptr) {
        gSoundDefaultFileIO.read = readProc;
    }

    if (writeProc != nullptr) {
        gSoundDefaultFileIO.write = writeProc;
    }

    if (seekProc != nullptr) {
        gSoundDefaultFileIO.seek = seekProc;
    }

    if (tellProc != nullptr) {
        gSoundDefaultFileIO.tell = tellProc;
    }

    if (fileLengthProc != nullptr) {
        gSoundDefaultFileIO.filelength = fileLengthProc;
    }

    gSoundLastError = SOUND_NO_ERROR;
    return gSoundLastError;
}

} // namespace fallout
