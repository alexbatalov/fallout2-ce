#ifndef GAME_SOUND_H
#define GAME_SOUND_H

#include "obj_types.h"
#include "sound.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

typedef enum WeaponSoundEffect {
    WEAPON_SOUND_EFFECT_READY,
    WEAPON_SOUND_EFFECT_ATTACK,
    WEAPON_SOUND_EFFECT_OUT_OF_AMMO,
    WEAPON_SOUND_EFFECT_AMMO_FLYING,
    WEAPON_SOUND_EFFECT_HIT,
    WEAPON_SOUND_EFFECT_COUNT,
} WeaponSoundEffect;

typedef enum SoundEffectActionType {
    SOUND_EFFECT_ACTION_TYPE_ACTIVE,
    SOUND_EFFECT_ACTION_TYPE_PASSIVE,
} SoundEffectActionType;

typedef enum ScenerySoundEffect {
    SCENERY_SOUND_EFFECT_OPEN,
    SCENERY_SOUND_EFFECT_CLOSED,
    SCENERY_SOUND_EFFECT_LOCKED,
    SCENERY_SOUND_EFFECT_UNLOCKED,
    SCENERY_SOUND_EFFECT_USED,
    SCENERY_SOUND_EFFECT_COUNT,
} ScenerySoundEffect;

typedef enum CharacterSoundEffect {
    CHARACTER_SOUND_EFFECT_UNUSED,
    CHARACTER_SOUND_EFFECT_KNOCKDOWN,
    CHARACTER_SOUND_EFFECT_PASS_OUT,
    CHARACTER_SOUND_EFFECT_DIE,
    CHARACTER_SOUND_EFFECT_CONTACT,
} CharacterSoundEffect;

typedef void(SoundEndCallback)();

extern char _aSoundSfx[];
extern char _aSoundMusic_0[];
extern char _aSoundSpeech_0[];

extern bool gGameSoundInitialized;
extern bool gGameSoundDebugEnabled;
extern bool gMusicEnabled;
extern int dword_518E3;
extern int _gsound_background_fade;
extern bool gSpeechEnabled;
extern bool gSoundEffectsEnabled;
extern int _gsound_active_effect_counter;
extern Sound* gBackgroundSound;
extern Sound* gSpeechSound;
extern SoundEndCallback* gBackgroundSoundEndCallback;
extern SoundEndCallback* gSpeechEndCallback;
extern char _snd_lookup_weapon_type[WEAPON_SOUND_EFFECT_COUNT];
extern char _snd_lookup_scenery_action[SCENERY_SOUND_EFFECT_COUNT];
extern int _background_storage_requested;
extern int _background_loop_requested;
extern char* _sound_sfx_path;
extern char* _sound_music_path1;
extern char* _sound_music_path2;
extern char* _sound_speech_path;
extern int gMasterVolume;
extern int gMusicVolume;
extern int gSpeechVolume;
extern int gSoundEffectsVolume;
extern int _detectDevices;
extern int _lastTime_1;

extern char _background_fname_copied[MAX_PATH];
extern char _sfx_file_name[13];
extern char gBackgroundSoundFileName[270];

int gameSoundInit();
void gameSoundReset();
int gameSoundExit();
void soundEffectsEnable();
void soundEffectsDisable();
int soundEffectsIsEnabled();
int gameSoundSetMasterVolume(int value);
int gameSoundGetMasterVolume();
int soundEffectsSetVolume(int value);
int soundEffectsGetVolume();
void backgroundSoundDisable();
void backgroundSoundEnable();
int backgroundSoundIsEnabled();
void backgroundSoundSetVolume(int value);
int backgroundSoundGetVolume();
int _gsound_background_volume_get_set(int a1);
void backgroundSoundSetEndCallback(SoundEndCallback* callback);
int backgroundSoundGetDuration();
int backgroundSoundLoad(const char* fileName, int a2, int a3, int a4);
int _gsound_background_play_level_music(const char* a1, int a2);
void backgroundSoundDelete();
void backgroundSoundRestart(int value);
void backgroundSoundPause();
void backgroundSoundResume();
void speechDisable();
void speechEnable();
int speechIsEnabled();
void speechSetVolume(int value);
int speechGetVolume();
int _gsound_speech_volume_get_set(int volume);
void speechSetEndCallback(SoundEndCallback* callback);
int speechGetDuration();
int speechLoad(const char* fname, int a2, int a3, int a4);
int _gsound_speech_play_preloaded();
void speechDelete();
void speechPause();
void speechResume();
int _gsound_play_sfx_file_volume(const char* a1, int a2);
Sound* soundEffectLoad(const char* name, Object* a2);
Sound* soundEffectLoadWithVolume(const char* a1, Object* a2, int a3);
void soundEffectDelete(Sound* a1);
int _gsnd_anim_sound(Sound* a1);
int soundEffectPlay(Sound* a1);
int _gsound_compute_relative_volume(Object* obj);
char* sfxBuildCharName(Object* a1, int anim, int extra);
char* gameSoundBuildAmbientSoundEffectName(const char* a1);
char* gameSoundBuildInterfaceName(const char* a1);
char* sfxBuildWeaponName(int effectType, Object* weapon, int hitMode, Object* target);
char* sfxBuildSceneryName(int actionType, int action, const char* name);
char* sfxBuildOpenName(Object* a1, int a2);
void _gsound_red_butt_press(int btn, int keyCode);
void _gsound_red_butt_release(int btn, int keyCode);
void _gsound_toggle_butt_press_(int btn, int keyCode);
void _gsound_med_butt_press(int btn, int keyCode);
void _gsound_med_butt_release(int btn, int keyCode);
void _gsound_lrg_butt_press(int btn, int keyCode);
void _gsound_lrg_butt_release(int btn, int keyCode);
int soundPlayFile(const char* name);
void _gsound_bkg_proc();
int gameSoundFileOpen(const char* fname, int access, ...);
long _gsound_write_();
long gameSoundFileTellNotImplemented(int handle);
int gameSoundFileWrite(int handle, const void* buf, unsigned int size);
int gameSoundFileClose(int handle);
int gameSoundFileRead(int handle, void* buf, unsigned int size);
long gameSoundFileSeek(int handle, long offset, int origin);
long gameSoundFileTell(int handle);
long gameSoundFileGetSize(int handle);
bool gameSoundIsCompressed(char* filePath);
void speechCallback(void* userData, int a2);
void backgroundSoundCallback(void* userData, int a2);
void soundEffectCallback(void* userData, int a2);
int _gsound_background_allocate(Sound** out_s, int a2, int a3);
int gameSoundFindBackgroundSoundPathWithCopy(char* dest, const char* src);
int gameSoundFindBackgroundSoundPath(char* dest, const char* src);
int gameSoundFindSpeechSoundPath(char* dest, const char* src);
void gameSoundDeleteOldMusicFile();
int backgroundSoundPlay();
int speechPlay();
int _gsound_get_music_path(char** out_value, const char* key);
Sound* _gsound_get_sound_ready_for_effect();
bool _gsound_file_exists_f(const char* fname);
int _gsound_setup_paths();
int _gsound_sfx_q_start();
int ambientSoundEffectEventProcess(Object* a1, void* a2);

#endif /* GAME_SOUND_H */
