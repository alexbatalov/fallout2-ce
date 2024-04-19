#include "game_sound.h"

#include <stdio.h>
#include <string.h>

#include "animation.h"
#include "art.h"
#include "audio.h"
#include "audio_file.h"
#include "combat.h"
#include "debug.h"
#include "game_config.h"
#include "input.h"
#include "item.h"
#include "map.h"
#include "memory.h"
#include "movie.h"
#include "object.h"
#include "pointer_registry.h"
#include "proto.h"
#include "queue.h"
#include "random.h"
#include "settings.h"
#include "sound_effects_cache.h"
#include "stat.h"
#include "svga.h"
#include "window_manager.h"
#include "worldmap.h"

namespace fallout {

typedef enum SoundEffectActionType {
    SOUND_EFFECT_ACTION_TYPE_ACTIVE,
    SOUND_EFFECT_ACTION_TYPE_PASSIVE,
} SoundEffectActionType;

// 0x5035BC
static char _aSoundSfx[] = "sound\\sfx\\";

// 0x5035C8
static char _aSoundMusic_0[] = "sound\\music\\";

// 0x5035D8
static char _aSoundSpeech_0[] = "sound\\speech\\";

// 0x518E30
static bool gGameSoundInitialized = false;

// 0x518E34
static bool gGameSoundDebugEnabled = false;

// 0x518E38
static bool gMusicEnabled = false;

// 0x518E3C
static int _gsound_background_df_vol = 0;

// 0x518E40
static int _gsound_background_fade = 0;

// 0x518E44
static bool gSpeechEnabled = false;

// 0x518E48
static bool gSoundEffectsEnabled = false;

// number of active effects (max 4)
static int _gsound_active_effect_counter;

// 0x518E50
static Sound* gBackgroundSound = nullptr;

// 0x518E54
static Sound* gSpeechSound = nullptr;

// 0x518E58
static SoundEndCallback* gBackgroundSoundEndCallback = nullptr;

// 0x518E5C
static SoundEndCallback* gSpeechEndCallback = nullptr;

// 0x518E60
static char _snd_lookup_weapon_type[WEAPON_SOUND_EFFECT_COUNT] = {
    'R', // Ready
    'A', // Attack
    'O', // Out of ammo
    'F', // Firing
    'H', // Hit
};

// 0x518E65
static char _snd_lookup_scenery_action[SCENERY_SOUND_EFFECT_COUNT] = {
    'O', // Open
    'C', // Close
    'L', // Lock
    'N', // Unlock
    'U', // Use
};

// 0x518E6C
static int _background_storage_requested = -1;

// 0x518E70
static int _background_loop_requested = -1;

// 0x518E74
static char* _sound_sfx_path = _aSoundSfx;

// 0x518E78
static char* _sound_music_path1 = _aSoundMusic_0;

// 0x518E7C
static char* _sound_music_path2 = _aSoundMusic_0;

// 0x518E80
static char* _sound_speech_path = _aSoundSpeech_0;

// 0x518E84
static int gMasterVolume = VOLUME_MAX;

// 0x518E88
int gMusicVolume = VOLUME_MAX;

// 0x518E8C
static int gSpeechVolume = VOLUME_MAX;

// 0x518E90
static int gSoundEffectsVolume = VOLUME_MAX;

// 0x518E94
static int _detectDevices = -1;

// 0x518E98
static int _lastTime_1 = 0;

// 0x596EB0
static char _background_fname_copied[COMPAT_MAX_PATH];

// 0x596FB5
static char _sfx_file_name[13];

// NOTE: I'm mot sure about it's size. Why not MAX_PATH?
//
// 0x596FC2
static char gBackgroundSoundFileName[270];

static void soundEffectsEnable();
static void soundEffectsDisable();
static int soundEffectsIsEnabled();
static int soundEffectsGetVolume();
static void backgroundSoundDisable();
static void backgroundSoundEnable();
static int backgroundSoundGetDuration();
static void speechDisable();
static void speechEnable();
static int _gsound_speech_volume_get_set(int volume);
static void speechPause();
static void speechResume();
static void _gsound_bkg_proc();
static int gameSoundFileOpen(const char* fname, int* sampleRate);
static long _gsound_write_();
static long gameSoundFileTellNotImplemented(int handle);
static int gameSoundFileWrite(int handle, const void* buf, unsigned int size);
static int gameSoundFileClose(int handle);
static int gameSoundFileRead(int handle, void* buf, unsigned int size);
static long gameSoundFileSeek(int handle, long offset, int origin);
static long gameSoundFileTell(int handle);
static long gameSoundFileGetSize(int handle);
static bool gameSoundIsCompressed(char* filePath);
static void speechCallback(void* userData, int a2);
static void backgroundSoundCallback(void* userData, int a2);
static void soundEffectCallback(void* userData, int a2);
static int _gsound_background_allocate(Sound** out_s, int a2, int a3);
static int gameSoundFindBackgroundSoundPathWithCopy(char* dest, const char* src);
static int gameSoundFindBackgroundSoundPath(char* dest, const char* src);
static int gameSoundFindSpeechSoundPath(char* dest, const char* src);
static void gameSoundDeleteOldMusicFile();
static int backgroundSoundPlay();
static int speechPlay();
static int _gsound_get_music_path(char** out_value, const char* key);
static Sound* _gsound_get_sound_ready_for_effect();
static bool _gsound_file_exists_f(const char* fname);
static int _gsound_setup_paths();

// 0x44FC70
int gameSoundInit()
{
    if (gGameSoundInitialized) {
        if (gGameSoundDebugEnabled) {
            debugPrint("Trying to initialize gsound twice.\n");
        }
        return -1;
    }

    if (!settings.sound.initialize) {
        return 0;
    }

    gGameSoundDebugEnabled = settings.sound.debug;

    if (gGameSoundDebugEnabled) {
        debugPrint("Initializing sound system...");
    }

    if (_gsound_get_music_path(&_sound_music_path1, GAME_CONFIG_MUSIC_PATH1_KEY) != 0) {
        return -1;
    }

    if (_gsound_get_music_path(&_sound_music_path2, GAME_CONFIG_MUSIC_PATH2_KEY) != 0) {
        return -1;
    }

    if (strlen(_sound_music_path1) > 247 || strlen(_sound_music_path2) > 247) {
        if (gGameSoundDebugEnabled) {
            debugPrint("Music paths way too long.\n");
        }
        return -1;
    }

    // gsound_setup_paths
    if (_gsound_setup_paths() != 0) {
        return -1;
    }

    soundSetMemoryProcs(internal_malloc, internal_realloc, internal_free);

    // initialize direct sound
    if (soundInit(_detectDevices, 24, 0x8000, 0x8000, 22050) != 0) {
        if (gGameSoundDebugEnabled) {
            debugPrint("failed!\n");
        }

        return -1;
    }

    if (gGameSoundDebugEnabled) {
        debugPrint("success.\n");
    }

    audioFileInit(gameSoundIsCompressed);
    audioInit(gameSoundIsCompressed);

    int cacheSize = settings.sound.cache_size;
    if (cacheSize >= 0x40000) {
        debugPrint("\n!!! Config file needs adustment.  Please remove the ");
        debugPrint("cache_size line and run fallout again.  This will reset ");
        debugPrint("cache_size to the new default, which is expressed in K.\n");
        return -1;
    }

    if (soundEffectsCacheInit(cacheSize << 10, _sound_sfx_path) != 0) {
        if (gGameSoundDebugEnabled) {
            debugPrint("Unable to initialize sound effects cache.\n");
        }
    }

    if (soundSetDefaultFileIO(gameSoundFileOpen, gameSoundFileClose, gameSoundFileRead, gameSoundFileWrite, gameSoundFileSeek, gameSoundFileTell, gameSoundFileGetSize) != 0) {
        if (gGameSoundDebugEnabled) {
            debugPrint("Failure setting sound I/O calls.\n");
        }
        return -1;
    }

    tickersAdd(_gsound_bkg_proc);
    gGameSoundInitialized = true;

    // SOUNDS
    if (gGameSoundDebugEnabled) {
        debugPrint("Sounds are ");
    }

    if (settings.sound.sounds) {
        // NOTE: Uninline.
        soundEffectsEnable();
    } else {
        if (gGameSoundDebugEnabled) {
            debugPrint(" not ");
        }
    }

    if (gGameSoundDebugEnabled) {
        debugPrint("on.\n");
    }

    // MUSIC
    if (gGameSoundDebugEnabled) {
        debugPrint("Music is ");
    }

    if (settings.sound.music) {
        // NOTE: Uninline.
        backgroundSoundEnable();
    } else {
        if (gGameSoundDebugEnabled) {
            debugPrint(" not ");
        }
    }

    if (gGameSoundDebugEnabled) {
        debugPrint("on.\n");
    }

    // SPEEECH
    if (gGameSoundDebugEnabled) {
        debugPrint("Speech is ");
    }

    if (settings.sound.speech) {
        // NOTE: Uninline.
        speechEnable();
    } else {
        if (gGameSoundDebugEnabled) {
            debugPrint(" not ");
        }
    }

    if (gGameSoundDebugEnabled) {
        debugPrint("on.\n");
    }

    gMasterVolume = settings.sound.master_volume;
    gameSoundSetMasterVolume(gMasterVolume);

    gMusicVolume = settings.sound.music_volume;
    backgroundSoundSetVolume(gMusicVolume);

    gSoundEffectsVolume = settings.sound.sndfx_volume;
    soundEffectsSetVolume(gSoundEffectsVolume);

    gSpeechVolume = settings.sound.speech_volume;
    speechSetVolume(gSpeechVolume);

    _gsound_background_fade = 0;
    gBackgroundSoundFileName[0] = '\0';

    return 0;
}

// 0x450164
void gameSoundReset()
{
    if (!gGameSoundInitialized) {
        return;
    }

    if (gGameSoundDebugEnabled) {
        debugPrint("Resetting sound system...");
    }

    // NOTE: Uninline.
    speechDelete();

    if (_gsound_background_df_vol) {
        // NOTE: Uninline.
        backgroundSoundEnable();
    }

    backgroundSoundDelete();

    _gsound_background_fade = 0;

    soundDeleteAll();

    soundEffectsCacheFlush();

    _gsound_active_effect_counter = 0;

    if (gGameSoundDebugEnabled) {
        debugPrint("done.\n");
    }

    return;
}

// 0x450244
int gameSoundExit()
{
    if (!gGameSoundInitialized) {
        return -1;
    }

    tickersRemove(_gsound_bkg_proc);

    // NOTE: Uninline.
    speechDelete();

    backgroundSoundDelete();
    gameSoundDeleteOldMusicFile();
    soundExit();
    soundEffectsCacheExit();
    audioFileExit();
    audioExit();

    gGameSoundInitialized = false;

    return 0;
}

// NOTE: Inlined.
//
// 0x4502BC
void soundEffectsEnable()
{
    if (gGameSoundInitialized) {
        gSoundEffectsEnabled = true;
    }
}

// NOTE: Inlined.
//
// 0x4502D0
void soundEffectsDisable()
{
    if (gGameSoundInitialized) {
        gSoundEffectsEnabled = false;
    }
}

// 0x4502E4
int soundEffectsIsEnabled()
{
    return gSoundEffectsEnabled;
}

// 0x4502EC
int gameSoundSetMasterVolume(int volume)
{
    if (!gGameSoundInitialized) {
        return -1;
    }

    if (volume < VOLUME_MIN && volume > VOLUME_MAX) {
        if (gGameSoundDebugEnabled) {
            debugPrint("Requested master volume out of range.\n");
        }
        return -1;
    }

    if (_gsound_background_df_vol && volume != 0 && backgroundSoundGetVolume() != 0) {
        // NOTE: Uninline.
        backgroundSoundEnable();
        _gsound_background_df_vol = 0;
    }

    if (_soundSetMasterVolume(volume) != 0) {
        if (gGameSoundDebugEnabled) {
            debugPrint("Error setting master sound volume.\n");
        }
        return -1;
    }

    gMasterVolume = volume;
    if (gMusicEnabled && volume == 0) {
        // NOTE: Uninline.
        backgroundSoundDisable();
        _gsound_background_df_vol = 1;
    }

    return 0;
}

// 0x450410
int gameSoundGetMasterVolume()
{
    return gMasterVolume;
}

// 0x450418
int soundEffectsSetVolume(int volume)
{
    if (!gGameSoundInitialized || volume < VOLUME_MIN || volume > VOLUME_MAX) {
        if (gGameSoundDebugEnabled) {
            debugPrint("Error setting sfx volume.\n");
        }
        return -1;
    }

    gSoundEffectsVolume = volume;

    return 0;
}

// 0x450454
int soundEffectsGetVolume()
{
    return gSoundEffectsVolume;
}

// NOTE: Inlined.
//
// 0x45045C
void backgroundSoundDisable()
{
    if (gGameSoundInitialized) {
        if (gMusicEnabled) {
            backgroundSoundDelete();
            movieSetVolume(0);
            gMusicEnabled = false;
        }
    }
}

// NOTE: Inlined.
//
// 0x450488
void backgroundSoundEnable()
{
    if (gGameSoundInitialized) {
        if (!gMusicEnabled) {
            movieSetVolume((int)(gMusicVolume * 0.94));
            gMusicEnabled = true;
            backgroundSoundRestart(12);
        }
    }
}

// 0x4504D4
int backgroundSoundIsEnabled()
{
    return gMusicEnabled;
}

// 0x4504DC
void backgroundSoundSetVolume(int volume)
{
    if (!gGameSoundInitialized) {
        return;
    }

    if (volume < VOLUME_MIN || volume > VOLUME_MAX) {
        if (gGameSoundDebugEnabled) {
            debugPrint("Requested background volume out of range.\n");
        }
        return;
    }

    gMusicVolume = volume;

    if (_gsound_background_df_vol) {
        // NOTE: Uninline.
        backgroundSoundEnable();
        _gsound_background_df_vol = 0;
    }

    if (gMusicEnabled) {
        movieSetVolume((int)(volume * 0.94));
    }

    if (gMusicEnabled) {
        if (gBackgroundSound != nullptr) {
            soundSetVolume(gBackgroundSound, (int)(gMusicVolume * 0.94));
        }
    }

    if (gMusicEnabled) {
        if (volume == 0 || gameSoundGetMasterVolume() == 0) {
            // NOTE: Uninline.
            backgroundSoundDisable();
            _gsound_background_df_vol = 1;
        }
    }
}

// 0x450618
int backgroundSoundGetVolume()
{
    return gMusicVolume;
}

//
int _gsound_background_volume_get_set(int volume)
{
    int oldMusicVolume = gMusicVolume;
    backgroundSoundSetVolume(volume);
    return oldMusicVolume;
}

// 0x450650
void backgroundSoundSetEndCallback(SoundEndCallback* callback)
{
    gBackgroundSoundEndCallback = callback;
}

// NOTE: There are no references to this function.
//
// 0x450670
int backgroundSoundGetDuration()
{
    return soundGetDuration(gBackgroundSound);
}

// [fileName] is base file name, without path and extension.
//
// 0x45067C
int backgroundSoundLoad(const char* fileName, int a2, int a3, int a4)
{
    int rc;

    _background_storage_requested = a3;
    _background_loop_requested = a4;

    strcpy(gBackgroundSoundFileName, fileName);

    if (!gGameSoundInitialized) {
        return -1;
    }

    if (!gMusicEnabled) {
        return -1;
    }

    if (gGameSoundDebugEnabled) {
        debugPrint("Loading background sound file %s%s...", fileName, ".acm");
    }

    backgroundSoundDelete();

    rc = _gsound_background_allocate(&gBackgroundSound, a3, a4);
    if (rc != 0) {
        if (gGameSoundDebugEnabled) {
            debugPrint("failed because sound could not be allocated.\n");
        }

        gBackgroundSound = nullptr;
        return -1;
    }

    rc = soundSetFileIO(gBackgroundSound, audioFileOpen, audioFileClose, audioFileRead, nullptr, audioFileSeek, gameSoundFileTellNotImplemented, audioFileGetSize);
    if (rc != 0) {
        if (gGameSoundDebugEnabled) {
            debugPrint("failed because file IO could not be set for compression.\n");
        }

        soundDelete(gBackgroundSound);
        gBackgroundSound = nullptr;

        return -1;
    }

    rc = soundSetChannels(gBackgroundSound, 3);
    if (rc != 0) {
        if (gGameSoundDebugEnabled) {
            debugPrint("failed because the channel could not be set.\n");
        }

        soundDelete(gBackgroundSound);
        gBackgroundSound = nullptr;

        return -1;
    }

    char path[COMPAT_MAX_PATH + 1];
    if (a3 == 13) {
        rc = gameSoundFindBackgroundSoundPath(path, fileName);
    } else if (a3 == 14) {
        rc = gameSoundFindBackgroundSoundPathWithCopy(path, fileName);
    }

    if (rc != SOUND_NO_ERROR) {
        if (gGameSoundDebugEnabled) {
            debugPrint("'failed because the file could not be found.\n");
        }

        soundDelete(gBackgroundSound);
        gBackgroundSound = nullptr;

        return -1;
    }

    if (a4 == 16) {
        rc = soundSetLooping(gBackgroundSound, 0xFFFF);
        if (rc != SOUND_NO_ERROR) {
            if (gGameSoundDebugEnabled) {
                debugPrint("failed because looping could not be set.\n");
            }

            soundDelete(gBackgroundSound);
            gBackgroundSound = nullptr;

            return -1;
        }
    }

    rc = soundSetCallback(gBackgroundSound, backgroundSoundCallback, nullptr);
    if (rc != SOUND_NO_ERROR) {
        if (gGameSoundDebugEnabled) {
            debugPrint("soundSetCallback failed for background sound\n");
        }
    }

    if (a2 == 11) {
        rc = soundSetReadLimit(gBackgroundSound, 0x40000);
        if (rc != SOUND_NO_ERROR) {
            if (gGameSoundDebugEnabled) {
                debugPrint("unable to set read limit ");
            }
        }
    }

    rc = soundLoad(gBackgroundSound, path);
    if (rc != SOUND_NO_ERROR) {
        if (gGameSoundDebugEnabled) {
            debugPrint("failed on call to soundLoad.\n");
        }

        soundDelete(gBackgroundSound);
        gBackgroundSound = nullptr;

        return -1;
    }

    if (a2 != 11) {
        rc = soundSetReadLimit(gBackgroundSound, 0x40000);
        if (rc != 0) {
            if (gGameSoundDebugEnabled) {
                debugPrint("unable to set read limit ");
            }
        }
    }

    if (a2 == 10) {
        return 0;
    }

    rc = backgroundSoundPlay();
    if (rc != 0) {
        if (gGameSoundDebugEnabled) {
            debugPrint("failed starting to play.\n");
        }

        soundDelete(gBackgroundSound);
        gBackgroundSound = nullptr;

        return -1;
    }

    if (gGameSoundDebugEnabled) {
        debugPrint("succeeded.\n");
    }

    return 0;
}

// 0x450A08
int _gsound_background_play_level_music(const char* a1, int a2)
{
    return backgroundSoundLoad(a1, a2, 14, 16);
}

// 0x450AB4
void backgroundSoundDelete()
{
    if (gGameSoundInitialized && gMusicEnabled && gBackgroundSound) {
        if (_gsound_background_fade) {
            if (_soundFade(gBackgroundSound, 2000, 0) == 0) {
                gBackgroundSound = nullptr;
                return;
            }
        }

        soundDelete(gBackgroundSound);
        gBackgroundSound = nullptr;
    }
}

// 0x450B0C
void backgroundSoundRestart(int value)
{
    if (gBackgroundSoundFileName[0] != '\0') {
        if (backgroundSoundLoad(gBackgroundSoundFileName, value, _background_storage_requested, _background_loop_requested) != 0) {
            if (gGameSoundDebugEnabled)
                debugPrint(" background restart failed ");
        }
    }
}

// 0x450B50
void backgroundSoundPause()
{
    if (gBackgroundSound != nullptr) {
        soundPause(gBackgroundSound);
    }
}

// 0x450B64
void backgroundSoundResume()
{
    if (gBackgroundSound != nullptr) {
        soundResume(gBackgroundSound);
    }
}

// NOTE: Inlined.
//
// 0x450B78
void speechDisable()
{
    if (gGameSoundInitialized) {
        if (gSpeechEnabled) {
            speechDelete();
            gSpeechEnabled = false;
        }
    }
}

// NOTE: Inlined.
//
// 0x450BC0
void speechEnable()
{
    if (gGameSoundInitialized) {
        if (!gSpeechEnabled) {
            gSpeechEnabled = true;
        }
    }
}

// 0x450BE0
int speechIsEnabled()
{
    return gSpeechEnabled;
}

// 0x450BE8
void speechSetVolume(int volume)
{
    if (!gGameSoundInitialized) {
        return;
    }

    if (volume < VOLUME_MIN || volume > VOLUME_MAX) {
        if (gGameSoundDebugEnabled) {
            debugPrint("Requested speech volume out of range.\n");
        }
        return;
    }

    gSpeechVolume = volume;

    if (gSpeechEnabled) {
        if (gSpeechSound != nullptr) {
            soundSetVolume(gSpeechSound, (int)(volume * 0.69));
        }
    }
}

// 0x450C5C
int speechGetVolume()
{
    return gSpeechVolume;
}

// 0x450C64
int _gsound_speech_volume_get_set(int volume)
{
    int oldVolume = gSpeechVolume;
    speechSetVolume(volume);
    return oldVolume;
}

// 0x450C74
void speechSetEndCallback(SoundEndCallback* callback)
{
    gSpeechEndCallback = callback;
}

// 0x450C94
int speechGetDuration()
{
    return soundGetDuration(gSpeechSound);
}

// 0x450CA0
int speechLoad(const char* fname, int a2, int a3, int a4)
{
    char path[COMPAT_MAX_PATH + 1];

    if (!gGameSoundInitialized) {
        return -1;
    }

    if (!gSpeechEnabled) {
        return -1;
    }

    if (gGameSoundDebugEnabled) {
        debugPrint("Loading speech sound file %s%s...", fname, ".ACM");
    }

    // uninline
    speechDelete();

    if (_gsound_background_allocate(&gSpeechSound, a3, a4)) {
        if (gGameSoundDebugEnabled) {
            debugPrint("failed because sound could not be allocated.\n");
        }
        gSpeechSound = nullptr;
        return -1;
    }

    if (soundSetFileIO(gSpeechSound, audioOpen, audioClose, audioRead, nullptr, audioSeek, gameSoundFileTellNotImplemented, audioGetSize)) {
        if (gGameSoundDebugEnabled) {
            debugPrint("failed because file IO could not be set for compression.\n");
        }
        soundDelete(gSpeechSound);
        gSpeechSound = nullptr;
        return -1;
    }

    if (gameSoundFindSpeechSoundPath(path, fname)) {
        if (gGameSoundDebugEnabled) {
            debugPrint("failed because the file could not be found.\n");
        }
        soundDelete(gSpeechSound);
        gSpeechSound = nullptr;
        return -1;
    }

    if (a4 == 16) {
        if (soundSetLooping(gSpeechSound, 0xFFFF)) {
            if (gGameSoundDebugEnabled) {
                debugPrint("failed because looping could not be set.\n");
            }
            soundDelete(gSpeechSound);
            gSpeechSound = nullptr;
            return -1;
        }
    }

    if (soundSetCallback(gSpeechSound, speechCallback, nullptr)) {
        if (gGameSoundDebugEnabled) {
            debugPrint("soundSetCallback failed for speech sound\n");
        }
    }

    if (a2 == 11) {
        if (soundSetReadLimit(gSpeechSound, 0x40000)) {
            if (gGameSoundDebugEnabled) {
                debugPrint("unable to set read limit ");
            }
        }
    }

    if (soundLoad(gSpeechSound, path)) {
        if (gGameSoundDebugEnabled) {
            debugPrint("failed on call to soundLoad.\n");
        }
        soundDelete(gSpeechSound);
        gSpeechSound = nullptr;
        return -1;
    }

    if (a2 != 11) {
        if (soundSetReadLimit(gSpeechSound, 0x40000)) {
            if (gGameSoundDebugEnabled) {
                debugPrint("unable to set read limit ");
            }
        }
    }

    if (a2 == 10) {
        return 0;
    }

    if (speechPlay()) {
        if (gGameSoundDebugEnabled) {
            debugPrint("failed starting to play.\n");
        }
        soundDelete(gSpeechSound);
        gSpeechSound = nullptr;
        return -1;
    }

    if (gGameSoundDebugEnabled) {
        debugPrint("succeeded.\n");
    }

    return 0;
}

// 0x450F8C
int _gsound_speech_play_preloaded()
{
    if (!gGameSoundInitialized) {
        return -1;
    }

    if (!gSpeechEnabled) {
        return -1;
    }

    if (gSpeechSound == nullptr) {
        return -1;
    }

    if (soundIsPlaying(gSpeechSound)) {
        return -1;
    }

    if (soundIsPaused(gSpeechSound)) {
        return -1;
    }

    if (_soundDone(gSpeechSound)) {
        return -1;
    }

    if (speechPlay() != 0) {
        soundDelete(gSpeechSound);
        gSpeechSound = nullptr;

        return -1;
    }

    return 0;
}

// 0x451024
void speechDelete()
{
    if (gGameSoundInitialized && gSpeechEnabled) {
        if (gSpeechSound != nullptr) {
            soundDelete(gSpeechSound);
            gSpeechSound = nullptr;
        }
    }
}

// 0x451054
void speechPause()
{
    if (gSpeechSound != nullptr) {
        soundPause(gSpeechSound);
    }
}

// 0x451068
void speechResume()
{
    if (gSpeechSound != nullptr) {
        soundResume(gSpeechSound);
    }
}

// 0x45108C
int _gsound_play_sfx_file_volume(const char* a1, int a2)
{
    Sound* v1;

    if (!gGameSoundInitialized) {
        return -1;
    }

    if (!gSoundEffectsEnabled) {
        return -1;
    }

    v1 = soundEffectLoadWithVolume(a1, nullptr, a2);
    if (v1 == nullptr) {
        return -1;
    }

    soundPlay(v1);

    return 0;
}

// 0x4510DC
Sound* soundEffectLoad(const char* name, Object* object)
{
    if (!gGameSoundInitialized) {
        return nullptr;
    }

    if (!gSoundEffectsEnabled) {
        return nullptr;
    }

    if (gGameSoundDebugEnabled) {
        debugPrint("Loading sound file %s%s...", name, ".ACM");
    }

    if (_gsound_active_effect_counter >= SOUND_EFFECTS_MAX_COUNT) {
        if (gGameSoundDebugEnabled) {
            debugPrint("failed because there are already %d active effects.\n", _gsound_active_effect_counter);
        }

        return nullptr;
    }

    Sound* sound = _gsound_get_sound_ready_for_effect();
    if (sound == nullptr) {
        if (gGameSoundDebugEnabled) {
            debugPrint("failed.\n");
        }

        return nullptr;
    }

    ++_gsound_active_effect_counter;

    char path[COMPAT_MAX_PATH];
    snprintf(path, sizeof(path), "%s%s%s", _sound_sfx_path, name, ".ACM");

    if (soundLoad(sound, path) == 0) {
        if (gGameSoundDebugEnabled) {
            debugPrint("succeeded.\n");
        }

        return sound;
    }

    if (object != nullptr) {
        if (FID_TYPE(object->fid) == OBJ_TYPE_CRITTER && (name[0] == 'H' || name[0] == 'N')) {
            char v9 = name[1];
            if (v9 == 'A' || v9 == 'F' || v9 == 'M') {
                if (v9 == 'A') {
                    if (critterGetStat(object, STAT_GENDER)) {
                        v9 = 'F';
                    } else {
                        v9 = 'M';
                    }
                }
            }

            snprintf(path, sizeof(path), "%sH%cXXXX%s%s", _sound_sfx_path, v9, name + 6, ".ACM");

            if (gGameSoundDebugEnabled) {
                debugPrint("tyring %s ", path + strlen(_sound_sfx_path));
            }

            if (soundLoad(sound, path) == 0) {
                if (gGameSoundDebugEnabled) {
                    debugPrint("succeeded (with alias).\n");
                }

                return sound;
            }

            if (v9 == 'F') {
                snprintf(path, sizeof(path), "%sHMXXXX%s%s", _sound_sfx_path, name + 6, ".ACM");

                if (gGameSoundDebugEnabled) {
                    debugPrint("tyring %s ", path + strlen(_sound_sfx_path));
                }

                if (soundLoad(sound, path) == 0) {
                    if (gGameSoundDebugEnabled) {
                        debugPrint("succeeded (with male alias).\n");
                    }

                    return sound;
                }
            }
        }
    }

    if (strncmp(name, "MALIEU", 6) == 0 || strncmp(name, "MAMTN2", 6) == 0) {
        snprintf(path, sizeof(path), "%sMAMTNT%s%s", _sound_sfx_path, name + 6, ".ACM");

        if (gGameSoundDebugEnabled) {
            debugPrint("tyring %s ", path + strlen(_sound_sfx_path));
        }

        if (soundLoad(sound, path) == 0) {
            if (gGameSoundDebugEnabled) {
                debugPrint("succeeded (with alias).\n");
            }

            return sound;
        }
    }

    --_gsound_active_effect_counter;

    soundDelete(sound);

    if (gGameSoundDebugEnabled) {
        debugPrint("failed.\n");
    }

    return nullptr;
}

// 0x45145C
Sound* soundEffectLoadWithVolume(const char* name, Object* object, int volume)
{
    Sound* sound = soundEffectLoad(name, object);

    if (sound != nullptr) {
        soundSetVolume(sound, (volume * gSoundEffectsVolume) / VOLUME_MAX);
    }

    return sound;
}

// 0x45148C
void soundEffectDelete(Sound* sound)
{
    if (!gGameSoundInitialized) {
        return;
    }

    if (!gSoundEffectsEnabled) {
        return;
    }

    if (soundIsPlaying(sound)) {
        if (gGameSoundDebugEnabled) {
            debugPrint("Trying to manually delete a sound effect after it has started playing.\n");
        }
        return;
    }

    if (soundDelete(sound) != 0) {
        if (gGameSoundDebugEnabled) {
            debugPrint("Unable to delete sound effect -- active effect counter may get out of sync.\n");
        }
        return;
    }

    --_gsound_active_effect_counter;
}

// 0x4514F0
int _gsnd_anim_sound(Sound* sound, void* a2)
{
    if (!gGameSoundInitialized) {
        return 0;
    }

    if (!gSoundEffectsEnabled) {
        return 0;
    }

    if (sound == nullptr) {
        return 0;
    }

    soundPlay(sound);

    return 0;
}

// 0x451510
int soundEffectPlay(Sound* sound)
{
    if (!gGameSoundInitialized) {
        return -1;
    }

    if (!gSoundEffectsEnabled) {
        return -1;
    }

    if (sound == nullptr) {
        return -1;
    }

    soundPlay(sound);

    return 0;
}

// Probably returns volume dependending on the distance between the specified
// object and dude.
//
// 0x451534
int _gsound_compute_relative_volume(Object* obj)
{
    int type;
    int v3;
    Object* v7;
    Rect v12;
    Rect v14;
    Rect iso_win_rect;
    int distance;
    int perception;

    v3 = 0x7FFF;

    if (obj) {
        type = FID_TYPE(obj->fid);
        if (type == 0 || type == 1 || type == 2) {
            v7 = objectGetOwner(obj);
            if (!v7) {
                v7 = obj;
            }

            objectGetRect(v7, &v14);

            windowGetRect(gIsoWindow, &iso_win_rect);

            if (rectIntersection(&v14, &iso_win_rect, &v12) == -1) {
                distance = objectGetDistanceBetween(v7, gDude);
                perception = critterGetStat(gDude, STAT_PERCEPTION);
                if (distance > perception) {
                    if (distance < 2 * perception) {
                        v3 = 0x7FFF - 0x5554 * (distance - perception) / perception;
                    } else {
                        v3 = 0x2AAA;
                    }
                } else {
                    v3 = 0x7FFF;
                }
            }
        }
    }

    return v3;
}

// sfx_build_char_name
// 0x451604
char* sfxBuildCharName(Object* a1, int anim, int extra)
{
    char v7[13];
    char v8;
    char v9;

    if (artCopyFileName(FID_TYPE(a1->fid), a1->fid & 0xFFF, v7) == -1) {
        return nullptr;
    }

    if (anim == ANIM_TAKE_OUT) {
        if (_art_get_code(anim, extra, &v8, &v9) == -1) {
            return nullptr;
        }
    } else {
        if (_art_get_code(anim, (a1->fid & 0xF000) >> 12, &v8, &v9) == -1) {
            return nullptr;
        }
    }

    // TODO: Check.
    if (anim == ANIM_FALL_FRONT || anim == ANIM_FALL_BACK) {
        if (extra == CHARACTER_SOUND_EFFECT_PASS_OUT) {
            v8 = 'Y';
        } else if (extra == CHARACTER_SOUND_EFFECT_DIE) {
            v8 = 'Z';
        }
    } else if ((anim == ANIM_THROW_PUNCH || anim == ANIM_KICK_LEG) && extra == CHARACTER_SOUND_EFFECT_CONTACT) {
        v8 = 'Z';
    }

    snprintf(_sfx_file_name, sizeof(_sfx_file_name), "%s%c%c", v7, v8, v9);
    compat_strupr(_sfx_file_name);
    return _sfx_file_name;
}

// sfx_build_ambient_name
// 0x4516F0
char* gameSoundBuildAmbientSoundEffectName(const char* a1)
{
    snprintf(_sfx_file_name, sizeof(_sfx_file_name), "A%6s%1d", a1, 1);
    compat_strupr(_sfx_file_name);
    return _sfx_file_name;
}

// sfx_build_interface_name
// 0x451718
char* gameSoundBuildInterfaceName(const char* a1)
{
    snprintf(_sfx_file_name, sizeof(_sfx_file_name), "N%6s%1d", a1, 1);
    compat_strupr(_sfx_file_name);
    return _sfx_file_name;
}

// sfx_build_weapon_name
// 0x451760
char* sfxBuildWeaponName(int effectType, Object* weapon, int hitMode, Object* target)
{
    int soundVariant;
    char weaponSoundCode;
    char effectTypeCode;
    char materialCode;
    Proto* proto;

    weaponSoundCode = weaponGetSoundId(weapon);
    effectTypeCode = _snd_lookup_weapon_type[effectType];

    if (effectType != WEAPON_SOUND_EFFECT_READY
        && effectType != WEAPON_SOUND_EFFECT_OUT_OF_AMMO) {
        if (hitMode != HIT_MODE_LEFT_WEAPON_PRIMARY
            && hitMode != HIT_MODE_RIGHT_WEAPON_PRIMARY
            && hitMode != HIT_MODE_PUNCH) {
            soundVariant = 2;
        } else {
            soundVariant = 1;
        }
    } else {
        soundVariant = 1;
    }

    int damageType = weaponGetDamageType(nullptr, weapon);

    // SFALL
    if (effectTypeCode != 'H' || target == nullptr || damageType == explosionGetDamageType() || damageType == DAMAGE_TYPE_PLASMA || damageType == DAMAGE_TYPE_EMP) {
        materialCode = 'X';
    } else {
        const int type = FID_TYPE(target->fid);
        int material;
        switch (type) {
        case OBJ_TYPE_ITEM:
            protoGetProto(target->pid, &proto);
            material = proto->item.material;
            break;
        case OBJ_TYPE_SCENERY:
            protoGetProto(target->pid, &proto);
            material = proto->scenery.field_2C;
            break;
        case OBJ_TYPE_WALL:
            protoGetProto(target->pid, &proto);
            material = proto->wall.material;
            break;
        default:
            material = -1;
            break;
        }

        switch (material) {
        case MATERIAL_TYPE_GLASS:
        case MATERIAL_TYPE_METAL:
        case MATERIAL_TYPE_PLASTIC:
            materialCode = 'M';
            break;
        case MATERIAL_TYPE_WOOD:
            materialCode = 'W';
            break;
        case MATERIAL_TYPE_DIRT:
        case MATERIAL_TYPE_STONE:
        case MATERIAL_TYPE_CEMENT:
            materialCode = 'S';
            break;
        default:
            materialCode = 'F';
            break;
        }
    }

    snprintf(_sfx_file_name, sizeof(_sfx_file_name), "W%c%c%1d%cXX%1d", effectTypeCode, weaponSoundCode, soundVariant, materialCode, 1);
    compat_strupr(_sfx_file_name);
    return _sfx_file_name;
}

// sfx_build_scenery_name
// 0x451898
char* sfxBuildSceneryName(int actionType, int action, const char* name)
{
    char actionTypeCode = actionType == SOUND_EFFECT_ACTION_TYPE_PASSIVE ? 'P' : 'A';
    char actionCode = _snd_lookup_scenery_action[action];

    snprintf(_sfx_file_name, sizeof(_sfx_file_name), "S%c%c%4s%1d", actionTypeCode, actionCode, name, 1);
    compat_strupr(_sfx_file_name);

    return _sfx_file_name;
}

// sfx_build_open_name
// 0x4518D
char* sfxBuildOpenName(Object* object, int action)
{
    if (FID_TYPE(object->fid) == OBJ_TYPE_SCENERY) {
        char scenerySoundId;
        Proto* proto;
        if (protoGetProto(object->pid, &proto) != -1) {
            scenerySoundId = proto->scenery.field_34;
        } else {
            scenerySoundId = 'A';
        }
        snprintf(_sfx_file_name, sizeof(_sfx_file_name), "S%cDOORS%c", _snd_lookup_scenery_action[action], scenerySoundId);
    } else {
        Proto* proto;
        protoGetProto(object->pid, &proto);
        snprintf(_sfx_file_name, sizeof(_sfx_file_name), "I%cCNTNR%c", _snd_lookup_scenery_action[action], proto->item.field_80);
    }
    compat_strupr(_sfx_file_name);
    return _sfx_file_name;
}

// 0x451970
void _gsound_red_butt_press(int btn, int keyCode)
{
    soundPlayFile("ib1p1xx1");
}

// 0x451978
void _gsound_red_butt_release(int btn, int keyCode)
{
    soundPlayFile("ib1lu1x1");
}

// 0x451980
void _gsound_toggle_butt_press_(int btn, int keyCode)
{
    soundPlayFile("toggle");
}

// 0x451988
void _gsound_med_butt_press(int btn, int keyCode)
{
    soundPlayFile("ib2p1xx1");
}

// 0x451990
void _gsound_med_butt_release(int btn, int keyCode)
{
    soundPlayFile("ib2lu1x1");
}

// 0x451998
void _gsound_lrg_butt_press(int btn, int keyCode)
{
    soundPlayFile("ib3p1xx1");
}

// 0x4519A0
void _gsound_lrg_butt_release(int btn, int keyCode)
{
    soundPlayFile("ib3lu1x1");
}

// 0x4519A8
int soundPlayFile(const char* name)
{
    if (!gGameSoundInitialized) {
        return -1;
    }

    if (!gSoundEffectsEnabled) {
        return -1;
    }

    Sound* sound = soundEffectLoad(name, nullptr);
    if (sound == nullptr) {
        return -1;
    }

    soundPlay(sound);

    return 0;
}

// 0x451A00
void _gsound_bkg_proc()
{
    soundContinueAll();
}

// 0x451A08
int gameSoundFileOpen(const char* fname, int* sampleRate)
{
    File* stream = fileOpen(fname, "rb");
    if (stream == nullptr) {
        return -1;
    }

    return ptrToInt(stream);
}

// NOTE: Collapsed.
//
// 0x451A1C
long _gsound_write_()
{
    return -1;
}

// NOTE: Uncollapsed 0x451A1C.
//
// The purpose of this function is unknown. It simply returns -1 without
// actually telling position. This function is used for all game sounds -
// background music, speech, and sound effects. There is another function
// [gameSoundFileTell] which actually provides position.
long gameSoundFileTellNotImplemented(int fileHandle)
{
    return _gsound_write_();
}

// NOTE: Uncollapsed 0x451A1C.
int gameSoundFileWrite(int fileHandle, const void* buf, unsigned int size)
{
    return _gsound_write_();
}

// 0x451A24
int gameSoundFileClose(int fileHandle)
{
    if (fileHandle == -1) {
        return -1;
    }

    return fileClose((File*)intToPtr(fileHandle, true));
}

// 0x451A30
int gameSoundFileRead(int fileHandle, void* buffer, unsigned int size)
{
    if (fileHandle == -1) {
        return -1;
    }

    return fileRead(buffer, 1, size, (File*)intToPtr(fileHandle));
}

// 0x451A4C
long gameSoundFileSeek(int fileHandle, long offset, int origin)
{
    if (fileHandle == -1) {
        return -1;
    }

    if (fileSeek((File*)intToPtr(fileHandle), offset, origin) != 0) {
        return -1;
    }

    return fileTell((File*)intToPtr(fileHandle));
}

// 0x451A70
long gameSoundFileTell(int handle)
{
    if (handle == -1) {
        return -1;
    }

    return fileTell((File*)intToPtr(handle));
}

// 0x451A7C
long gameSoundFileGetSize(int handle)
{
    if (handle == -1) {
        return -1;
    }

    return fileGetSize((File*)intToPtr(handle));
}

// 0x451A88
bool gameSoundIsCompressed(char* filePath)
{
    return true;
}

// 0x451A90
void speechCallback(void* userData, int a2)
{
    if (a2 == 1) {
        gSpeechSound = nullptr;

        if (gSpeechEndCallback) {
            gSpeechEndCallback();
        }
    }
}

// 0x451AB0
void backgroundSoundCallback(void* userData, int a2)
{
    if (a2 == 1) {
        gBackgroundSound = nullptr;

        if (gBackgroundSoundEndCallback) {
            gBackgroundSoundEndCallback();
        }
    }
}

// 0x451AD0
void soundEffectCallback(void* userData, int a2)
{
    if (a2 == 1) {
        --_gsound_active_effect_counter;
    }
}

// 0x451ADC
int _gsound_background_allocate(Sound** soundPtr, int a2, int a3)
{
    int soundFlags = SOUND_FLAG_0x02 | SOUND_16BIT;
    int type = 0;
    if (a2 == 13) {
        type |= SOUND_TYPE_MEMORY;
    } else if (a2 == 14) {
        type |= SOUND_TYPE_STREAMING;
    }

    if (a3 == 15) {
        type |= SOUND_TYPE_FIRE_AND_FORGET;
    } else if (a3 == 16) {
        soundFlags |= SOUND_LOOPING;
    }

    Sound* sound = soundAllocate(type, soundFlags);
    if (sound == nullptr) {
        return -1;
    }

    *soundPtr = sound;

    return 0;
}

// gsound_background_find_with_copy
// 0x451B30
int gameSoundFindBackgroundSoundPathWithCopy(char* dest, const char* src)
{
    size_t len = strlen(src) + strlen(".ACM");
    if (strlen(_sound_music_path1) + len > COMPAT_MAX_PATH || strlen(_sound_music_path2) + len > COMPAT_MAX_PATH) {
        if (gGameSoundDebugEnabled) {
            debugPrint("Full background path too long.\n");
        }

        return -1;
    }

    if (gGameSoundDebugEnabled) {
        debugPrint(" finding background sound ");
    }

    char outPath[COMPAT_MAX_PATH];
    snprintf(outPath, sizeof(outPath), "%s%s%s", _sound_music_path1, src, ".ACM");
    if (_gsound_file_exists_f(outPath)) {
        strncpy(dest, outPath, COMPAT_MAX_PATH);
        dest[COMPAT_MAX_PATH] = '\0';
        return 0;
    }

    if (gGameSoundDebugEnabled) {
        debugPrint("by copy ");
    }

    gameSoundDeleteOldMusicFile();

    char inPath[COMPAT_MAX_PATH];
    snprintf(inPath, sizeof(inPath), "%s%s%s", _sound_music_path2, src, ".ACM");

    FILE* inStream = compat_fopen(inPath, "rb");
    if (inStream == nullptr) {
        if (gGameSoundDebugEnabled) {
            debugPrint("Unable to find music file %s to copy down.\n", src);
        }

        return -1;
    }

    FILE* outStream = compat_fopen(outPath, "wb");
    if (outStream == nullptr) {
        if (gGameSoundDebugEnabled) {
            debugPrint("Unable to open music file %s for copying to.", src);
        }

        fclose(inStream);

        return -1;
    }

    void* buffer = internal_malloc(0x2000);
    if (buffer == nullptr) {
        if (gGameSoundDebugEnabled) {
            debugPrint("Out of memory in gsound_background_find_with_copy.\n", src);
        }

        fclose(outStream);
        fclose(inStream);

        return -1;
    }

    bool err = false;
    while (!feof(inStream)) {
        size_t bytesRead = fread(buffer, 1, 0x2000, inStream);
        if (bytesRead == 0) {
            break;
        }

        if (fwrite(buffer, 1, bytesRead, outStream) != bytesRead) {
            err = true;
            break;
        }
    }

    internal_free(buffer);
    fclose(outStream);
    fclose(inStream);

    if (err) {
        if (gGameSoundDebugEnabled) {
            debugPrint("Background sound file copy failed on write -- ");
            debugPrint("likely out of disc space.\n");
        }

        return -1;
    }

    strcpy(_background_fname_copied, src);

    strncpy(dest, outPath, COMPAT_MAX_PATH);
    dest[COMPAT_MAX_PATH] = '\0';

    return 0;
}

// 0x451E2C
int gameSoundFindBackgroundSoundPath(char* dest, const char* src)
{
    char path[COMPAT_MAX_PATH];
    size_t len;

    len = strlen(src) + strlen(".ACM");
    if (strlen(_sound_music_path1) + len > COMPAT_MAX_PATH || strlen(_sound_music_path2) + len > COMPAT_MAX_PATH) {
        if (gGameSoundDebugEnabled) {
            debugPrint("Full background path too long.\n");
        }

        return -1;
    }

    if (gGameSoundDebugEnabled) {
        debugPrint(" finding background sound ");
    }

    snprintf(path, sizeof(path), "%s%s%s", _sound_music_path1, src, ".ACM");
    if (_gsound_file_exists_f(path)) {
        strncpy(dest, path, COMPAT_MAX_PATH);
        dest[COMPAT_MAX_PATH] = '\0';
        return 0;
    }

    if (gGameSoundDebugEnabled) {
        debugPrint("in 2nd path ");
    }

    snprintf(path, sizeof(path), "%s%s%s", _sound_music_path2, src, ".ACM");
    if (_gsound_file_exists_f(path)) {
        strncpy(dest, path, COMPAT_MAX_PATH);
        dest[COMPAT_MAX_PATH] = '\0';
        return 0;
    }

    if (gGameSoundDebugEnabled) {
        debugPrint("-- find failed ");
    }

    return -1;
}

// 0x451F94
int gameSoundFindSpeechSoundPath(char* dest, const char* src)
{
    char path[COMPAT_MAX_PATH];

    if (strlen(_sound_speech_path) + strlen(".acm") > COMPAT_MAX_PATH) {
        if (gGameSoundDebugEnabled) {
            // FIXME: The message is wrong (notes background path, but here
            // we're dealing with speech path).
            debugPrint("Full background path too long.\n");
        }

        return -1;
    }

    if (gGameSoundDebugEnabled) {
        debugPrint(" finding speech sound ");
    }

    snprintf(path, sizeof(path), "%s%s%s", _sound_speech_path, src, ".ACM");

    // Check for existence by getting file size.
    int fileSize;
    if (dbGetFileSize(path, &fileSize) != 0) {
        if (gGameSoundDebugEnabled) {
            debugPrint("-- find failed ");
        }

        return -1;
    }

    strncpy(dest, path, COMPAT_MAX_PATH);
    dest[COMPAT_MAX_PATH] = '\0';

    return 0;
}

// delete old music file
// 0x452088
void gameSoundDeleteOldMusicFile()
{
    if (_background_fname_copied[0] != '\0') {
        char path[COMPAT_MAX_PATH];
        snprintf(path, sizeof(path), "%s%s%s", "sound\\music\\", _background_fname_copied, ".ACM");
        if (compat_remove(path)) {
            if (gGameSoundDebugEnabled) {
                debugPrint("Deleting old music file failed.\n");
            }
        }

        _background_fname_copied[0] = '\0';
    }
}

// 0x4520EC
int backgroundSoundPlay()
{
    int result;

    if (gGameSoundDebugEnabled) {
        debugPrint(" playing ");
    }

    if (_gsound_background_fade) {
        soundSetVolume(gBackgroundSound, 1);
        result = _soundFade(gBackgroundSound, 2000, (int)(gMusicVolume * 0.94));
    } else {
        soundSetVolume(gBackgroundSound, (int)(gMusicVolume * 0.94));
        result = soundPlay(gBackgroundSound);
    }

    if (result != 0) {
        if (gGameSoundDebugEnabled) {
            debugPrint("Unable to play background sound.\n");
        }

        result = -1;
    }

    return result;
}

// 0x45219C
int speechPlay()
{
    if (gGameSoundDebugEnabled) {
        debugPrint(" playing ");
    }

    soundSetVolume(gSpeechSound, (int)(gSpeechVolume * 0.69));

    if (soundPlay(gSpeechSound) != 0) {
        if (gGameSoundDebugEnabled) {
            debugPrint("Unable to play speech sound.\n");
        }

        return -1;
    }

    return 0;
}

// TODO: Refactor to use Settings.
//
// 0x452208
int _gsound_get_music_path(char** out_value, const char* key)
{
    size_t len;
    char* copy;
    char* value;

    configGetString(&gGameConfig, GAME_CONFIG_SOUND_KEY, key, out_value);

    value = *out_value;
    len = strlen(value);

    if (value[len - 1] == '\\' || value[len - 1] == '/') {
        return 0;
    }

    copy = (char*)internal_malloc(len + 2);
    if (copy == nullptr) {
        if (gGameSoundDebugEnabled) {
            debugPrint("Out of memory in gsound_get_music_path.\n");
        }
        return -1;
    }

    strcpy(copy, value);
    copy[len] = '\\';
    copy[len + 1] = '\0';

    if (configSetString(&gGameConfig, GAME_CONFIG_SOUND_KEY, key, copy) != 1) {
        if (gGameSoundDebugEnabled) {
            debugPrint("config_set_string failed in gsound_music_path.\n");
        }

        return -1;
    }

    if (configGetString(&gGameConfig, GAME_CONFIG_SOUND_KEY, key, out_value)) {
        internal_free(copy);
        return 0;
    }

    if (gGameSoundDebugEnabled) {
        debugPrint("config_get_string failed in gsound_music_path.\n");
    }

    return -1;
}

// 0x452378
Sound* _gsound_get_sound_ready_for_effect()
{
    int rc;

    Sound* sound = soundAllocate(SOUND_TYPE_MEMORY | SOUND_TYPE_FIRE_AND_FORGET, SOUND_FLAG_0x02 | SOUND_16BIT);
    if (sound == nullptr) {
        if (gGameSoundDebugEnabled) {
            debugPrint(" Can't allocate sound for effect. ");
        }

        if (gGameSoundDebugEnabled) {
            debugPrint("soundAllocate returned: %d, %s\n", 0, soundGetErrorDescription(0));
        }

        return nullptr;
    }

    if (soundEffectsCacheInitialized()) {
        rc = soundSetFileIO(sound, soundEffectsCacheFileOpen, soundEffectsCacheFileClose, soundEffectsCacheFileRead, soundEffectsCacheFileWrite, soundEffectsCacheFileSeek, soundEffectsCacheFileTell, soundEffectsCacheFileLength);
    } else {
        rc = soundSetFileIO(sound, audioOpen, audioClose, audioRead, nullptr, audioSeek, gameSoundFileTellNotImplemented, audioGetSize);
    }

    if (rc != 0) {
        if (gGameSoundDebugEnabled) {
            debugPrint("Can't set file IO on sound effect.\n");
        }

        if (gGameSoundDebugEnabled) {
            debugPrint("soundSetFileIO returned: %d, %s\n", rc, soundGetErrorDescription(rc));
        }

        soundDelete(sound);

        return nullptr;
    }

    rc = soundSetCallback(sound, soundEffectCallback, nullptr);
    if (rc != 0) {
        if (gGameSoundDebugEnabled) {
            debugPrint("failed because the callback could not be set.\n");
        }

        if (gGameSoundDebugEnabled) {
            debugPrint("soundSetCallback returned: %d, %s\n", rc, soundGetErrorDescription(rc));
        }

        soundDelete(sound);

        return nullptr;
    }

    soundSetVolume(sound, gSoundEffectsVolume);

    return sound;
}

// Check file for existence.
//
// 0x4524E0
bool _gsound_file_exists_f(const char* fname)
{
    FILE* f = compat_fopen(fname, "rb");
    if (f == nullptr) {
        return false;
    }

    fclose(f);

    return true;
}

// gsound_setup_paths
// 0x452518
int _gsound_setup_paths()
{
    // TODO: Incomplete.

    return 0;
}

// 0x452628
int _gsound_sfx_q_start()
{
    return ambientSoundEffectEventProcess(nullptr, nullptr);
}

// 0x452634
int ambientSoundEffectEventProcess(Object* a1, void* data)
{
    _queue_clear_type(EVENT_TYPE_GSOUND_SFX_EVENT, nullptr);

    AmbientSoundEffectEvent* soundEffectEvent = (AmbientSoundEffectEvent*)data;
    int ambientSoundEffectIndex = -1;
    if (soundEffectEvent != nullptr) {
        ambientSoundEffectIndex = soundEffectEvent->ambientSoundEffectIndex;
    } else {
        if (wmSfxMaxCount() > 0) {
            ambientSoundEffectIndex = wmSfxRollNextIdx();
        }
    }

    AmbientSoundEffectEvent* nextSoundEffectEvent = (AmbientSoundEffectEvent*)internal_malloc(sizeof(*nextSoundEffectEvent));
    if (nextSoundEffectEvent == nullptr) {
        return -1;
    }

    if (gMapHeader.name[0] == '\0') {
        return 0;
    }

    int delay = 10 * randomBetween(15, 20);
    if (wmSfxMaxCount() > 0) {
        nextSoundEffectEvent->ambientSoundEffectIndex = wmSfxRollNextIdx();
        if (queueAddEvent(delay, nullptr, nextSoundEffectEvent, EVENT_TYPE_GSOUND_SFX_EVENT) == -1) {
            return -1;
        }
    }

    if (isInCombat()) {
        ambientSoundEffectIndex = -1;
    }

    if (ambientSoundEffectIndex != -1) {
        char* fileName;
        if (wmSfxIdxName(ambientSoundEffectIndex, &fileName) == 0) {
            int v7 = _get_bk_time();
            if (getTicksBetween(v7, _lastTime_1) >= 5000) {
                if (soundPlayFile(fileName) == -1) {
                    debugPrint("\nGsound: playing ambient map sfx: %s.  FAILED", fileName);
                } else {
                    debugPrint("\nGsound: playing ambient map sfx: %s", fileName);
                }
            }
            _lastTime_1 = v7;
        }
    }

    return 0;
}

} // namespace fallout
