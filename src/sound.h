#ifndef SOUND_H
#define SOUND_H

#include "memory_defs.h"
#include "win32.h"

#define SOUND_FLAG_SOUND_IS_PLAYING (0x02)
#define SOUND_FLAG_SOUND_IS_PAUSED (0x08)

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
    // TODO: Remove once DirectX -> SDL transition is completed.
    SOUND_NOT_IMPLEMENTED = 33,
    SOUND_ERR_COUNT,
} SoundError;

typedef char*(SoundFileNameMangler)(char*);
typedef int SoundOpenProc(const char* filePath, int flags, ...);
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

typedef void SoundCallback(void* userData, int a2);

typedef struct Sound {
    SoundFileIO io;
    unsigned char* field_20;
#ifdef HAVE_DSOUND
    LPDIRECTSOUNDBUFFER directSoundBuffer;
    DSBUFFERDESC directSoundBufferDescription;
#endif
    int field_3C;
    // flags
    int field_40;
    int field_44;
    // pause pos
    int field_48;
    int volume;
    int field_50;
    int field_54;
    int field_58;
    int field_5C;
    // file size
    int field_60;
    int field_64;
    int field_68;
    int readLimit;
    int field_70;
#ifdef HAVE_DSOUND
    DWORD field_74;
#endif
    int field_78;
    int field_7C;
    int field_80;
    // callback data
    void* callbackUserData;
    SoundCallback* callback;
    int field_8C;
    void (*field_90)(int);
    struct Sound* next;
    struct Sound* prev;
} Sound;

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

extern STRUCT_51D478* _fadeHead;
extern STRUCT_51D478* _fadeFreeList;

extern unsigned int _fadeEventHandle;
extern MallocProc* gSoundMallocProc;
extern ReallocProc* gSoundReallocProc;
extern FreeProc* gSoundFreeProc;
extern SoundFileIO gSoundDefaultFileIO;
extern SoundFileNameMangler* gSoundFileNameMangler;
extern const char* gSoundErrorDescriptions[SOUND_ERR_COUNT];

extern int gSoundLastError;
extern int _masterVol;
#ifdef HAVE_DSOUND
extern LPDIRECTSOUNDBUFFER gDirectSoundPrimaryBuffer;
#endif
extern int _sampleRate;
extern int _numSounds;
extern int _deviceInit;
extern int _dataSize;
extern int _numBuffers;
extern bool gSoundInitialized;
extern Sound* gSoundListHead;
#ifdef HAVE_DSOUND
extern LPDIRECTSOUND gDirectSound;
#endif

void* soundMallocProcDefaultImpl(size_t size);
void* soundReallocProcDefaultImpl(void* ptr, size_t size);
void soundFreeProcDefaultImpl(void* ptr);
void soundSetMemoryProcs(MallocProc* mallocProc, ReallocProc* reallocProc, FreeProc* freeProc);

char* soundFileManglerDefaultImpl(char* fname);
const char* soundGetErrorDescription(int err);
void _refreshSoundBuffers(Sound* sound);
int soundInit(int a1, int a2, int a3, int a4, int rate);
void soundExit();
Sound* soundAllocate(int a1, int a2);
int _preloadBuffers(Sound* sound);
int soundLoad(Sound* sound, char* filePath);
int _soundRewind(Sound* sound);
int _addSoundData(Sound* sound, unsigned char* buf, int size);
int _soundSetData(Sound* sound, unsigned char* buf, int size);
int soundPlay(Sound* sound);
int soundStop(Sound* sound);
int soundDelete(Sound* sound);
int soundContinue(Sound* sound);
bool soundIsPlaying(Sound* sound);
bool _soundDone(Sound* sound);
bool soundIsPaused(Sound* sound);
int _soundType(Sound* sound, int a2);
int soundGetDuration(Sound* sound);
int soundSetLooping(Sound* sound, int a2);
int _soundVolumeHMItoDirectSound(int a1);
int soundSetVolume(Sound* sound, int volume);
int _soundGetVolume(Sound* sound);
int soundSetCallback(Sound* sound, SoundCallback* callback, void* userData);
int soundSetChannels(Sound* sound, int channels);
int soundSetReadLimit(Sound* sound, int readLimit);
int soundPause(Sound* sound);
int soundResume(Sound* sound);
int soundSetFileIO(Sound* sound, SoundOpenProc* openProc, SoundCloseProc* closeProc, SoundReadProc* readProc, SoundWriteProc* writeProc, SoundSeekProc* seekProc, SoundTellProc* tellProc, SoundFileLengthProc* fileLengthProc);
void soundDeleteInternal(Sound* sound);
int _soundSetMasterVolume(int value);
#ifdef HAVE_DSOUND
void CALLBACK _doTimerEvent(UINT uTimerID, UINT uMsg, DWORD_PTR dwUser, DWORD_PTR dw1, DWORD_PTR dw2);
void _removeTimedEvent(unsigned int* timerId);
#endif
int _soundGetPosition(Sound* sound);
int _soundSetPosition(Sound* sound, int a2);
void _removeFadeSound(STRUCT_51D478* a1);
void _fadeSounds();
int _internalSoundFade(Sound* sound, int a2, int a3, int a4);
int _soundFade(Sound* sound, int a2, int a3);
void soundDeleteAll();
void soundContinueAll();
int soundSetDefaultFileIO(SoundOpenProc* openProc, SoundCloseProc* closeProc, SoundReadProc* readProc, SoundWriteProc* writeProc, SoundSeekProc* seekProc, SoundTellProc* tellProc, SoundFileLengthProc* fileLengthProc);

#endif /* SOUND_H */
