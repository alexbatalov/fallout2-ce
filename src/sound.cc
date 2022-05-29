#include "sound.h"

#include "debug.h"
#include "memory.h"
#include "platform_compat.h"

#include <io.h>
#include <limits.h>
#include <math.h>
#include <mmsystem.h>
#include <stdlib.h>

#include <algorithm>

// 0x51D478
STRUCT_51D478* _fadeHead = NULL;

// 0x51D47C
STRUCT_51D478* _fadeFreeList = NULL;

// 0x51D480
unsigned int _fadeEventHandle = UINT_MAX;

// 0x51D488
MallocProc* gSoundMallocProc = soundMallocProcDefaultImpl;

// 0x51D48C
ReallocProc* gSoundReallocProc = soundReallocProcDefaultImpl;

// 0x51D490
FreeProc* gSoundFreeProc = soundFreeProcDefaultImpl;

// 0x51D494
SoundFileIO gSoundDefaultFileIO = {
    open,
    close,
    read,
    write,
    lseek,
    tell,
    compat_filelength,
    -1,
};

// 0x51D4B4
SoundFileNameMangler* gSoundFileNameMangler = soundFileManglerDefaultImpl;

// 0x51D4B8
const char* gSoundErrorDescriptions[SOUND_ERR_COUNT] = {
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
    "sound.c: not implemented",
};

// 0x668150
int gSoundLastError;

// 0x668154
int _masterVol;

#ifdef HAVE_DSOUND
// 0x668158
LPDIRECTSOUNDBUFFER gDirectSoundPrimaryBuffer;
#endif

// 0x66815C
int _sampleRate;

// Number of sounds currently playing.
//
// 0x668160
int _numSounds;

// 0x668164
int _deviceInit;

// 0x668168
int _dataSize;

// 0x66816C
int _numBuffers;

// 0x668170
bool gSoundInitialized;

// 0x668174
Sound* gSoundListHead;

#ifdef HAVE_DSOUND
// 0x668178
LPDIRECTSOUND gDirectSound;
#endif

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
#ifdef HAVE_DSOUND
    if (sound->field_3C & 0x80) {
        return;
    }

    DWORD readPos;
    DWORD writePos;
    HRESULT hr = IDirectSoundBuffer_GetCurrentPosition(sound->directSoundBuffer, &readPos, &writePos);
    if (hr != DS_OK) {
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

    VOID* audioPtr1;
    VOID* audioPtr2;
    DWORD audioBytes1;
    DWORD audioBytes2;
    hr = IDirectSoundBuffer_Lock(sound->directSoundBuffer, sound->field_7C * sound->field_70, sound->field_7C * v53, &audioPtr1, &audioBytes1, &audioPtr2, &audioBytes2, 0);
    if (hr == DSERR_BUFFERLOST) {
        IDirectSoundBuffer_Restore(sound->directSoundBuffer);
        hr = IDirectSoundBuffer_Lock(sound->directSoundBuffer, sound->field_7C * sound->field_70, sound->field_7C * v53, &audioPtr1, &audioBytes1, &audioPtr2, &audioBytes2, 0);
    }

    if (hr != DS_OK) {
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

    IDirectSoundBuffer_Unlock(sound->directSoundBuffer, audioPtr1, audioBytes1, audioPtr2, audioBytes2);

    sound->field_70 = v6;

    return;
#endif
}

// 0x4ACC58
int soundInit(int a1, int a2, int a3, int a4, int rate)
{
#ifdef HAVE_DSOUND
    HRESULT hr;
    DWORD v24;

    if (gDirectSoundCreateProc(0, &gDirectSound, 0) != DS_OK) {
        gDirectSound = NULL;
        gSoundLastError = SOUND_SOS_DETECTION_FAILURE;
        return gSoundLastError;
    }

    if (IDirectSound_SetCooperativeLevel(gDirectSound, gProgramWindow, DSSCL_EXCLUSIVE) != DS_OK) {
        gSoundLastError = SOUND_UNKNOWN_ERROR;
        return gSoundLastError;
    }

    _sampleRate = rate;
    _dataSize = a4;
    _numBuffers = a2;
    gSoundInitialized = true;
    _deviceInit = 1;

    DSBUFFERDESC dsbdesc;
    memset(&dsbdesc, 0, sizeof(dsbdesc));
    dsbdesc.dwSize = sizeof(dsbdesc);
    dsbdesc.dwFlags = DSCAPS_PRIMARYMONO;
    dsbdesc.dwBufferBytes = 0;

    hr = IDirectSound_CreateSoundBuffer(gDirectSound, &dsbdesc, &gDirectSoundPrimaryBuffer, NULL);
    if (hr != DS_OK) {
        switch (hr) {
        case DSERR_ALLOCATED:
            debugPrint("%s:%s\n", "CreateSoundBuffer", "DSERR_ALLOCATED");
            break;
        case DSERR_BADFORMAT:
            debugPrint("%s:%s\n", "CreateSoundBuffer", "DSERR_BADFORMAT");
            break;
        case DSERR_INVALIDPARAM:
            debugPrint("%s:%s\n", "CreateSoundBuffer", "DSERR_INVALIDPARAM");
            break;
        case DSERR_NOAGGREGATION:
            debugPrint("%s:%s\n", "CreateSoundBuffer", "DSERR_NOAGGREGATION");
            break;
        case DSERR_OUTOFMEMORY:
            debugPrint("%s:%s\n", "CreateSoundBuffer", "DSERR_OUTOFMEMORY");
            break;
        case DSERR_UNINITIALIZED:
            debugPrint("%s:%s\n", "CreateSoundBuffer", "DSERR_UNINITIALIZED");
            break;
        case DSERR_UNSUPPORTED:
            debugPrint("%s:%s\n", "CreateSoundBuffer", "DSERR_UNSUPPORTED");
            break;
        }

        exit(1);
    }

    WAVEFORMATEX pcmwf;
    memset(&pcmwf, 0, sizeof(pcmwf));

    DSCAPS dscaps;
    memset(&dscaps, 0, sizeof(dscaps));
    dscaps.dwSize = sizeof(dscaps);

    hr = IDirectSound_GetCaps(gDirectSound, &dscaps);
    if (hr != DS_OK) {
        debugPrint("soundInit: Error getting primary buffer parameters\n");
        goto out;
    }

    pcmwf.nSamplesPerSec = rate;
    pcmwf.wFormatTag = WAVE_FORMAT_PCM;

    if (dscaps.dwFlags & DSCAPS_PRIMARY16BIT) {
        pcmwf.wBitsPerSample = 16;
    } else {
        pcmwf.wBitsPerSample = 8;
    }

    pcmwf.nChannels = (dscaps.dwFlags & DSCAPS_PRIMARYSTEREO) ? 2 : 1;
    pcmwf.nBlockAlign = pcmwf.wBitsPerSample * pcmwf.nChannels / 8;
    pcmwf.nSamplesPerSec = rate;
    pcmwf.cbSize = 0;
    pcmwf.nAvgBytesPerSec = pcmwf.nBlockAlign * rate;

    debugPrint("soundInit: Setting primary buffer to: %d bit, %d channels, %d rate\n", pcmwf.wBitsPerSample, pcmwf.nChannels, rate);
    hr = IDirectSoundBuffer_SetFormat(gDirectSoundPrimaryBuffer, &pcmwf);
    if (hr != DS_OK) {
        debugPrint("soundInit: Couldn't change rate to %d\n", rate);

        switch (hr) {
        case DSERR_BADFORMAT:
            debugPrint("%s:%s\n", "SetFormat", "DSERR_BADFORMAT");
            break;
        case DSERR_INVALIDCALL:
            debugPrint("%s:%s\n", "SetFormat", "DSERR_INVALIDCALL");
            break;
        case DSERR_INVALIDPARAM:
            debugPrint("%s:%s\n", "SetFormat", "DSERR_INVALIDPARAM");
            break;
        case DSERR_OUTOFMEMORY:
            debugPrint("%s:%s\n", "SetFormat", "DSERR_OUTOFMEMORY");
            break;
        case DSERR_PRIOLEVELNEEDED:
            debugPrint("%s:%s\n", "SetFormat", "DSERR_PRIOLEVELNEEDED");
            break;
        case DSERR_UNSUPPORTED:
            debugPrint("%s:%s\n", "SetFormat", "DSERR_UNSUPPORTED");
            break;
        }

        goto out;
    }

    hr = IDirectSoundBuffer_GetFormat(gDirectSoundPrimaryBuffer, &pcmwf, sizeof(WAVEFORMATEX), &v24);
    if (hr != DS_OK) {
        debugPrint("soundInit: Couldn't read new settings\n");
        goto out;
    }

    debugPrint("soundInit: Primary buffer settings set to: %d bit, %d channels, %d rate\n", pcmwf.wBitsPerSample, pcmwf.nChannels, pcmwf.nSamplesPerSec);

    if (dscaps.dwFlags & DSCAPS_EMULDRIVER) {
        debugPrint("soundInit: using DirectSound emulated drivers\n");
    }

out:

    _soundSetMasterVolume(VOLUME_MAX);
    gSoundLastError = SOUND_NO_ERROR;

    return 0;
#else
    gSoundLastError = SOUND_NOT_IMPLEMENTED;
    return gSoundLastError;
#endif
}

// 0x4AD04C
void soundExit()
{
#ifdef HAVE_DSOUND
    while (gSoundListHead != NULL) {
        Sound* next = gSoundListHead->next;
        soundDelete(gSoundListHead);
        gSoundListHead = next;
    }

    if (_fadeEventHandle != -1) {
        _removeTimedEvent(&_fadeEventHandle);
    }

    while (_fadeFreeList != NULL) {
        STRUCT_51D478* next = _fadeFreeList->next;
        gSoundFreeProc(_fadeFreeList);
        _fadeFreeList = next;
    }

    if (gDirectSoundPrimaryBuffer != NULL) {
        IDirectSoundBuffer_Release(gDirectSoundPrimaryBuffer);
        gDirectSoundPrimaryBuffer = NULL;
    }

    if (gDirectSound != NULL) {
        IDirectSound_Release(gDirectSound);
        gDirectSound = NULL;
    }

    gSoundLastError = SOUND_NO_ERROR;
    gSoundInitialized = false;
#endif
}

// 0x4AD0FC
Sound* soundAllocate(int a1, int a2)
{
#ifdef HAVE_DSOUND
    if (!gSoundInitialized) {
        gSoundLastError = SOUND_NOT_INITIALIZED;
        return NULL;
    }

    Sound* sound = (Sound*)gSoundMallocProc(sizeof(*sound));
    memset(sound, 0, sizeof(*sound));

    memcpy(&(sound->io), &gSoundDefaultFileIO, sizeof(gSoundDefaultFileIO));

    WAVEFORMATEX* wfxFormat = (WAVEFORMATEX*)gSoundMallocProc(sizeof(*wfxFormat));
    memset(wfxFormat, 0, sizeof(*wfxFormat));

    wfxFormat->wFormatTag = 1;
    wfxFormat->nChannels = 1;

    if (a2 & 0x08) {
        wfxFormat->wBitsPerSample = 16;
    } else {
        wfxFormat->wBitsPerSample = 8;
    }

    if (!(a2 & 0x02)) {
        a2 |= 0x02;
    }

    wfxFormat->nSamplesPerSec = _sampleRate;
    wfxFormat->nBlockAlign = wfxFormat->nChannels * (wfxFormat->wBitsPerSample / 8);
    wfxFormat->cbSize = 0;
    wfxFormat->nAvgBytesPerSec = wfxFormat->nBlockAlign * wfxFormat->nSamplesPerSec;

    sound->field_3C = a2;
    sound->field_44 = a1;
    sound->field_7C = _dataSize;
    sound->field_64 = 0;
    sound->directSoundBuffer = 0;
    sound->field_40 = 0;
    sound->directSoundBufferDescription.dwSize = sizeof(DSBUFFERDESC);
    sound->directSoundBufferDescription.dwFlags = DSBCAPS_GETCURRENTPOSITION2;
    sound->field_78 = _numBuffers;
    sound->readLimit = sound->field_7C * _numBuffers;

    if (a2 & 0x2) {
        sound->directSoundBufferDescription.dwFlags |= DSBCAPS_CTRLVOLUME;
    }

    if (a2 & 0x4) {
        sound->directSoundBufferDescription.dwFlags |= DSBCAPS_CTRLPAN;
    }

    if (a2 & 0x40) {
        sound->directSoundBufferDescription.dwFlags |= DSBCAPS_CTRLFREQUENCY;
    }

    sound->directSoundBufferDescription.lpwfxFormat = wfxFormat;

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
#else
    gSoundLastError = SOUND_NOT_IMPLEMENTED;
    return NULL;
#endif
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
#ifdef HAVE_DSOUND
    HRESULT hr;

    if (!gSoundInitialized) {
        gSoundLastError = SOUND_NOT_INITIALIZED;
        return gSoundLastError;
    }

    if (sound == NULL || sound->directSoundBuffer == NULL) {
        gSoundLastError = SOUND_NO_SOUND;
        return gSoundLastError;
    }

    if (sound->field_44 & 0x02) {
        sound->io.seek(sound->io.fd, 0, SEEK_SET);
        sound->field_70 = 0;
        sound->field_74 = 0;
        sound->field_64 = 0;
        sound->field_3C &= 0xFD7F;
        hr = IDirectSoundBuffer_SetCurrentPosition(sound->directSoundBuffer, 0);
        _preloadBuffers(sound);
    } else {
        hr = IDirectSoundBuffer_SetCurrentPosition(sound->directSoundBuffer, 0);
    }

    if (hr != DS_OK) {
        gSoundLastError = SOUND_UNKNOWN_ERROR;
        return gSoundLastError;
    }

    sound->field_40 &= ~(0x01);

    gSoundLastError = SOUND_NO_ERROR;
    return gSoundLastError;
#else
    gSoundLastError = SOUND_NOT_IMPLEMENTED;
    return gSoundLastError;
#endif
}

// 0x4AD5C8
int _addSoundData(Sound* sound, unsigned char* buf, int size)
{
#ifdef HAVE_DSOUND
    HRESULT hr;
    void* audio_ptr_1;
    DWORD audio_bytes_1;
    void* audio_ptr_2;
    DWORD audio_bytes_2;

    hr = IDirectSoundBuffer_Lock(sound->directSoundBuffer, 0, size, &audio_ptr_1, &audio_bytes_1, &audio_ptr_2, &audio_bytes_2, DSBLOCK_FROMWRITECURSOR);
    if (hr == DSERR_BUFFERLOST) {
        IDirectSoundBuffer_Restore(sound->directSoundBuffer);
        hr = IDirectSoundBuffer_Lock(sound->directSoundBuffer, 0, size, &audio_ptr_1, &audio_bytes_1, &audio_ptr_2, &audio_bytes_2, DSBLOCK_FROMWRITECURSOR);
    }

    if (hr != DS_OK) {
        gSoundLastError = SOUND_UNKNOWN_ERROR;
        return gSoundLastError;
    }

    memcpy(audio_ptr_1, buf, audio_bytes_1);

    if (audio_ptr_2 != NULL) {
        memcpy(audio_ptr_2, buf + audio_bytes_1, audio_bytes_2);
    }

    hr = IDirectSoundBuffer_Unlock(sound->directSoundBuffer, audio_ptr_1, audio_bytes_1, audio_ptr_2, audio_bytes_2);
    if (hr != DS_OK) {
        gSoundLastError = SOUND_UNKNOWN_ERROR;
        return gSoundLastError;
    }

    gSoundLastError = SOUND_NO_ERROR;
    return gSoundLastError;
#else
    gSoundLastError = SOUND_NOT_IMPLEMENTED;
    return gSoundLastError;
#endif
}

// 0x4AD6C0
int _soundSetData(Sound* sound, unsigned char* buf, int size)
{
#ifdef HAVE_DSOUND
    if (!gSoundInitialized) {
        gSoundLastError = SOUND_NOT_INITIALIZED;
        return gSoundLastError;
    }

    if (sound == NULL) {
        gSoundLastError = SOUND_NO_SOUND;
        return gSoundLastError;
    }

    if (sound->directSoundBuffer == NULL) {
        sound->directSoundBufferDescription.dwBufferBytes = size;

        if (IDirectSound_CreateSoundBuffer(gDirectSound, &(sound->directSoundBufferDescription), &(sound->directSoundBuffer), NULL) != DS_OK) {
            gSoundLastError = SOUND_UNKNOWN_ERROR;
            return gSoundLastError;
        }
    }

    return _addSoundData(sound, buf, size);
#else
    gSoundLastError = SOUND_NOT_IMPLEMENTED;
    return gSoundLastError;
#endif
}

// 0x4AD73C
int soundPlay(Sound* sound)
{
#ifdef HAVE_DSOUND
    HRESULT hr;
    DWORD readPos;
    DWORD writePos;

    if (!gSoundInitialized) {
        gSoundLastError = SOUND_NOT_INITIALIZED;
        return gSoundLastError;
    }

    if (sound == NULL || sound->directSoundBuffer == NULL) {
        gSoundLastError = SOUND_NO_SOUND;
        return gSoundLastError;
    }

    // TODO: Check.
    if (sound->field_40 & 0x01) {
        _soundRewind(sound);
    }

    soundSetVolume(sound, sound->volume);

    hr = IDirectSoundBuffer_Play(sound->directSoundBuffer, 0, 0, sound->field_3C & 0x20 ? DSBPLAY_LOOPING : 0);

    IDirectSoundBuffer_GetCurrentPosition(sound->directSoundBuffer, &readPos, &writePos);
    sound->field_70 = readPos / sound->field_7C;

    if (hr != DS_OK) {
        gSoundLastError = SOUND_UNKNOWN_ERROR;
        return gSoundLastError;
    }

    sound->field_40 |= SOUND_FLAG_SOUND_IS_PLAYING;

    ++_numSounds;

    gSoundLastError = SOUND_NO_ERROR;
    return gSoundLastError;
#else
    gSoundLastError = SOUND_NOT_IMPLEMENTED;
    return gSoundLastError;
#endif
}

// 0x4AD828
int soundStop(Sound* sound)
{
#ifdef HAVE_DSOUND
    HRESULT hr;

    if (!gSoundInitialized) {
        gSoundLastError = SOUND_NOT_INITIALIZED;
        return gSoundLastError;
    }

    if (sound == NULL || sound->directSoundBuffer == NULL) {
        gSoundLastError = SOUND_NO_SOUND;
        return gSoundLastError;
    }

    if (!(sound->field_40 & SOUND_FLAG_SOUND_IS_PLAYING)) {
        gSoundLastError = SOUND_NOT_PLAYING;
        return gSoundLastError;
    }

    hr = IDirectSoundBuffer_Stop(sound->directSoundBuffer);
    if (hr != DS_OK) {
        gSoundLastError = SOUND_UNKNOWN_ERROR;
        return gSoundLastError;
    }

    sound->field_40 &= ~SOUND_FLAG_SOUND_IS_PLAYING;
    _numSounds--;

    gSoundLastError = SOUND_NO_ERROR;
    return gSoundLastError;
#else
    gSoundLastError = SOUND_NOT_IMPLEMENTED;
    return gSoundLastError;
#endif
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
#ifdef HAVE_DSOUND
    HRESULT hr;
    DWORD status;

    if (!gSoundInitialized) {
        gSoundLastError = SOUND_NOT_INITIALIZED;
        return gSoundLastError;
    }

    if (sound == NULL) {
        gSoundLastError = SOUND_NO_SOUND;
        return gSoundLastError;
    }

    if (sound->directSoundBuffer == NULL) {
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

    hr = IDirectSoundBuffer_GetStatus(sound->directSoundBuffer, &status);
    if (hr != DS_OK) {
        debugPrint("Error in soundContinue, %x\n", hr);

        gSoundLastError = SOUND_UNKNOWN_ERROR;
        return gSoundLastError;
    }

    if (!(sound->field_3C & 0x80) && (status & (DSBSTATUS_PLAYING | DSBSTATUS_LOOPING))) {
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
#else
    gSoundLastError = SOUND_NOT_IMPLEMENTED;
    return gSoundLastError;
#endif
}

// 0x4ADA84
bool soundIsPlaying(Sound* sound)
{
#ifdef HAVE_DSOUND
    if (!gSoundInitialized) {
        gSoundLastError = SOUND_NOT_INITIALIZED;
        return false;
    }

    if (sound == NULL || sound->directSoundBuffer == 0) {
        gSoundLastError = SOUND_NO_SOUND;
        return false;
    }

    return (sound->field_40 & SOUND_FLAG_SOUND_IS_PLAYING) != 0;
#else
    gSoundLastError = SOUND_NOT_IMPLEMENTED;
    return false;
#endif
}

// 0x4ADAC4
bool _soundDone(Sound* sound)
{
#ifdef HAVE_DSOUND
    if (!gSoundInitialized) {
        gSoundLastError = SOUND_NOT_INITIALIZED;
        return false;
    }

    if (sound == NULL || sound->directSoundBuffer == 0) {
        gSoundLastError = SOUND_NO_SOUND;
        return false;
    }

    return sound->field_40 & 1;
#else
    gSoundLastError = SOUND_NOT_IMPLEMENTED;
    return false;
#endif
}

// 0x4ADB44
bool soundIsPaused(Sound* sound)
{
#ifdef HAVE_DSOUND
    if (!gSoundInitialized) {
        gSoundLastError = SOUND_NOT_INITIALIZED;
        return false;
    }

    if (sound == NULL || sound->directSoundBuffer == NULL) {
        gSoundLastError = SOUND_NO_SOUND;
        return false;
    }

    return (sound->field_40 & SOUND_FLAG_SOUND_IS_PAUSED) != 0;
#else
    gSoundLastError = SOUND_NOT_IMPLEMENTED;
    return false;
#endif
}

// 0x4ADBC4
int _soundType(Sound* sound, int a2)
{
#ifdef HAVE_DSOUND
    if (!gSoundInitialized) {
        gSoundLastError = SOUND_NOT_INITIALIZED;
        return 0;
    }

    if (sound == NULL || sound->directSoundBuffer == NULL) {
        gSoundLastError = SOUND_NO_SOUND;
        return 0;
    }

    return sound->field_44 & a2;
#else
    gSoundLastError = SOUND_NOT_IMPLEMENTED;
    return 0;
#endif
}

// 0x4ADC04
int soundGetDuration(Sound* sound)
{
#ifdef HAVE_DSOUND
    if (!gSoundInitialized) {
        gSoundLastError = SOUND_NOT_INITIALIZED;
        return gSoundLastError;
    }

    if (sound == NULL || sound->directSoundBuffer == NULL) {
        gSoundLastError = SOUND_NO_SOUND;
        return gSoundLastError;
    }

    int bytesPerSec = sound->directSoundBufferDescription.lpwfxFormat->nAvgBytesPerSec;
    int v3 = sound->field_60;
    int v4 = v3 % bytesPerSec;
    int result = v3 / bytesPerSec;
    if (v4 != 0) {
        result += 1;
    }

    return result;
#else
    gSoundLastError = SOUND_NOT_IMPLEMENTED;
    return gSoundLastError;
#endif
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

// Normalize volume?
//
// 0x4ADD68
int _soundVolumeHMItoDirectSound(int volume)
{
    double normalizedVolume;

    if (volume > VOLUME_MAX) {
        volume = VOLUME_MAX;
    }

    if (volume != 0) {
        normalizedVolume = -1000.0 * log2(32767.0 / volume);
        normalizedVolume = std::clamp(normalizedVolume, -10000.0, 0.0);
    } else {
        normalizedVolume = -10000.0;
    }

    return (int)normalizedVolume;
}

// 0x4ADE0C
int soundSetVolume(Sound* sound, int volume)
{
#ifdef HAVE_DSOUND
    int normalizedVolume;
    HRESULT hr;

    if (!gSoundInitialized) {
        gSoundLastError = SOUND_NOT_INITIALIZED;
        return gSoundLastError;
    }

    if (sound == NULL) {
        gSoundLastError = SOUND_NO_SOUND;
        return gSoundLastError;
    }

    sound->volume = volume;

    if (sound->directSoundBuffer == NULL) {
        gSoundLastError = SOUND_NO_ERROR;
        return gSoundLastError;
    }

    normalizedVolume = _soundVolumeHMItoDirectSound(_masterVol * volume / VOLUME_MAX);

    hr = IDirectSoundBuffer_SetVolume(sound->directSoundBuffer, normalizedVolume);
    if (hr != DS_OK) {
        gSoundLastError = SOUND_UNKNOWN_ERROR;
        return gSoundLastError;
    }

    gSoundLastError = SOUND_NO_ERROR;
    return gSoundLastError;
#else
    gSoundLastError = SOUND_NOT_IMPLEMENTED;
    return gSoundLastError;
#endif
}

// 0x4ADE80
int _soundGetVolume(Sound* sound)
{
#ifdef HAVE_DSOUND
    long volume;
    int v13;
    int v8;
    int diff;

    if (!_deviceInit) {
        gSoundLastError = SOUND_NOT_INITIALIZED;
        return gSoundLastError;
    }

    if (sound == NULL || sound->directSoundBuffer == NULL) {
        gSoundLastError = SOUND_NO_SOUND;
        return gSoundLastError;
    }

    IDirectSoundBuffer_GetVolume(sound->directSoundBuffer, &volume);

    if (volume == -10000) {
        v13 = 0;
    } else {
        // TODO: Check.
        volume = -volume;
        v13 = (int)(32767.0 / pow(2.0, (volume * 0.001)));
    }

    v8 = VOLUME_MAX * v13 / _masterVol;
    diff = abs(v8 - sound->volume);
    if (diff > 20) {
        debugPrint("Actual sound volume differs significantly from noted volume actual %x stored %x, diff %d.", v8, sound->volume, diff);
    }

    return sound->volume;
#else
    gSoundLastError = SOUND_NOT_IMPLEMENTED;
    return gSoundLastError;
#endif
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
#ifdef HAVE_DSOUND
    LPWAVEFORMATEX format;

    if (!gSoundInitialized) {
        gSoundLastError = SOUND_NOT_INITIALIZED;
        return gSoundLastError;
    }

    if (sound == NULL) {
        gSoundLastError = SOUND_NO_SOUND;
        return gSoundLastError;
    }

    if (channels == 3) {
        format = sound->directSoundBufferDescription.lpwfxFormat;

        format->nBlockAlign = (2 * format->wBitsPerSample) / 8;
        format->nChannels = 2;
        format->nAvgBytesPerSec = format->nBlockAlign * _sampleRate;
    }

    gSoundLastError = SOUND_NO_ERROR;
    return gSoundLastError;
#else
    gSoundLastError = SOUND_NOT_IMPLEMENTED;
    return gSoundLastError;
#endif
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
#ifdef HAVE_DSOUND
    HRESULT hr;
    DWORD readPos;
    DWORD writePos;

    if (!gSoundInitialized) {
        gSoundLastError = SOUND_NOT_INITIALIZED;
        return gSoundLastError;
    }

    if (sound == NULL) {
        gSoundLastError = SOUND_NO_SOUND;
        return gSoundLastError;
    }

    if (sound->directSoundBuffer == NULL) {
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

    hr = IDirectSoundBuffer_GetCurrentPosition(sound->directSoundBuffer, &readPos, &writePos);
    if (hr != DS_OK) {
        gSoundLastError = SOUND_UNKNOWN_ERROR;
        return gSoundLastError;
    }

    sound->field_48 = readPos;
    sound->field_40 |= SOUND_FLAG_SOUND_IS_PAUSED;

    return soundStop(sound);
#else
    gSoundLastError = SOUND_NOT_IMPLEMENTED;
    return gSoundLastError;
#endif
}

// TODO: Check, looks like it uses couple of inlined functions.
//
// 0x4AE1F0
int soundResume(Sound* sound)
{
#ifdef HAVE_DSOUND
    HRESULT hr;

    if (!gSoundInitialized) {
        gSoundLastError = SOUND_NOT_INITIALIZED;
        return gSoundLastError;
    }

    if (sound == NULL || sound->directSoundBuffer == NULL) {
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

    hr = IDirectSoundBuffer_SetCurrentPosition(sound->directSoundBuffer, sound->field_48);
    if (hr != DS_OK) {
        gSoundLastError = SOUND_UNKNOWN_ERROR;
        return gSoundLastError;
    }

    sound->field_40 &= ~SOUND_FLAG_SOUND_IS_PAUSED;
    sound->field_48 = 0;

    return soundPlay(sound);
#else
    gSoundLastError = SOUND_NOT_IMPLEMENTED;
    return gSoundLastError;
#endif
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
#ifdef HAVE_DSOUND
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

    if (sound->directSoundBuffer != NULL) {
        // NOTE: Uninline.
        if (!soundIsPlaying(sound)) {
            soundStop(sound);
        }

        if (sound->callback != NULL) {
            sound->callback(sound->callbackUserData, 1);
        }

        IDirectSoundBuffer_Release(sound->directSoundBuffer);
        sound->directSoundBuffer = NULL;
    }

    if (sound->field_90 != NULL) {
        sound->field_90(sound->field_8C);
    }

    if (sound->field_20 != NULL) {
        gSoundFreeProc(sound->field_20);
        sound->field_20 = NULL;
    }

    if (sound->directSoundBufferDescription.lpwfxFormat != NULL) {
        gSoundFreeProc(sound->directSoundBufferDescription.lpwfxFormat);
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
#endif
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
void CALLBACK _doTimerEvent(UINT uTimerID, UINT uMsg, DWORD_PTR dwUser, DWORD_PTR dw1, DWORD_PTR dw2)
{
    void (*fn)();

    if (dwUser != 0) {
        fn = (void (*)())dwUser;
        fn();
    }
}

// 0x4AE614
void _removeTimedEvent(unsigned int* timerId)
{
    if (*timerId != -1) {
        timeKillEvent(*timerId);
        *timerId = -1;
    }
}

// 0x4AE634
int _soundGetPosition(Sound* sound)
{
#ifdef HAVE_DSOUND
    if (!gSoundInitialized) {
        gSoundLastError = SOUND_NOT_INITIALIZED;
        return gSoundLastError;
    }

    if (sound == NULL) {
        gSoundLastError = SOUND_NO_SOUND;
        return gSoundLastError;
    }

    DWORD playPos;
    DWORD writePos;
    IDirectSoundBuffer_GetCurrentPosition(sound->directSoundBuffer, &playPos, &writePos);

    if ((sound->field_44 & 0x02) != 0) {
        if (playPos < sound->field_74) {
            playPos += sound->field_64 + sound->field_78 * sound->field_7C - sound->field_74;
        } else {
            playPos -= sound->field_74 + sound->field_64;
        }
    }

    return playPos;
#else
    gSoundLastError = SOUND_NOT_IMPLEMENTED;
    return gSoundLastError;
#endif
}

// 0x4AE6CC
int _soundSetPosition(Sound* sound, int a2)
{
#ifdef HAVE_DSOUND
    if (!gSoundInitialized) {
        gSoundLastError = SOUND_NOT_INITIALIZED;
        return gSoundLastError;
    }

    if (sound == NULL) {
        gSoundLastError = SOUND_NO_SOUND;
        return gSoundLastError;
    }

    if (sound->directSoundBuffer == NULL) {
        gSoundLastError = SOUND_NO_SOUND;
        return gSoundLastError;
    }

    if (sound->field_44 & 0x02) {
        int v6 = a2 / sound->field_7C % sound->field_78;

        IDirectSoundBuffer_SetCurrentPosition(sound->directSoundBuffer, v6 * sound->field_7C + a2 % sound->field_7C);

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
        IDirectSoundBuffer_SetCurrentPosition(sound->directSoundBuffer, a2);
    }

    gSoundLastError = SOUND_NO_ERROR;
    return gSoundLastError;
#else
    gSoundLastError = SOUND_NOT_IMPLEMENTED;
    return gSoundLastError;
#endif
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

    if (_fadeHead == NULL && _fadeEventHandle != -1) {
        timeKillEvent(_fadeEventHandle);
        _fadeEventHandle = -1;
    }
}

// 0x4AE988
int _internalSoundFade(Sound* sound, int a2, int a3, int a4)
{
#ifdef HAVE_DSOUND
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
        if (sound->directSoundBuffer != NULL) {
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

    if (_fadeEventHandle != -1) {
        gSoundLastError = SOUND_NO_ERROR;
        return gSoundLastError;
    }

    _fadeEventHandle = timeSetEvent(40, 10, _doTimerEvent, (DWORD_PTR)_fadeSounds, 1);
    if (_fadeEventHandle == 0) {
        gSoundLastError = SOUND_UNKNOWN_ERROR;
        return gSoundLastError;
    }

    gSoundLastError = SOUND_NO_ERROR;
    return gSoundLastError;
#else
    gSoundLastError = SOUND_NOT_IMPLEMENTED;
    return gSoundLastError;
#endif
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
