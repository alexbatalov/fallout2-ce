#include "sound.h"

#include "audio_engine.h"
#include "debug.h"
#include "platform_compat.h"

#ifdef _WIN32
#include <io.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>

#include <SDL.h>

#define SOUND_FLAG_SOUND_IS_PLAYING (0x02)
#define SOUND_FLAG_SOUND_IS_PAUSED (0x08)

typedef char*(SoundFileNameMangler)(char*);

typedef struct STRUCT_51D478 {
    Sound* field_0;
    int field_4;
    int field_8;
    int field_C;
    int field_10;
    int field_14;
    struct STRUCT_51D478* prev;
    struct STRUCT_51D478* next;
} STRUCT_51D478;

static void* soundMallocProcDefaultImpl(size_t size);
static void* soundReallocProcDefaultImpl(void* ptr, size_t size);
static void soundFreeProcDefaultImpl(void* ptr);
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
static void _removeFadeSound(STRUCT_51D478* a1);
static void _fadeSounds();
static int _internalSoundFade(Sound* sound, int a2, int a3, int a4);

// 0x51D478
static STRUCT_51D478* _fadeHead = NULL;

// 0x51D47C
static STRUCT_51D478* _fadeFreeList = NULL;

// 0x51D488
static MallocProc* gSoundMallocProc = soundMallocProcDefaultImpl;

// 0x51D48C
static ReallocProc* gSoundReallocProc = soundReallocProcDefaultImpl;

// 0x51D490
static FreeProc* gSoundFreeProc = soundFreeProcDefaultImpl;

// 0x51D494
static SoundFileIO gSoundDefaultFileIO = {
    open,
    close,
    compat_read,
    compat_write,
    compat_lseek,
    compat_tell,
    compat_filelength,
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
    if (sound->field_3C & 0x80) {
        return;
    }

    unsigned int readPos;
    unsigned int writePos;
    bool hr = audioEngineSoundBufferGetCurrentPosition(sound->soundBuffer, &readPos, &writePos);
    if (!hr) {
        return;
    }

    if (readPos < sound->field_74) {
        sound->field_64 += readPos + sound->field_78 * sound->field_7C - sound->field_74;
    } else {
        sound->field_64 += readPos - sound->field_74;
    }

    if (sound->field_3C & 0x0100) {
        if (sound->field_44 & 0x20) {
            if (sound->field_3C & 0x0200) {
                sound->field_3C |= 0x80;
            }
        } else {
            if (sound->field_60 <= sound->field_64) {
                sound->field_3C |= 0x0280;
            }
        }
    }
    sound->field_74 = readPos;

    if (sound->field_60 < sound->field_64) {
        int v3;
        do {
            v3 = sound->field_64 - sound->field_60;
            sound->field_64 = v3;
        } while (v3 > sound->field_60);
    }

    int v6 = readPos / sound->field_7C;
    if (sound->field_70 == v6) {
        return;
    }

    int v53;
    if (sound->field_70 > v6) {
        v53 = v6 + sound->field_78 - sound->field_70;
    } else {
        v53 = v6 - sound->field_70;
    }

    if (sound->field_7C * v53 >= sound->readLimit) {
        v53 = (sound->readLimit + sound->field_7C - 1) / sound->field_7C;
    }

    if (v53 < sound->field_5C) {
        return;
    }

    void* audioPtr1;
    void* audioPtr2;
    unsigned int audioBytes1;
    unsigned int audioBytes2;
    hr = audioEngineSoundBufferLock(sound->soundBuffer, sound->field_7C * sound->field_70, sound->field_7C * v53, &audioPtr1, &audioBytes1, &audioPtr2, &audioBytes2, 0);
    if (!hr) {
        return;
    }

    if (audioBytes1 + audioBytes2 != sound->field_7C * v53) {
        debugPrint("locked memory region not big enough, wanted %d (%d * %d), got %d (%d + %d)\n", sound->field_7C * v53, v53, sound->field_7C, audioBytes1 + audioBytes2, audioBytes1, audioBytes2);
        debugPrint("Resetting readBuffers from %d to %d\n", v53, (audioBytes1 + audioBytes2) / sound->field_7C);

        v53 = (audioBytes1 + audioBytes2) / sound->field_7C;
        if (v53 < sound->field_5C) {
            debugPrint("No longer above read buffer size, returning\n");
            return;
        }
    }
    unsigned char* audioPtr = (unsigned char*)audioPtr1;
    int audioBytes = audioBytes1;
    while (--v53 != -1) {
        int bytesRead;
        if (sound->field_3C & 0x0200) {
            bytesRead = sound->field_7C;
            memset(sound->field_20, 0, bytesRead);
        } else {
            int bytesToRead = sound->field_7C;
            if (sound->field_58 != -1) {
                int pos = sound->io.tell(sound->io.fd);
                if (bytesToRead + pos > sound->field_58) {
                    bytesToRead = sound->field_58 - pos;
                }
            }

            bytesRead = sound->io.read(sound->io.fd, sound->field_20, bytesToRead);
            if (bytesRead < sound->field_7C) {
                if (!(sound->field_3C & 0x20) || (sound->field_3C & 0x0100)) {
                    memset(sound->field_20 + bytesRead, 0, sound->field_7C - bytesRead);
                    sound->field_3C |= 0x0200;
                    bytesRead = sound->field_7C;
                } else {
                    while (bytesRead < sound->field_7C) {
                        if (sound->field_50 == -1) {
                            sound->io.seek(sound->io.fd, sound->field_54, SEEK_SET);
                            if (sound->callback != NULL) {
                                sound->callback(sound->callbackUserData, 0x0400);
                            }
                        } else {
                            if (sound->field_50 <= 0) {
                                sound->field_58 = -1;
                                sound->field_54 = 0;
                                sound->field_50 = 0;
                                sound->field_3C &= ~0x20;
                                bytesRead += sound->io.read(sound->io.fd, sound->field_20 + bytesRead, sound->field_7C - bytesRead);
                                break;
                            }

                            sound->field_50--;
                            sound->io.seek(sound->io.fd, sound->field_54, SEEK_SET);

                            if (sound->callback != NULL) {
                                sound->callback(sound->callbackUserData, 0x400);
                            }
                        }

                        if (sound->field_58 == -1) {
                            bytesToRead = sound->field_7C - bytesRead;
                        } else {
                            int pos = sound->io.tell(sound->io.fd);
                            if (sound->field_7C + bytesRead + pos <= sound->field_58) {
                                bytesToRead = sound->field_7C - bytesRead;
                            } else {
                                bytesToRead = sound->field_58 - bytesRead - pos;
                            }
                        }

                        int v20 = sound->io.read(sound->io.fd, sound->field_20 + bytesRead, bytesToRead);
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
                memcpy(audioPtr, sound->field_20, audioBytes);
            }

            if (audioPtr2 != NULL) {
                memcpy(audioPtr2, sound->field_20 + audioBytes, bytesRead - audioBytes);
                audioPtr = (unsigned char*)audioPtr2 + bytesRead - audioBytes;
                audioBytes = audioBytes2 - bytesRead;
            } else {
                debugPrint("Hm, no second write pointer, but buffer not big enough, this shouldn't happen\n");
            }
        } else {
            memcpy(audioPtr, sound->field_20, bytesRead);
            audioPtr += bytesRead;
            audioBytes -= bytesRead;
        }
    }

    audioEngineSoundBufferUnlock(sound->soundBuffer, audioPtr1, audioBytes1, audioPtr2, audioBytes2);

    sound->field_70 = v6;
}

// 0x4ACC58
int soundInit(int a1, int a2, int a3, int a4, int rate)
{
    if (!audioEngineInit()) {
        debugPrint("soundInit: Unable to init audio engine\n");

        gSoundLastError = SOUND_SOS_DETECTION_FAILURE;
        return gSoundLastError;
    }

    _sampleRate = rate;
    _dataSize = a4;
    _numBuffers = a2;
    gSoundInitialized = true;
    _deviceInit = 1;

    _soundSetMasterVolume(VOLUME_MAX);

    gSoundLastError = SOUND_NO_ERROR;
    return gSoundLastError;
}

// 0x4AD04C
void soundExit()
{
    while (gSoundListHead != NULL) {
        Sound* next = gSoundListHead->next;
        soundDelete(gSoundListHead);
        gSoundListHead = next;
    }

    if (gFadeSoundsTimerId != 0) {
        _removeTimedEvent(&gFadeSoundsTimerId);
    }

    while (_fadeFreeList != NULL) {
        STRUCT_51D478* next = _fadeFreeList->next;
        gSoundFreeProc(_fadeFreeList);
        _fadeFreeList = next;
    }

    audioEngineExit();

    gSoundLastError = SOUND_NO_ERROR;
    gSoundInitialized = false;
}

// 0x4AD0FC
Sound* soundAllocate(int a1, int a2)
{
    if (!gSoundInitialized) {
        gSoundLastError = SOUND_NOT_INITIALIZED;
        return NULL;
    }

    Sound* sound = (Sound*)gSoundMallocProc(sizeof(*sound));
    memset(sound, 0, sizeof(*sound));

    memcpy(&(sound->io), &gSoundDefaultFileIO, sizeof(gSoundDefaultFileIO));

    if (!(a2 & 0x02)) {
        a2 |= 0x02;
    }

    sound->bitsPerSample = (a2 & 0x08) != 0 ? 16 : 8;
    sound->channels = 1;
    sound->rate = _sampleRate;

    sound->field_3C = a2;
    sound->field_44 = a1;
    sound->field_7C = _dataSize;
    sound->field_64 = 0;
    sound->soundBuffer = -1;
    sound->field_40 = 0;
    sound->field_78 = _numBuffers;
    sound->readLimit = sound->field_7C * _numBuffers;

    if (a1 & 0x10) {
        sound->field_50 = -1;
        sound->field_3C |= 0x20;
    }

    sound->field_58 = -1;
    sound->field_5C = 1;
    sound->volume = VOLUME_MAX;
    sound->prev = NULL;
    sound->field_54 = 0;
    sound->next = gSoundListHead;

    if (gSoundListHead != NULL) {
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
    sound->field_60 = size;

    if (sound->field_44 & 0x02) {
        if (!(sound->field_3C & 0x20)) {
            sound->field_3C |= 0x0120;
        }

        if (sound->field_78 * sound->field_7C >= size) {
            if (size / sound->field_7C * sound->field_7C != size) {
                size = (size / sound->field_7C + 1) * sound->field_7C;
            }
        } else {
            size = sound->field_78 * sound->field_7C;
        }
    } else {
        sound->field_44 &= ~(0x03);
        sound->field_44 |= 0x01;
    }

    buf = (unsigned char*)gSoundMallocProc(size);
    bytes_read = sound->io.read(sound->io.fd, buf, size);
    if (bytes_read != size) {
        if (!(sound->field_3C & 0x20) || (sound->field_3C & (0x01 << 8))) {
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

    if (sound->field_44 & 0x01) {
        sound->io.close(sound->io.fd);
        sound->io.fd = -1;
    } else {
        if (sound->field_20 == NULL) {
            sound->field_20 = (unsigned char*)gSoundMallocProc(sound->field_7C);
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

    if (sound == NULL) {
        gSoundLastError = SOUND_NO_SOUND;
        return gSoundLastError;
    }

    sound->io.fd = sound->io.open(gSoundFileNameMangler(filePath), 0x0200);
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

    if (sound == NULL || sound->soundBuffer == -1) {
        gSoundLastError = SOUND_NO_SOUND;
        return gSoundLastError;
    }

    if (sound->field_44 & 0x02) {
        sound->io.seek(sound->io.fd, 0, SEEK_SET);
        sound->field_70 = 0;
        sound->field_74 = 0;
        sound->field_64 = 0;
        sound->field_3C &= 0xFD7F;
        hr = audioEngineSoundBufferSetCurrentPosition(sound->soundBuffer, 0);
        _preloadBuffers(sound);
    } else {
        hr = audioEngineSoundBufferSetCurrentPosition(sound->soundBuffer, 0);
    }

    if (!hr) {
        gSoundLastError = SOUND_UNKNOWN_ERROR;
        return gSoundLastError;
    }

    sound->field_40 &= ~(0x01);

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

    if (audioPtr2 != NULL) {
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

    if (sound == NULL) {
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

    if (sound == NULL || sound->soundBuffer == -1) {
        gSoundLastError = SOUND_NO_SOUND;
        return gSoundLastError;
    }

    // TODO: Check.
    if (sound->field_40 & 0x01) {
        _soundRewind(sound);
    }

    soundSetVolume(sound, sound->volume);

    hr = audioEngineSoundBufferPlay(sound->soundBuffer, sound->field_3C & 0x20 ? AUDIO_ENGINE_SOUND_BUFFER_PLAY_LOOPING : 0);

    audioEngineSoundBufferGetCurrentPosition(sound->soundBuffer, &readPos, &writePos);
    sound->field_70 = readPos / sound->field_7C;

    if (!hr) {
        gSoundLastError = SOUND_UNKNOWN_ERROR;
        return gSoundLastError;
    }

    sound->field_40 |= SOUND_FLAG_SOUND_IS_PLAYING;

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

    if (sound == NULL || sound->soundBuffer == -1) {
        gSoundLastError = SOUND_NO_SOUND;
        return gSoundLastError;
    }

    if (!(sound->field_40 & SOUND_FLAG_SOUND_IS_PLAYING)) {
        gSoundLastError = SOUND_NOT_PLAYING;
        return gSoundLastError;
    }

    hr = audioEngineSoundBufferStop(sound->soundBuffer);
    if (!hr) {
        gSoundLastError = SOUND_UNKNOWN_ERROR;
        return gSoundLastError;
    }

    sound->field_40 &= ~SOUND_FLAG_SOUND_IS_PLAYING;
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

    if (sample == NULL) {
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

    if (sound == NULL) {
        gSoundLastError = SOUND_NO_SOUND;
        return gSoundLastError;
    }

    if (sound->soundBuffer == -1) {
        gSoundLastError = SOUND_NO_SOUND;
        return gSoundLastError;
    }

    if (!(sound->field_40 & SOUND_FLAG_SOUND_IS_PLAYING) || (sound->field_40 & SOUND_FLAG_SOUND_IS_PAUSED)) {
        gSoundLastError = SOUND_NOT_PLAYING;
        return gSoundLastError;
    }

    if (sound->field_40 & 0x01) {
        gSoundLastError = SOUND_UNKNOWN_ERROR;
        return gSoundLastError;
    }

    hr = audioEngineSoundBufferGetStatus(sound->soundBuffer, &status);
    if (!hr) {
        debugPrint("Error in soundContinue, %x\n", hr);

        gSoundLastError = SOUND_UNKNOWN_ERROR;
        return gSoundLastError;
    }

    if (!(sound->field_3C & 0x80) && (status & (AUDIO_ENGINE_SOUND_BUFFER_STATUS_PLAYING | AUDIO_ENGINE_SOUND_BUFFER_STATUS_LOOPING))) {
        if (!(sound->field_40 & SOUND_FLAG_SOUND_IS_PAUSED) && (sound->field_44 & 0x02)) {
            _refreshSoundBuffers(sound);
        }
    } else if (!(sound->field_40 & SOUND_FLAG_SOUND_IS_PAUSED)) {
        if (sound->callback != NULL) {
            sound->callback(sound->callbackUserData, 1);
            sound->callback = NULL;
        }

        if (sound->field_44 & 0x04) {
            sound->callback = NULL;
            soundDelete(sound);
        } else {
            sound->field_40 |= 0x01;

            if (sound->field_40 & 0x02) {
                --_numSounds;
            }

            soundStop(sound);

            sound->field_40 &= ~(0x03);
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

    if (sound == NULL || sound->soundBuffer == -1) {
        gSoundLastError = SOUND_NO_SOUND;
        return false;
    }

    return (sound->field_40 & SOUND_FLAG_SOUND_IS_PLAYING) != 0;
}

// 0x4ADAC4
bool _soundDone(Sound* sound)
{
    if (!gSoundInitialized) {
        gSoundLastError = SOUND_NOT_INITIALIZED;
        return false;
    }

    if (sound == NULL || sound->soundBuffer == -1) {
        gSoundLastError = SOUND_NO_SOUND;
        return false;
    }

    return sound->field_40 & 1;
}

// 0x4ADB44
bool soundIsPaused(Sound* sound)
{
    if (!gSoundInitialized) {
        gSoundLastError = SOUND_NOT_INITIALIZED;
        return false;
    }

    if (sound == NULL || sound->soundBuffer == -1) {
        gSoundLastError = SOUND_NO_SOUND;
        return false;
    }

    return (sound->field_40 & SOUND_FLAG_SOUND_IS_PAUSED) != 0;
}

// 0x4ADBC4
int _soundType(Sound* sound, int a2)
{
    if (!gSoundInitialized) {
        gSoundLastError = SOUND_NOT_INITIALIZED;
        return 0;
    }

    if (sound == NULL || sound->soundBuffer == -1) {
        gSoundLastError = SOUND_NO_SOUND;
        return 0;
    }

    return sound->field_44 & a2;
}

// 0x4ADC04
int soundGetDuration(Sound* sound)
{
    if (!gSoundInitialized) {
        gSoundLastError = SOUND_NOT_INITIALIZED;
        return gSoundLastError;
    }

    if (sound == NULL || sound->soundBuffer == -1) {
        gSoundLastError = SOUND_NO_SOUND;
        return gSoundLastError;
    }

    int bytesPerSec = sound->bitsPerSample / 8 * sound->rate;
    int v3 = sound->field_60;
    int v4 = v3 % bytesPerSec;
    int result = v3 / bytesPerSec;
    if (v4 != 0) {
        result += 1;
    }

    return result;
}

// 0x4ADD00
int soundSetLooping(Sound* sound, int a2)
{
    if (!gSoundInitialized) {
        gSoundLastError = SOUND_NOT_INITIALIZED;
        return gSoundLastError;
    }

    if (sound == NULL) {
        gSoundLastError = SOUND_NO_SOUND;
        return gSoundLastError;
    }

    if (a2) {
        sound->field_3C |= 0x20;
        sound->field_50 = a2;
    } else {
        sound->field_50 = 0;
        sound->field_58 = -1;
        sound->field_54 = 0;
        sound->field_3C &= ~(0x20);
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

    if (sound == NULL) {
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

    if (sound == NULL || sound->soundBuffer == -1) {
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

    if (sound == NULL) {
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

    if (sound == NULL) {
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

    if (sound == NULL) {
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

    if (sound == NULL) {
        gSoundLastError = SOUND_NO_SOUND;
        return gSoundLastError;
    }

    if (sound->soundBuffer == -1) {
        gSoundLastError = SOUND_NO_SOUND;
        return gSoundLastError;
    }

    if (!(sound->field_40 & SOUND_FLAG_SOUND_IS_PLAYING)) {
        gSoundLastError = SOUND_NOT_PLAYING;
        return gSoundLastError;
    }

    if (sound->field_40 & SOUND_FLAG_SOUND_IS_PAUSED) {
        gSoundLastError = SOUND_ALREADY_PAUSED;
        return gSoundLastError;
    }

    hr = audioEngineSoundBufferGetCurrentPosition(sound->soundBuffer, &readPos, &writePos);
    if (!hr) {
        gSoundLastError = SOUND_UNKNOWN_ERROR;
        return gSoundLastError;
    }

    sound->field_48 = readPos;
    sound->field_40 |= SOUND_FLAG_SOUND_IS_PAUSED;

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

    if (sound == NULL || sound->soundBuffer == -1) {
        gSoundLastError = SOUND_NO_SOUND;
        return gSoundLastError;
    }

    if ((sound->field_40 & SOUND_FLAG_SOUND_IS_PLAYING) != 0) {
        gSoundLastError = SOUND_NOT_PAUSED;
        return gSoundLastError;
    }

    if (!(sound->field_40 & SOUND_FLAG_SOUND_IS_PAUSED)) {
        gSoundLastError = SOUND_NOT_PAUSED;
        return gSoundLastError;
    }

    hr = audioEngineSoundBufferSetCurrentPosition(sound->soundBuffer, sound->field_48);
    if (!hr) {
        gSoundLastError = SOUND_UNKNOWN_ERROR;
        return gSoundLastError;
    }

    sound->field_40 &= ~SOUND_FLAG_SOUND_IS_PAUSED;
    sound->field_48 = 0;

    return soundPlay(sound);
}

// 0x4AE2FC
int soundSetFileIO(Sound* sound, SoundOpenProc* openProc, SoundCloseProc* closeProc, SoundReadProc* readProc, SoundWriteProc* writeProc, SoundSeekProc* seekProc, SoundTellProc* tellProc, SoundFileLengthProc* fileLengthProc)
{
    if (!gSoundInitialized) {
        gSoundLastError = SOUND_NOT_INITIALIZED;
        return gSoundLastError;
    }

    if (sound == NULL) {
        gSoundLastError = SOUND_NO_SOUND;
        return gSoundLastError;
    }

    if (openProc != NULL) {
        sound->io.open = openProc;
    }

    if (closeProc != NULL) {
        sound->io.close = closeProc;
    }

    if (readProc != NULL) {
        sound->io.read = readProc;
    }

    if (writeProc != NULL) {
        sound->io.write = writeProc;
    }

    if (seekProc != NULL) {
        sound->io.seek = seekProc;
    }

    if (tellProc != NULL) {
        sound->io.tell = tellProc;
    }

    if (fileLengthProc != NULL) {
        sound->io.filelength = fileLengthProc;
    }

    gSoundLastError = SOUND_NO_ERROR;
    return gSoundLastError;
}

// 0x4AE378
void soundDeleteInternal(Sound* sound)
{
    STRUCT_51D478* curr;
    Sound* v10;
    Sound* v11;

    if (sound->field_40 & 0x04) {
        curr = _fadeHead;

        while (curr != NULL) {
            if (sound == curr->field_0) {
                break;
            }

            curr = curr->next;
        }

        _removeFadeSound(curr);
    }

    if (sound->soundBuffer != -1) {
        // NOTE: Uninline.
        if (!soundIsPlaying(sound)) {
            soundStop(sound);
        }

        if (sound->callback != NULL) {
            sound->callback(sound->callbackUserData, 1);
        }

        audioEngineSoundBufferRelease(sound->soundBuffer);
        sound->soundBuffer = -1;
    }

    if (sound->field_90 != NULL) {
        sound->field_90(sound->field_8C);
    }

    if (sound->field_20 != NULL) {
        gSoundFreeProc(sound->field_20);
        sound->field_20 = NULL;
    }

    v10 = sound->next;
    if (v10 != NULL) {
        v10->prev = sound->prev;
    }

    v11 = sound->prev;
    if (v11 != NULL) {
        v11->next = sound->next;
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
    while (curr != NULL) {
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

    if (param != NULL) {
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

    if (sound == NULL) {
        gSoundLastError = SOUND_NO_SOUND;
        return gSoundLastError;
    }

    unsigned int playPos;
    unsigned int writePos;
    audioEngineSoundBufferGetCurrentPosition(sound->soundBuffer, &playPos, &writePos);

    if ((sound->field_44 & 0x02) != 0) {
        if (playPos < sound->field_74) {
            playPos += sound->field_64 + sound->field_78 * sound->field_7C - sound->field_74;
        } else {
            playPos -= sound->field_74 + sound->field_64;
        }
    }

    return playPos;
}

// 0x4AE6CC
int _soundSetPosition(Sound* sound, int a2)
{
    if (!gSoundInitialized) {
        gSoundLastError = SOUND_NOT_INITIALIZED;
        return gSoundLastError;
    }

    if (sound == NULL) {
        gSoundLastError = SOUND_NO_SOUND;
        return gSoundLastError;
    }

    if (sound->soundBuffer == -1) {
        gSoundLastError = SOUND_NO_SOUND;
        return gSoundLastError;
    }

    if (sound->field_44 & 0x02) {
        int v6 = a2 / sound->field_7C % sound->field_78;

        audioEngineSoundBufferSetCurrentPosition(sound->soundBuffer, v6 * sound->field_7C + a2 % sound->field_7C);

        sound->io.seek(sound->io.fd, v6 * sound->field_7C, SEEK_SET);
        int bytes_read = sound->io.read(sound->io.fd, sound->field_20, sound->field_7C);
        if (bytes_read < sound->field_7C) {
            if (sound->field_44 & 0x02) {
                sound->io.seek(sound->io.fd, 0, SEEK_SET);
                sound->io.read(sound->io.fd, sound->field_20 + bytes_read, sound->field_7C - bytes_read);
            } else {
                memset(sound->field_20 + bytes_read, 0, sound->field_7C - bytes_read);
            }
        }

        int v17 = v6 + 1;
        sound->field_64 = a2;

        if (v17 < sound->field_78) {
            sound->field_70 = v17;
        } else {
            sound->field_70 = 0;
        }

        soundContinue(sound);
    } else {
        audioEngineSoundBufferSetCurrentPosition(sound->soundBuffer, a2);
    }

    gSoundLastError = SOUND_NO_ERROR;
    return gSoundLastError;
}

// 0x4AE830
void _removeFadeSound(STRUCT_51D478* a1)
{
    STRUCT_51D478* prev;
    STRUCT_51D478* next;
    STRUCT_51D478* tmp;

    if (a1 == NULL) {
        return;
    }

    if (a1->field_0 == NULL) {
        return;
    }

    if (!(a1->field_0->field_40 & 0x04)) {
        return;
    }

    prev = a1->prev;
    if (prev != NULL) {
        prev->next = a1->next;
    } else {
        _fadeHead = a1->next;
    }

    next = a1->next;
    if (next != NULL) {
        next->prev = a1->prev;
    }

    a1->field_0->field_40 &= ~(0x04);
    a1->field_0 = NULL;

    tmp = _fadeFreeList;
    _fadeFreeList = a1;
    a1->next = tmp;
}

// 0x4AE8B0
void _fadeSounds()
{
    STRUCT_51D478* ptr;

    ptr = _fadeHead;
    while (ptr != NULL) {
        if ((ptr->field_10 > ptr->field_8 || ptr->field_10 + ptr->field_4 < ptr->field_8) && (ptr->field_10 < ptr->field_8 || ptr->field_10 + ptr->field_4 > ptr->field_8)) {
            ptr->field_10 += ptr->field_4;
            soundSetVolume(ptr->field_0, ptr->field_10);
        } else {
            if (ptr->field_8 == 0) {
                if (ptr->field_14) {
                    soundPause(ptr->field_0);
                    soundSetVolume(ptr->field_0, ptr->field_C);
                } else {
                    if (ptr->field_0->field_44 & 0x04) {
                        soundDelete(ptr->field_0);
                    } else {
                        soundStop(ptr->field_0);

                        ptr->field_C = ptr->field_8;
                        ptr->field_10 = ptr->field_8;
                        ptr->field_4 = 0;

                        soundSetVolume(ptr->field_0, ptr->field_8);
                    }
                }
            }

            _removeFadeSound(ptr);
        }
    }

    if (_fadeHead == NULL) {
        // NOTE: Uninline.
        _removeTimedEvent(&gFadeSoundsTimerId);
    }
}

// 0x4AE988
int _internalSoundFade(Sound* sound, int a2, int a3, int a4)
{
    STRUCT_51D478* ptr;

    if (!_deviceInit) {
        gSoundLastError = SOUND_NOT_INITIALIZED;
        return gSoundLastError;
    }

    if (sound == NULL) {
        gSoundLastError = SOUND_NO_SOUND;
        return gSoundLastError;
    }

    ptr = NULL;
    if (sound->field_40 & 0x04) {
        ptr = _fadeHead;
        while (ptr != NULL) {
            if (ptr->field_0 == sound) {
                break;
            }

            ptr = ptr->next;
        }
    }

    if (ptr == NULL) {
        if (_fadeFreeList != NULL) {
            ptr = _fadeFreeList;
            _fadeFreeList = _fadeFreeList->next;
        } else {
            ptr = (STRUCT_51D478*)gSoundMallocProc(sizeof(STRUCT_51D478));
        }

        if (ptr != NULL) {
            if (_fadeHead != NULL) {
                _fadeHead->prev = ptr;
            }

            ptr->field_0 = sound;
            ptr->prev = NULL;
            ptr->next = _fadeHead;
            _fadeHead = ptr;
        }
    }

    if (ptr == NULL) {
        gSoundLastError = SOUND_NO_MEMORY_AVAILABLE;
        return gSoundLastError;
    }

    ptr->field_8 = a3;
    ptr->field_C = _soundGetVolume(sound);
    ptr->field_10 = ptr->field_C;
    ptr->field_14 = a4;
    // TODO: Check.
    ptr->field_4 = 8 * (125 * (a3 - ptr->field_C)) / (40 * a2);

    sound->field_40 |= 0x04;

    bool v14;
    if (gSoundInitialized) {
        if (sound->soundBuffer != -1) {
            v14 = (sound->field_40 & 0x02) == 0;
        } else {
            gSoundLastError = SOUND_NO_SOUND;
            v14 = true;
        }
    } else {
        gSoundLastError = SOUND_NOT_INITIALIZED;
        v14 = true;
    }

    if (v14) {
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
int _soundFade(Sound* sound, int a2, int a3)
{
    return _internalSoundFade(sound, a2, a3, 0);
}

// 0x4AEB54
void soundDeleteAll()
{
    while (gSoundListHead != NULL) {
        soundDelete(gSoundListHead);
    }
}

// 0x4AEBE0
void soundContinueAll()
{
    Sound* curr = gSoundListHead;
    while (curr != NULL) {
        // Sound can be deallocated in `soundContinue`.
        Sound* next = curr->next;
        soundContinue(curr);
        curr = next;
    }
}

// 0x4AEC00
int soundSetDefaultFileIO(SoundOpenProc* openProc, SoundCloseProc* closeProc, SoundReadProc* readProc, SoundWriteProc* writeProc, SoundSeekProc* seekProc, SoundTellProc* tellProc, SoundFileLengthProc* fileLengthProc)
{
    if (openProc != NULL) {
        gSoundDefaultFileIO.open = openProc;
    }

    if (closeProc != NULL) {
        gSoundDefaultFileIO.close = closeProc;
    }

    if (readProc != NULL) {
        gSoundDefaultFileIO.read = readProc;
    }

    if (writeProc != NULL) {
        gSoundDefaultFileIO.write = writeProc;
    }

    if (seekProc != NULL) {
        gSoundDefaultFileIO.seek = seekProc;
    }

    if (tellProc != NULL) {
        gSoundDefaultFileIO.tell = tellProc;
    }

    if (fileLengthProc != NULL) {
        gSoundDefaultFileIO.filelength = fileLengthProc;
    }

    gSoundLastError = SOUND_NO_ERROR;
    return gSoundLastError;
}
