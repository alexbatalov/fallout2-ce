#ifndef ART_H
#define ART_H

#include "cache.h"
#include "db.h"
#include "heap.h"
#include "obj_types.h"
#include "proto_types.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

typedef enum Head {
    HEAD_INVALID,
    HEAD_MARCUS,
    HEAD_MYRON,
    HEAD_ELDER,
    HEAD_LYNETTE,
    HEAD_HAROLD,
    HEAD_TANDI,
    HEAD_COM_OFFICER,
    HEAD_SULIK,
    HEAD_PRESIDENT,
    HEAD_HAKUNIN,
    HEAD_BOSS,
    HEAD_DYING_HAKUNIN,
    HEAD_COUNT,
} Head;

typedef enum HeadAnimation {
    HEAD_ANIMATION_VERY_GOOD_REACTION = 0,
    FIDGET_GOOD = 1,
    HEAD_ANIMATION_GOOD_TO_NEUTRAL = 2,
    HEAD_ANIMATION_NEUTRAL_TO_GOOD = 3,
    FIDGET_NEUTRAL = 4,
    HEAD_ANIMATION_NEUTRAL_TO_BAD = 5,
    HEAD_ANIMATION_BAD_TO_NEUTRAL = 6,
    FIDGET_BAD = 7,
    HEAD_ANIMATION_VERY_BAD_REACTION = 8,
    HEAD_ANIMATION_GOOD_PHONEMES = 9,
    HEAD_ANIMATION_NEUTRAL_PHONEMES = 10,
    HEAD_ANIMATION_BAD_PHONEMES = 11,
} HeadAnimation;

typedef enum Background {
    BACKGROUND_0,
    BACKGROUND_1,
    BACKGROUND_2,
    BACKGROUND_HUB,
    BACKGROUND_NECROPOLIS,
    BACKGROUND_BROTHERHOOD,
    BACKGROUND_MILITARY_BASE,
    BACKGROUND_JUNK_TOWN,
    BACKGROUND_CATHEDRAL,
    BACKGROUND_SHADY_SANDS,
    BACKGROUND_VAULT,
    BACKGROUND_MASTER,
    BACKGROUND_FOLLOWER,
    BACKGROUND_RAIDERS,
    BACKGROUND_CAVE,
    BACKGROUND_ENCLAVE,
    BACKGROUND_WASTELAND,
    BACKGROUND_BOSS,
    BACKGROUND_PRESIDENT,
    BACKGROUND_TENT,
    BACKGROUND_ADOBE,
    BACKGROUND_COUNT,
} Background;

#pragma pack(2)
typedef struct Art {
    int field_0;
    short framesPerSecond;
    short actionFrame;
    short frameCount;
    short xOffsets[6];
    short yOffsets[6];
    int dataOffsets[6];
    int field_3A;
} Art;
#pragma pack()

typedef struct ArtFrame {
    short width;
    short height;
    int size;
    short x;
    short y;
} ArtFrame;

typedef struct ArtListDescription {
    int flags;
    char name[16];
    char* fileNames; // dynamic array of null terminated strings 13 bytes long each
    void* field_18;
    int fileNamesLength; // number of entries in list
} ArtListDescription;

typedef struct HeadDescription {
    int goodFidgetCount;
    int neutralFidgetCount;
    int badFidgetCount;
} HeadDescription;

typedef enum WeaponAnimation {
    WEAPON_ANIMATION_NONE,
    WEAPON_ANIMATION_KNIFE, // d
    WEAPON_ANIMATION_CLUB, // e
    WEAPON_ANIMATION_HAMMER, // f
    WEAPON_ANIMATION_SPEAR, // g
    WEAPON_ANIMATION_PISTOL, // h
    WEAPON_ANIMATION_SMG, // i
    WEAPON_ANIMATION_SHOTGUN, // j
    WEAPON_ANIMATION_LASER_RIFLE, // k
    WEAPON_ANIMATION_MINIGUN, // l
    WEAPON_ANIMATION_LAUNCHER, // m
    WEAPON_ANIMATION_COUNT,
} WeaponAnimation;

typedef enum DudeNativeLook {
    // Hero looks as one the tribals (before finishing Temple of Trails).
    DUDE_NATIVE_LOOK_TRIBAL,

    // Hero have finished Temple of Trails and received Vault Jumpsuit.
    DUDE_NATIVE_LOOK_JUMPSUIT,
    DUDE_NATIVE_LOOK_COUNT,
} DudeNativeLook;

extern ArtListDescription gArtListDescriptions[OBJ_TYPE_COUNT];
extern bool gArtLanguageInitialized;
extern const char* _head1;
extern const char* _head2;
extern int _art_vault_guy_num;
extern int _art_vault_person_nums[DUDE_NATIVE_LOOK_COUNT][GENDER_COUNT];
extern int _art_mapper_blank_tile;

extern char gArtLanguage[32];
extern Cache gArtCache;
extern char _art_name[MAX_PATH];
extern HeadDescription* gHeadDescriptions;
extern int* _anon_alias;
extern int* gArtCritterFidShoudRunData;

int artInit();
void artReset();
void artExit();
char* artGetObjectTypeName(int objectType);
int artIsObjectTypeHidden(int objectType);
int artGetFidgetCount(int headFid);
void artRender(int fid, unsigned char* dest, int width, int height, int pitch);
Art* artLock(int fid, CacheEntry** cache_entry);
unsigned char* artLockFrameData(int fid, int frame, int direction, CacheEntry** out_cache_entry);
unsigned char* artLockFrameDataReturningSize(int fid, CacheEntry** out_cache_entry, int* widthPtr, int* heightPtr);
int artUnlock(CacheEntry* cache_entry);
int artCacheFlush();
int artCopyFileName(int a1, int a2, char* a3);
int _art_get_code(int a1, int a2, char* a3, char* a4);
char* artBuildFilePath(int a1);
int artReadList(const char* path, char** out_arr, int* out_count);
int artGetFramesPerSecond(Art* art);
int artGetActionFrame(Art* art);
int artGetFrameCount(Art* art);
int artGetWidth(Art* art, int frame, int direction);
int artGetHeight(Art* art, int frame, int direction);
int artGetSize(Art* art, int frame, int direction, int* out_width, int* out_height);
int artGetFrameOffsets(Art* art, int frame, int direction, int* a4, int* a5);
int artGetRotationOffsets(Art* art, int rotation, int* out_offset_x, int* out_offset_y);
unsigned char* artGetFrameData(Art* art, int frame, int direction);
ArtFrame* artGetFrame(Art* art, int frame, int direction);
bool artExists(int fid);
bool _art_fid_valid(int fid);
int _art_alias_num(int a1);
int artCritterFidShouldRun(int a1);
int _art_alias_fid(int a1);
int artCacheGetFileSizeImpl(int a1, int* out_size);
int artCacheReadDataImpl(int a1, int* a2, unsigned char* data);
void artCacheFreeImpl(void* ptr);
int buildFid(int a1, int a2, int a3, int a4, int a5);
int artReadFrameData(unsigned char* data, File* stream, int count);
int artReadHeader(Art* art, File* stream);
int artRead(const char* path, unsigned char* data);

#endif
