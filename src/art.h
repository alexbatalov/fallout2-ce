#ifndef ART_H
#define ART_H

#include "cache.h"
#include "db.h"
#include "heap.h"
#include "obj_types.h"
#include "platform_compat.h"
#include "proto_types.h"

namespace fallout {

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

typedef struct Art {
    int field_0;
    short framesPerSecond;
    short actionFrame;
    short frameCount;
    short xOffsets[6];
    short yOffsets[6];
    int dataOffsets[6];
    int padding[6];
    int dataSize;
} Art;

typedef struct ArtFrame {
    short width;
    short height;
    int size;
    short x;
    short y;
} ArtFrame;

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

extern int _art_vault_guy_num;
extern int _art_vault_person_nums[DUDE_NATIVE_LOOK_COUNT][GENDER_COUNT];

extern Cache gArtCache;

int artInit();
void artReset();
void artExit();
char* artGetObjectTypeName(int objectType);
int artIsObjectTypeHidden(int objectType);
int artGetFidgetCount(int headFid);
void artRender(int fid, unsigned char* dest, int width, int height, int pitch);
int art_list_str(int fid, char* name);
Art* artLock(int fid, CacheEntry** cache_entry);
unsigned char* artLockFrameData(int fid, int frame, int direction, CacheEntry** out_cache_entry);
unsigned char* artLockFrameDataReturningSize(int fid, CacheEntry** out_cache_entry, int* widthPtr, int* heightPtr);
int artUnlock(CacheEntry* cache_entry);
int artCacheFlush();
int artCopyFileName(int objectType, int id, char* a3);
int _art_get_code(int animation, int weaponType, char* a3, char* a4);
char* artBuildFilePath(int fid);
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
int _art_alias_num(int index);
int artCritterFidShouldRun(int fid);
int artAliasFid(int fid);
int buildFid(int objectType, int frmId, int animType, int weaponCode, int rotation);
Art* artLoad(const char* path);
int artRead(const char* path, unsigned char* data);
int artWrite(const char* path, unsigned char* data);

class FrmImage {
public:
    FrmImage();
    ~FrmImage();

    bool isLocked() const { return _key != nullptr; }
    bool lock(unsigned int fid);
    void unlock();

    int getWidth() const { return _width; }
    int getHeight() const { return _height; }
    unsigned char* getData() const { return _data; }

private:
    CacheEntry* _key;
    unsigned char* _data;
    int _width;
    int _height;
};

} // namespace fallout

#endif
