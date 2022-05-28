#ifndef ANIMATION_H
#define ANIMATION_H

#include "art.h"
#include "combat_defs.h"
#include "obj_types.h"
#include "sound.h"

#define ANIMATION_SEQUENCE_LIST_CAPACITY (32)
#define ANIMATION_DESCRIPTION_LIST_CAPACITY (55)

typedef enum AnimKind {
    ANIM_KIND_OBJ_MOVE_TO_OBJ = 0,
    ANIM_KIND_OBJ_MOVE_TO_TILE = 1,
    ANIM_KIND_2 = 2,
    ANIM_KIND_KNOCKDOWN = 3,
    ANIM_KIND_ANIMATE = 4,
    ANIM_KIND_ANIMATE_REVERSE = 5,
    ANIM_KIND_6 = 6,
    ANIM_KIND_SET_ROTATION_TO_TILE = 7,
    ANIM_KIND_ROTATE_CLOCKWISE = 8,
    ANIM_KIND_ROTATE_COUNTER_CLOCKWISE = 9,
    ANIM_KIND_HIDE = 10,
    ANIM_KIND_EXEC = 11,
    ANIM_KIND_EXEC_2 = 12,
    ANIM_KIND_14 = 14,
    ANIM_KIND_15 = 15,
    ANIM_KIND_16 = 16,
    ANIM_KIND_17 = 17,
    ANIM_KIND_18 = 18,
    ANIM_KIND_19 = 19,
    ANIM_KIND_20 = 20,
    ANIM_KIND_23 = 23,
    ANIM_KIND_24 = 24,
    ANIM_KIND_ANIMATE_FOREVER = 25,
    ANIM_KIND_26 = 26,
    ANIM_KIND_27 = 27,
    ANIM_KIND_28 = 28,
} AnimKind;

// Basic animations: 0-19
// Knockdown and death: 20-35
// Change positions: 36-37
// Weapon: 38-47
// Single-frame death animations (the last frame of knockdown and death animations): 48-63
typedef enum AnimationType {
    ANIM_STAND = 0,
    ANIM_WALK = 1,
    ANIM_JUMP_BEGIN = 2,
    ANIM_JUMP_END = 3,
    ANIM_CLIMB_LADDER = 4,
    ANIM_FALLING = 5,
    ANIM_UP_STAIRS_RIGHT = 6,
    ANIM_UP_STAIRS_LEFT = 7,
    ANIM_DOWN_STAIRS_RIGHT = 8,
    ANIM_DOWN_STAIRS_LEFT = 9,
    ANIM_MAGIC_HANDS_GROUND = 10,
    ANIM_MAGIC_HANDS_MIDDLE = 11,
    ANIM_MAGIC_HANDS_UP = 12,
    ANIM_DODGE_ANIM = 13,
    ANIM_HIT_FROM_FRONT = 14,
    ANIM_HIT_FROM_BACK = 15,
    ANIM_THROW_PUNCH = 16,
    ANIM_KICK_LEG = 17,
    ANIM_THROW_ANIM = 18,
    ANIM_RUNNING = 19,
    ANIM_FALL_BACK = 20,
    ANIM_FALL_FRONT = 21,
    ANIM_BAD_LANDING = 22,
    ANIM_BIG_HOLE = 23,
    ANIM_CHARRED_BODY = 24,
    ANIM_CHUNKS_OF_FLESH = 25,
    ANIM_DANCING_AUTOFIRE = 26,
    ANIM_ELECTRIFY = 27,
    ANIM_SLICED_IN_HALF = 28,
    ANIM_BURNED_TO_NOTHING = 29,
    ANIM_ELECTRIFIED_TO_NOTHING = 30,
    ANIM_EXPLODED_TO_NOTHING = 31,
    ANIM_MELTED_TO_NOTHING = 32,
    ANIM_FIRE_DANCE = 33,
    ANIM_FALL_BACK_BLOOD = 34,
    ANIM_FALL_FRONT_BLOOD = 35,
    ANIM_PRONE_TO_STANDING = 36,
    ANIM_BACK_TO_STANDING = 37,
    ANIM_TAKE_OUT = 38,
    ANIM_PUT_AWAY = 39,
    ANIM_PARRY_ANIM = 40,
    ANIM_THRUST_ANIM = 41,
    ANIM_SWING_ANIM = 42,
    ANIM_POINT = 43,
    ANIM_UNPOINT = 44,
    ANIM_FIRE_SINGLE = 45,
    ANIM_FIRE_BURST = 46,
    ANIM_FIRE_CONTINUOUS = 47,
    ANIM_FALL_BACK_SF = 48,
    ANIM_FALL_FRONT_SF = 49,
    ANIM_BAD_LANDING_SF = 50,
    ANIM_BIG_HOLE_SF = 51,
    ANIM_CHARRED_BODY_SF = 52,
    ANIM_CHUNKS_OF_FLESH_SF = 53,
    ANIM_DANCING_AUTOFIRE_SF = 54,
    ANIM_ELECTRIFY_SF = 55,
    ANIM_SLICED_IN_HALF_SF = 56,
    ANIM_BURNED_TO_NOTHING_SF = 57,
    ANIM_ELECTRIFIED_TO_NOTHING_SF = 58,
    ANIM_EXPLODED_TO_NOTHING_SF = 59,
    ANIM_MELTED_TO_NOTHING_SF = 60,
    ANIM_FIRE_DANCE_SF = 61,
    ANIM_FALL_BACK_BLOOD_SF = 62,
    ANIM_FALL_FRONT_BLOOD_SF = 63,
    ANIM_CALLED_SHOT_PIC = 64,
    ANIM_COUNT = 65,
    FIRST_KNOCKDOWN_AND_DEATH_ANIM = ANIM_FALL_BACK,
    LAST_KNOCKDOWN_AND_DEATH_ANIM = ANIM_FALL_FRONT_BLOOD,
    FIRST_SF_DEATH_ANIM = ANIM_FALL_BACK_SF,
    LAST_SF_DEATH_ANIM = ANIM_FALL_FRONT_BLOOD_SF,
} AnimationType;

typedef int AnimationProc(Object*, Object*);
typedef int AnimationSoundProc(Sound*);
typedef int AnimationProc2(Object*, Object*, void*);

typedef struct AnimationDescription {
    int type;
    Object* owner;
    union {
        Object* destinationObj;
        Sound* sound;
    };
    union {
        int tile;
        int fid; // for type == 17
        int weaponAnimationCode; // for type == 18
        int lightDistance; // for type == 19
    };
    int elevation;
    int anim; // anim
    int delay; // delay
    union {
        AnimationProc* proc;
        AnimationSoundProc* soundProc;
    };
    AnimationProc2* field_20; // func
    int field_24;
    union {
        int field_28; // actionPoints
        Object* field_28_obj; // obj in type == 12
        void* field_28_void;
    };
    CacheEntry* field_2C;
} AnimationDescription;

typedef struct AnimationSequence {
    int field_0;
    // Index of current animation in [animations] array or -1 if animations in
    // this sequence is not playing.
    int animationIndex;
    // Number of scheduled animations in [animations] array.
    int length;
    int flags;
    AnimationDescription animations[ANIMATION_DESCRIPTION_LIST_CAPACITY];
} AnimationSequence;

typedef struct PathNode {
    int tile;
    int from;
    // actual type is likely char
    int rotation;
    int field_C;
    int field_10;
} PathNode;

typedef struct STRUCT_530014_28 {
    int tile;
    int elevation;
    int x;
    int y;
} STRUCT_530014_28;

typedef struct STRUCT_530014 {
    int flags; // flags
    Object* obj;
    int fid; // fid
    int field_C;
    int field_10;
    int field_14; // animation speed?
    int animationSequenceIndex;
    int field_1C; // length of field_28
    int field_20; // current index in field_28
    int field_24;
    union {
        unsigned char rotations[3200];
        STRUCT_530014_28 field_28[200];
    };
} STRUCT_530014;

typedef Object* PathBuilderCallback(Object* object, int tile, int elevation);

extern int _curr_sad;
extern int gAnimationSequenceCurrentIndex;
extern int _anim_in_init;
extern bool _anim_in_anim_stop;
extern bool _anim_in_bk;
extern int _lastDestination;
extern unsigned int _last_time_;
extern unsigned int _next_time;

extern STRUCT_530014 _sad[24];
extern PathNode gClosedPathNodeList[2000];
extern AnimationSequence gAnimationSequences[32];
extern unsigned char gPathfinderProcessedTiles[5000];
extern PathNode gOpenPathNodeList[2000];
extern int gAnimationDescriptionCurrentIndex;
extern Object* dword_56C7E0[100];

void animationInit();
void animationReset();
void animationExit();
int reg_anim_begin(int a1);
int _anim_free_slot(int a1);
int _register_priority(int a1);
int reg_anim_clear(Object* a1);
int reg_anim_end();
void _anim_cleanup();
int _check_registry(Object* obj);
int animationIsBusy(Object* a1);
int reg_anim_obj_move_to_obj(Object* a1, Object* a2, int actionPoints, int delay);
int reg_anim_obj_run_to_obj(Object* owner, Object* destination, int actionPoints, int delay);
int reg_anim_obj_move_to_tile(Object* obj, int tile_num, int elev, int actionPoints, int delay);
int reg_anim_obj_run_to_tile(Object* obj, int tile_num, int elev, int actionPoints, int delay);
int reg_anim_2(Object* obj, int tile_num, int elev, int a4, int a5);
int reg_anim_knockdown(Object* obj, int tile, int elev, int anim, int delay);
int reg_anim_animate(Object* obj, int anim, int delay);
int reg_anim_animate_reverse(Object* obj, int anim, int delay);
int reg_anim_6(Object* obj, int anim, int delay);
int reg_anim_set_rotation_to_tile(Object* owner, int tile);
int reg_anim_rotate_clockwise(Object* obj);
int reg_anim_rotate_counter_clockwise(Object* obj);
int reg_anim_hide(Object* obj);
int reg_anim_11_0(Object* a1, Object* a2, AnimationProc* proc, int delay);
int reg_anim_12(Object* a1, Object* a2, void* a3, AnimationProc2* proc, int delay);
int reg_anim_11_1(Object* a1, Object* a2, AnimationProc* proc, int delay);
int reg_anim_15(Object* obj, int a2, int a3);
int reg_anim_17(Object* obj, int fid, int a3);
int reg_anim_18(Object* obj, int a2, int a3);
int reg_anim_update_light(Object* obj, int fid, int a3);
int reg_anim_play_sfx(Object* obj, const char* a2, int a3);
int reg_anim_animate_forever(Object* obj, int a2, int a3);
int reg_anim_26(int a1, int a2);
int animationRunSequence(int a1);
int _anim_set_continue(int a1, int a2);
int _anim_set_end(int a1);
bool canUseDoor(Object* critter, Object* door);
int _make_path(Object* object, int from, int to, unsigned char* a4, int a5);
int pathfinderFindPath(Object* object, int from, int to, unsigned char* rotations, int a5, PathBuilderCallback* callback);
int _idist(int a1, int a2, int a3, int a4);
int _tile_idistance(int tile1, int tile2);
int _make_straight_path(Object* a1, int from, int to, STRUCT_530014_28* pathNodes, Object** a5, int a6);
int _make_straight_path_func(Object* a1, int from, int to, STRUCT_530014_28* a4, Object** a5, int a6, Object* (*a7)(Object*, int, int));
int animateMoveObjectToObject(Object* a1, Object* a2, int a3, int a4, int a5);
int animateMoveObjectToTile(Object* obj, int tile_num, int elev, int a4, int a5, int a6);
int _anim_move(Object* obj, int tile, int elev, int a3, int a4, int a5, int animationSequenceIndex);
int _anim_move_straight_to_tile(Object* obj, int a2, int a3, int fid, int a5, int a6);
int _anim_move_on_stairs(Object* obj, int a2, int a3, int fid, int a5);
int _check_for_falling(Object* obj, int a2, int a3);
void _object_move(int index);
void _object_straight_move(int a1);
int _anim_animate(Object* obj, int a2, int a3, int a4);
void _object_animate();
void _object_anim_compact();
int _check_move(int* a1);
int _dude_move(int a1);
int _dude_run(int a1);
void _dude_fidget();
void _dude_stand(Object* obj, int rotation, int fid);
void _dude_standup(Object* a1);
int actionRotate(Object* obj, int a2, int a3);
int _anim_change_fid(Object* obj, int a2, int fid);
void _anim_stop();
int _check_gravity(int tile, int elevation);
unsigned int _compute_tpf(Object* object, int fid);

#endif /* ANIMATION_H */
