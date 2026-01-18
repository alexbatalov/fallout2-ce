#ifndef GAME_SOUND_H
#define GAME_SOUND_H

#include "obj_types.h"
#include "sound.h"

namespace fallout {

typedef enum WeaponSoundEffect {
    WEAPON_SOUND_EFFECT_READY,
    WEAPON_SOUND_EFFECT_ATTACK,
    WEAPON_SOUND_EFFECT_OUT_OF_AMMO,
    WEAPON_SOUND_EFFECT_AMMO_FLYING,
    WEAPON_SOUND_EFFECT_HIT,
    WEAPON_SOUND_EFFECT_COUNT,
} WeaponSoundEffect;

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

extern int gMusicVolume;

int gameSoundInit();
void gameSoundReset();
int gameSoundExit();
int gameSoundSetMasterVolume(int value);
int gameSoundGetMasterVolume();
int soundEffectsSetVolume(int value);
int backgroundSoundIsEnabled();
void backgroundSoundSetVolume(int value);
int backgroundSoundGetVolume();
int _gsound_background_volume_get_set(int a1);
void backgroundSoundSetEndCallback(SoundEndCallback* callback);
int backgroundSoundLoad(const char* fileName, int a2, int a3, int a4);
int _gsound_background_play_level_music(const char* a1, int a2);
void backgroundSoundDelete();
void backgroundSoundRestart(int value);
void backgroundSoundPause();
void backgroundSoundResume();
bool backgoundSoundIsPlaying();
int speechIsEnabled();
void speechSetVolume(int value);
int speechGetVolume();
void speechSetEndCallback(SoundEndCallback* callback);
int speechGetDuration();
int speechLoad(const char* fname, int a2, int a3, int a4);
int _gsound_speech_play_preloaded();
void speechDelete();
int _gsound_play_sfx_file_volume(const char* a1, int a2);
Sound* soundEffectLoad(const char* name, Object* a2);
Sound* soundEffectLoadWithVolume(const char* a1, Object* a2, int a3);
void soundEffectDelete(Sound* a1);
int _gsnd_anim_sound(Sound* sound, void* a2);
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
int _gsound_sfx_q_start();
int ambientSoundEffectEventProcess(Object* a1, void* a2);

} // namespace fallout

#endif /* GAME_SOUND_H */
