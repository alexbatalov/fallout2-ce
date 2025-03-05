#include "animation.h"

#include <stdio.h>
#include <string.h>

#include "art.h"
#include "color.h"
#include "combat.h"
#include "combat_ai.h"
#include "critter.h"
#include "debug.h"
#include "display_monitor.h"
#include "game.h"
#include "game_mouse.h"
#include "game_sound.h"
#include "geometry.h"
#include "input.h"
#include "interface.h"
#include "item.h"
#include "kb.h"
#include "map.h"
#include "mouse.h"
#include "object.h"
#include "party_member.h"
#include "perk.h"
#include "proto.h"
#include "proto_instance.h"
#include "random.h"
#include "scripts.h"
#include "settings.h"
#include "stat.h"
#include "svga.h"
#include "text_object.h"
#include "tile.h"
#include "trait.h"
#include "worldmap.h"

namespace fallout {

#define ANIMATION_SEQUENCE_LIST_CAPACITY 32
#define ANIMATION_DESCRIPTION_LIST_CAPACITY 55
#define ANIMATION_SAD_LIST_CAPACITY 24

#define ANIMATION_SEQUENCE_FORCED 0x01

typedef enum AnimationKind {
    ANIM_KIND_MOVE_TO_OBJECT = 0,
    ANIM_KIND_MOVE_TO_TILE = 1,
    ANIM_KIND_MOVE_TO_TILE_STRAIGHT = 2,
    ANIM_KIND_MOVE_TO_TILE_STRAIGHT_AND_WAIT_FOR_COMPLETE = 3,
    ANIM_KIND_ANIMATE = 4,
    ANIM_KIND_ANIMATE_REVERSED = 5,
    ANIM_KIND_ANIMATE_AND_HIDE = 6,
    ANIM_KIND_ROTATE_TO_TILE = 7,
    ANIM_KIND_ROTATE_CLOCKWISE = 8,
    ANIM_KIND_ROTATE_COUNTER_CLOCKWISE = 9,
    ANIM_KIND_HIDE = 10,
    ANIM_KIND_CALLBACK = 11,
    ANIM_KIND_CALLBACK3 = 12,
    ANIM_KIND_SET_FLAG = 14,
    ANIM_KIND_UNSET_FLAG = 15,
    ANIM_KIND_TOGGLE_FLAT = 16,
    ANIM_KIND_SET_FID = 17,
    ANIM_KIND_TAKE_OUT_WEAPON = 18,
    ANIM_KIND_SET_LIGHT_DISTANCE = 19,
    ANIM_KIND_MOVE_ON_STAIRS = 20,
    ANIM_KIND_CHECK_FALLING = 23,
    ANIM_KIND_TOGGLE_OUTLINE = 24,
    ANIM_KIND_ANIMATE_FOREVER = 25,
    ANIM_KIND_PING = 26,
    ANIM_KIND_CONTINUE = 28,

    // New animation to update both light distance and intensity. Required to
    // impement Sfall's explosion light effects without resorting to hackery.
    ANIM_KIND_SET_LIGHT_INTENSITY,
} AnimationKind;

typedef enum AnimationSequenceFlags {
    // Specifies that the animation sequence has high priority, it cannot be
    // cleared.
    ANIM_SEQ_PRIORITIZED = 0x01,

    // Specifies that the animation sequence started combat animation mode and
    // therefore should balance it with appropriate finish call.
    ANIM_SEQ_COMBAT_ANIM_STARTED = 0x02,

    // Specifies that the animation sequence is reserved (TODO: explain what it
    // actually means).
    ANIM_SEQ_RESERVED = 0x04,

    // Specifies that the animation sequence is in the process of adding actions
    // to it (that is in the middle of begin/end calls).
    ANIM_SEQ_ACCUMULATING = 0x08,

    // TODO: Add description.
    ANIM_SEQ_0x10 = 0x10,

    // TODO: Add description.
    ANIM_SEQ_0x20 = 0x20,

    // Specifies that the animation sequence is negligible and will be end
    // immediately when a new animation sequence is requested for the same
    // object.
    ANIM_SEQ_INSIGNIFICANT = 0x40,

    // Specifies that the animation sequence should not return to ANIM_STAND
    // when it's completed.
    ANIM_SEQ_NO_STAND = 0x80,
} AnimationSequenceFlags;

typedef enum AnimationSadFlags {
    // Specifies that the animation should play from end to start.
    ANIM_SAD_REVERSE = 0x01,

    // Specifies that the animation should use straight movement mode (as
    // opposed to normal movement mode).
    ANIM_SAD_STRAIGHT = 0x02,

    // Specifies that no frame change should occur during animation.
    ANIM_SAD_NO_ANIM = 0x04,

    // Specifies that the animation should be played fully from start to finish.
    //
    // NOTE: This flag is only used together with straight movement mode to
    // implement knockdown. Without this flag when the knockdown distance is
    // short, say 1 or 2 tiles, knockdown animation might not be completed by
    // the time critter reached it's destination. With this flag set animation
    // will be played to it's final frame.
    ANIM_SAD_WAIT_FOR_COMPLETION = 0x10,

    // Unknown, set once, never read.
    ANIM_SAD_0x20 = 0x20,

    // Specifies that the owner of the animation should be hidden when animation
    // is completed.
    ANIM_SAD_HIDE_ON_END = 0x40,

    // Specifies that the animation should never end.
    ANIM_SAD_FOREVER = 0x80,
} AnimationSadFlags;

typedef struct AnimationDescription {
    int kind;
    union {
        Object* owner;

        // - ANIM_KIND_CALLBACK
        // - ANIM_KIND_CALLBACK3
        void* param2;
    };

    union {
        // - ANIM_KIND_MOVE_TO_OBJECT
        Object* destination;

        // - ANIM_KIND_CALLBACK
        void* param1;
    };
    union {
        // - ANIM_KIND_MOVE_TO_TILE
        // - ANIM_KIND_ANIMATE_AND_MOVE_TO_TILE_STRAIGHT
        // - ANIM_KIND_MOVE_TO_TILE_STRAIGHT
        struct {
            int tile;
            int elevation;
        };

        // ANIM_KIND_SET_FID
        int fid;

        // ANIM_KIND_TAKE_OUT_WEAPON
        int weaponAnimationCode;

        // ANIM_KIND_SET_LIGHT_DISTANCE
        int lightDistance;

        // ANIM_KIND_TOGGLE_OUTLINE
        bool outline;
    };
    int anim;
    int delay;

    // ANIM_KIND_CALLBACK
    AnimationCallback* callback;

    // ANIM_KIND_CALLBACK3
    AnimationCallback3* callback3;

    union {
        // - ANIM_KIND_SET_FLAG
        // - ANIM_KIND_UNSET_FLAG
        unsigned int objectFlag;

        // - ANIM_KIND_HIDE
        // - ANIM_KIND_CALLBACK
        unsigned int extendedFlags;
    };

    union {
        // - ANIM_KIND_MOVE_TO_TILE
        // - ANIM_KIND_MOVE_TO_OBJECT
        int actionPoints;

        // ANIM_KIND_26
        int animationSequenceIndex;

        // ANIM_KIND_CALLBACK3
        void* param3;

        // ANIM_KIND_SET_LIGHT_INTENSITY
        int lightIntensity;
    };
    CacheEntry* artCacheKey;
} AnimationDescription;

typedef struct AnimationSequence {
    int field_0;
    // Index of current animation in [animations] array or -1 if animations in
    // this sequence is not playing.
    int animationIndex;
    // Number of scheduled animations in [animations] array.
    int length;
    unsigned int flags;
    AnimationDescription animations[ANIMATION_DESCRIPTION_LIST_CAPACITY];
} AnimationSequence;

typedef struct PathNode {
    int tile;
    int from;
    // actual type is likely char
    int rotation;
    int estimate;
    int cost;
} PathNode;

// TODO: I don't know what `sad` means, but it's definitely better than
// `STRUCT_530014`. Find a better name.
typedef struct AnimationSad {
    unsigned int flags;
    Object* obj;
    int fid; // fid
    int anim;

    // Timestamp (in game ticks) when animation last occurred.
    unsigned int animationTimestamp;

    // Number of ticks per frame (taking art's fps and overall animation speed
    // settings into account).
    unsigned int ticksPerFrame;

    int animationSequenceIndex;
    int field_1C; // length of field_28
    int field_20; // current index in field_28
    int field_24;
    union {
        unsigned char rotations[3200];
        StraightPathNode straightPathNodeList[200];
    };
} AnimationSad;

static int _anim_free_slot(int a1);
static int _anim_preload(Object* object, int fid, CacheEntry** cacheEntryPtr);
static void _anim_cleanup();
static int _check_registry(Object* obj);
static int animationRunSequence(int a1);
static int _anim_set_continue(int a1, int a2);
static int _anim_set_end(int a1);
static bool canUseDoor(Object* critter, Object* door);
static int _idist(int a1, int a2, int a3, int a4);
static int _tile_idistance(int tile1, int tile2);
static int animateMoveObjectToObject(Object* from, Object* to, int actionPoints, int anim, int animationSequenceIndex);
static int animateMoveObjectToTile(Object* obj, int tile, int elev, int actionPoints, int anim, int animationSequenceIndex);
static int _anim_move(Object* obj, int tile, int elev, int a3, int anim, int a5, int animationSequenceIndex);
static int animateMoveObjectToTileStraight(Object* obj, int tile, int elevation, int anim, int animationSequenceIndex, int flags);
static int _anim_move_on_stairs(Object* obj, int tile, int elevation, int anim, int animationSequenceIndex);
static int _check_for_falling(Object* obj, int anim, int a3);
static void _object_move(int index);
static void _object_straight_move(int index);
static int _anim_animate(Object* obj, int anim, int animationSequenceIndex, int flags);
static void _object_anim_compact();
static int actionRotate(Object* obj, int delta, int animationSequenceIndex);
static int _anim_hide(Object* object, int animationSequenceIndex);
static int _anim_change_fid(Object* obj, int animationSequenceIndex, int fid);
static int _check_gravity(int tile, int elevation);
static unsigned int animationComputeTicksPerFrame(Object* object, int fid);

static void reportOverloaded(Object* critter);

// 0x510718
static int gAnimationCurrentSad = 0;

// 0x51071C
static int gAnimationSequenceCurrentIndex = -1;

// 0x510720
static bool gAnimationInInit = false;

// 0x510724
static bool gAnimationInStop = false;

// 0x510728
static bool _anim_in_bk = false;

// 0x530014
static AnimationSad gAnimationSads[ANIMATION_SAD_LIST_CAPACITY];

// 0x542FD4
static PathNode gClosedPathNodeList[2000];

// 0x54CC14
static AnimationSequence gAnimationSequences[32];

// 0x561814
static unsigned char gPathfinderProcessedTiles[5000];

// 0x562B9C
static PathNode gOpenPathNodeList[2000];

// 0x56C7DC
static int gAnimationDescriptionCurrentIndex;

// anim_init
// 0x413A20
void animationInit()
{
    gAnimationInInit = true;
    animationReset();
    gAnimationInInit = false;
}

// 0x413A40
void animationReset()
{
    if (!gAnimationInInit) {
        // NOTE: Uninline.
        animationStop();
    }

    gAnimationCurrentSad = 0;
    gAnimationSequenceCurrentIndex = -1;

    for (int index = 0; index < ANIMATION_SEQUENCE_LIST_CAPACITY; index++) {
        gAnimationSequences[index].field_0 = -1000;
        gAnimationSequences[index].flags = 0;
    }
}

// 0x413AB8
void animationExit()
{
    // NOTE: Uninline.
    animationStop();
}

// 0x413AF4
int reg_anim_begin(int requestOptions)
{
    if (gAnimationSequenceCurrentIndex != -1) {
        return -1;
    }

    if (gAnimationInStop) {
        return -1;
    }

    int v1 = _anim_free_slot(requestOptions);
    if (v1 == -1) {
        return -1;
    }

    AnimationSequence* animationSequence = &(gAnimationSequences[v1]);
    animationSequence->flags |= ANIM_SEQ_ACCUMULATING;

    if ((requestOptions & ANIMATION_REQUEST_RESERVED) != 0) {
        animationSequence->flags |= ANIM_SEQ_RESERVED;
    }

    if ((requestOptions & ANIMATION_REQUEST_INSIGNIFICANT) != 0) {
        animationSequence->flags |= ANIM_SEQ_INSIGNIFICANT;
    }

    if ((requestOptions & ANIMATION_REQUEST_NO_STAND) != 0) {
        animationSequence->flags |= ANIM_SEQ_NO_STAND;
    }

    gAnimationSequenceCurrentIndex = v1;

    gAnimationDescriptionCurrentIndex = 0;

    return 0;
}

// 0x413B80
static int _anim_free_slot(int requestOptions)
{
    int v1 = -1;
    int v2 = 0;
    for (int index = 0; index < ANIMATION_SEQUENCE_LIST_CAPACITY; index++) {
        AnimationSequence* animationSequence = &(gAnimationSequences[index]);
        if (animationSequence->field_0 != -1000 || (animationSequence->flags & ANIM_SEQ_ACCUMULATING) != 0 || (animationSequence->flags & ANIM_SEQ_0x20) != 0) {
            if (!(animationSequence->flags & ANIM_SEQ_RESERVED)) {
                v2++;
            }
        } else if (v1 == -1 && ((requestOptions & ANIMATION_REQUEST_PING) == 0 || (animationSequence->flags & ANIM_SEQ_0x10) == 0)) {
            v1 = index;
        }
    }

    if (v1 == -1) {
        if ((requestOptions & ANIMATION_REQUEST_RESERVED) != 0) {
            debugPrint("Unable to begin reserved animation!\n");
        }

        return -1;
    } else if ((requestOptions & ANIMATION_REQUEST_RESERVED) != 0 || v2 < 20) {
        return v1;
    }

    return -1;
}

// 0x413C20
int _register_priority(int a1)
{
    if (gAnimationSequenceCurrentIndex == -1) {
        return -1;
    }

    if (a1 == 0) {
        return -1;
    }

    gAnimationSequences[gAnimationSequenceCurrentIndex].flags |= ANIM_SEQ_PRIORITIZED;

    return 0;
}

// 0x413C4C
int reg_anim_clear(Object* a1)
{
    for (int animationSequenceIndex = 0; animationSequenceIndex < ANIMATION_SEQUENCE_LIST_CAPACITY; animationSequenceIndex++) {
        AnimationSequence* animationSequence = &(gAnimationSequences[animationSequenceIndex]);
        if (animationSequence->field_0 == -1000) {
            continue;
        }

        int animationDescriptionIndex;
        for (animationDescriptionIndex = 0; animationDescriptionIndex < animationSequence->length; animationDescriptionIndex++) {
            AnimationDescription* animationDescription = &(animationSequence->animations[animationDescriptionIndex]);
            if (a1 != animationDescription->owner || animationDescription->kind == 11) {
                continue;
            }

            break;
        }

        if (animationDescriptionIndex == animationSequence->length) {
            continue;
        }

        if ((animationSequence->flags & ANIM_SEQ_PRIORITIZED) != 0) {
            return -2;
        }

        _anim_set_end(animationSequenceIndex);

        return 0;
    }

    return -1;
}

// 0x413CCC
int reg_anim_end()
{
    if (gAnimationSequenceCurrentIndex == -1) {
        return -1;
    }

    AnimationSequence* animationSequence = &(gAnimationSequences[gAnimationSequenceCurrentIndex]);
    animationSequence->field_0 = 0;
    animationSequence->length = gAnimationDescriptionCurrentIndex;
    animationSequence->animationIndex = -1;
    animationSequence->flags &= ~ANIM_SEQ_ACCUMULATING;
    animationSequence->animations[0].delay = 0;
    if (isInCombat()) {
        _combat_anim_begin();
        animationSequence->flags |= ANIM_SEQ_COMBAT_ANIM_STARTED;
    }

    int index = gAnimationSequenceCurrentIndex;
    gAnimationSequenceCurrentIndex = -1;

    if (!(animationSequence->flags & ANIM_SEQ_0x10)) {
        _anim_set_continue(index, 1);
    }

    return 0;
}

// NOTE: Inlined.
//
// 0x413D6C
static int _anim_preload(Object* object, int fid, CacheEntry** cacheEntryPtr)
{
    *cacheEntryPtr = nullptr;

    if (artLock(fid, cacheEntryPtr) != nullptr) {
        artUnlock(*cacheEntryPtr);
        *cacheEntryPtr = nullptr;
        return 0;
    }

    return -1;
}

// 0x413D98
static void _anim_cleanup()
{
    if (gAnimationSequenceCurrentIndex == -1) {
        return;
    }

    for (int index = 0; index < ANIMATION_SEQUENCE_LIST_CAPACITY; index++) {
        gAnimationSequences[index].flags &= ~(ANIM_SEQ_ACCUMULATING | ANIM_SEQ_0x10);
    }

    AnimationSequence* animationSequence = &(gAnimationSequences[gAnimationSequenceCurrentIndex]);
    for (int index = 0; index < gAnimationDescriptionCurrentIndex; index++) {
        AnimationDescription* animationDescription = &(animationSequence->animations[index]);
        if (animationDescription->artCacheKey != nullptr) {
            artUnlock(animationDescription->artCacheKey);
        }

        if (animationDescription->kind == ANIM_KIND_CALLBACK && animationDescription->callback == (AnimationCallback*)_gsnd_anim_sound) {
            soundEffectDelete((Sound*)animationDescription->param1);
        }
    }

    gAnimationSequenceCurrentIndex = -1;
}

// 0x413E2C
static int _check_registry(Object* obj)
{
    if (gAnimationSequenceCurrentIndex == -1) {
        return -1;
    }

    if (gAnimationDescriptionCurrentIndex >= ANIMATION_DESCRIPTION_LIST_CAPACITY) {
        return -1;
    }

    if (obj == nullptr) {
        return 0;
    }

    for (int animationSequenceIndex = 0; animationSequenceIndex < ANIMATION_SEQUENCE_LIST_CAPACITY; animationSequenceIndex++) {
        AnimationSequence* animationSequence = &(gAnimationSequences[animationSequenceIndex]);

        if (animationSequenceIndex != gAnimationSequenceCurrentIndex && animationSequence->field_0 != -1000) {
            for (int animationDescriptionIndex = 0; animationDescriptionIndex < animationSequence->length; animationDescriptionIndex++) {
                AnimationDescription* animationDescription = &(animationSequence->animations[animationDescriptionIndex]);
                if (obj == animationDescription->owner && animationDescription->kind != 11) {
                    if ((animationSequence->flags & ANIM_SEQ_INSIGNIFICANT) == 0) {
                        return -1;
                    }

                    _anim_set_end(animationSequenceIndex);
                }
            }
        }
    }

    return 0;
}

// Returns -1 if object is playing some animation.
//
// 0x413EC8
int animationIsBusy(Object* a1)
{
    if (gAnimationDescriptionCurrentIndex >= ANIMATION_DESCRIPTION_LIST_CAPACITY || a1 == nullptr) {
        return 0;
    }

    for (int animationSequenceIndex = 0; animationSequenceIndex < ANIMATION_SEQUENCE_LIST_CAPACITY; animationSequenceIndex++) {
        AnimationSequence* animationSequence = &(gAnimationSequences[animationSequenceIndex]);
        if (animationSequenceIndex != gAnimationSequenceCurrentIndex && animationSequence->field_0 != -1000) {
            for (int animationDescriptionIndex = 0; animationDescriptionIndex < animationSequence->length; animationDescriptionIndex++) {
                AnimationDescription* animationDescription = &(animationSequence->animations[animationDescriptionIndex]);
                if (a1 != animationDescription->owner) {
                    continue;
                }

                if (animationDescription->kind == ANIM_KIND_CALLBACK) {
                    continue;
                }

                if (animationSequence->length == 1 && animationDescription->anim == ANIM_STAND) {
                    continue;
                }

                return -1;
            }
        }
    }

    return 0;
}

// 0x413F5C
int animationRegisterMoveToObject(Object* owner, Object* destination, int actionPoints, int delay)
{
    if (_check_registry(owner) == -1 || actionPoints == 0) {
        _anim_cleanup();
        return -1;
    }

    if (owner->tile == destination->tile && owner->elevation == destination->elevation) {
        return 0;
    }

    AnimationDescription* animationDescription = &(gAnimationSequences[gAnimationSequenceCurrentIndex].animations[gAnimationDescriptionCurrentIndex]);
    animationDescription->kind = ANIM_KIND_MOVE_TO_OBJECT;
    animationDescription->anim = ANIM_WALK;
    animationDescription->owner = owner;
    animationDescription->destination = destination;
    animationDescription->actionPoints = actionPoints;
    animationDescription->delay = delay;

    int fid = buildFid(FID_TYPE(owner->fid), owner->fid & 0xFFF, animationDescription->anim, (owner->fid & 0xF000) >> 12, owner->rotation + 1);

    // NOTE: Uninline.
    if (_anim_preload(owner, fid, &(animationDescription->artCacheKey)) == -1) {
        _anim_cleanup();
        return -1;
    }

    gAnimationDescriptionCurrentIndex++;

    return animationRegisterRotateToTile(owner, destination->tile);
}

// 0x41405C
int animationRegisterRunToObject(Object* owner, Object* destination, int actionPoints, int delay)
{
    if (_check_registry(owner) == -1 || actionPoints == 0) {
        _anim_cleanup();
        return -1;
    }

    if (owner->tile == destination->tile && owner->elevation == destination->elevation) {
        return 0;
    }

    if (critterIsEncumbered(owner)) {
        if (objectIsPartyMember(owner)) {
            reportOverloaded(owner);
        }

        return animationRegisterMoveToObject(owner, destination, actionPoints, delay);
    }

    AnimationDescription* animationDescription = &(gAnimationSequences[gAnimationSequenceCurrentIndex].animations[gAnimationDescriptionCurrentIndex]);
    animationDescription->kind = ANIM_KIND_MOVE_TO_OBJECT;
    animationDescription->owner = owner;
    animationDescription->destination = destination;

    if ((FID_TYPE(owner->fid) == OBJ_TYPE_CRITTER && (owner->data.critter.combat.results & DAM_CRIP_LEG_ANY) != 0)
        || (owner == gDude && dudeHasState(DUDE_STATE_SNEAKING) && !perkGetRank(gDude, PERK_SILENT_RUNNING))
        || !artExists(buildFid(FID_TYPE(owner->fid), owner->fid & 0xFFF, ANIM_RUNNING, 0, owner->rotation + 1))) {
        animationDescription->anim = ANIM_WALK;
    } else {
        animationDescription->anim = ANIM_RUNNING;
    }

    animationDescription->actionPoints = actionPoints;
    animationDescription->delay = delay;

    int fid = buildFid(FID_TYPE(owner->fid), owner->fid & 0xFFF, animationDescription->anim, (owner->fid & 0xF000) >> 12, owner->rotation + 1);

    // NOTE: Uninline.
    if (_anim_preload(owner, fid, &(animationDescription->artCacheKey)) == -1) {
        _anim_cleanup();
        return -1;
    }

    gAnimationDescriptionCurrentIndex++;
    return animationRegisterRotateToTile(owner, destination->tile);
}

// 0x414294
int animationRegisterMoveToTile(Object* owner, int tile, int elevation, int actionPoints, int delay)
{
    if (_check_registry(owner) == -1 || actionPoints == 0) {
        _anim_cleanup();
        return -1;
    }

    if (tile == owner->tile && elevation == owner->elevation) {
        return 0;
    }

    AnimationDescription* animationDescription = &(gAnimationSequences[gAnimationSequenceCurrentIndex].animations[gAnimationDescriptionCurrentIndex]);
    animationDescription->kind = ANIM_KIND_MOVE_TO_TILE;
    animationDescription->anim = ANIM_WALK;
    animationDescription->owner = owner;
    animationDescription->tile = tile;
    animationDescription->elevation = elevation;
    animationDescription->actionPoints = actionPoints;
    animationDescription->delay = delay;

    int fid = buildFid(FID_TYPE(owner->fid), owner->fid & 0xFFF, animationDescription->anim, (owner->fid & 0xF000) >> 12, owner->rotation + 1);

    // NOTE: Uninline.
    if (_anim_preload(owner, fid, &(animationDescription->artCacheKey)) == -1) {
        _anim_cleanup();
        return -1;
    }

    gAnimationDescriptionCurrentIndex++;

    return 0;
}

// 0x414394
int animationRegisterRunToTile(Object* owner, int tile, int elevation, int actionPoints, int delay)
{
    if (_check_registry(owner) == -1 || actionPoints == 0) {
        _anim_cleanup();
        return -1;
    }

    if (tile == owner->tile && elevation == owner->elevation) {
        return 0;
    }

    if (critterIsEncumbered(owner)) {
        if (objectIsPartyMember(owner)) {
            reportOverloaded(owner);
        }

        return animationRegisterMoveToTile(owner, tile, elevation, actionPoints, delay);
    }

    AnimationDescription* animationDescription = &(gAnimationSequences[gAnimationSequenceCurrentIndex].animations[gAnimationDescriptionCurrentIndex]);
    animationDescription->kind = ANIM_KIND_MOVE_TO_TILE;
    animationDescription->owner = owner;
    animationDescription->tile = tile;
    animationDescription->elevation = elevation;

    if ((FID_TYPE(owner->fid) == OBJ_TYPE_CRITTER && (owner->data.critter.combat.results & DAM_CRIP_LEG_ANY) != 0)
        || (owner == gDude && dudeHasState(DUDE_STATE_SNEAKING) && !perkGetRank(gDude, PERK_SILENT_RUNNING))
        || !artExists(buildFid(FID_TYPE(owner->fid), owner->fid & 0xFFF, ANIM_RUNNING, 0, owner->rotation + 1))) {
        animationDescription->anim = ANIM_WALK;
    } else {
        animationDescription->anim = ANIM_RUNNING;
    }

    animationDescription->actionPoints = actionPoints;
    animationDescription->delay = delay;

    int fid = buildFid(FID_TYPE(owner->fid), owner->fid & 0xFFF, animationDescription->anim, (owner->fid & 0xF000) >> 12, owner->rotation + 1);

    // NOTE: Uninline.
    if (_anim_preload(owner, fid, &(animationDescription->artCacheKey)) == -1) {
        _anim_cleanup();
        return -1;
    }

    gAnimationDescriptionCurrentIndex++;

    return 0;
}

// 0x4145D0
int animationRegisterMoveToTileStraight(Object* object, int tile, int elevation, int anim, int delay)
{
    if (_check_registry(object) == -1) {
        _anim_cleanup();
        return -1;
    }

    if (tile == object->tile && elevation == object->elevation) {
        return 0;
    }

    AnimationSequence* animationSequence = &(gAnimationSequences[gAnimationSequenceCurrentIndex]);

    AnimationDescription* animationDescription = &(animationSequence->animations[gAnimationDescriptionCurrentIndex]);
    animationDescription->kind = ANIM_KIND_MOVE_TO_TILE_STRAIGHT;
    animationDescription->owner = object;
    animationDescription->tile = tile;
    animationDescription->elevation = elevation;
    animationDescription->anim = anim;
    animationDescription->delay = delay;

    int fid = buildFid(FID_TYPE(object->fid), object->fid & 0xFFF, animationDescription->anim, (object->fid & 0xF000) >> 12, object->rotation + 1);

    // NOTE: Uninline.
    if (_anim_preload(object, fid, &(animationDescription->artCacheKey)) == -1) {
        _anim_cleanup();
        return -1;
    }

    gAnimationDescriptionCurrentIndex++;

    return 0;
}

// 0x4146C4
int animationRegisterMoveToTileStraightAndWaitForComplete(Object* owner, int tile, int elevation, int anim, int delay)
{
    if (_check_registry(owner) == -1) {
        _anim_cleanup();
        return -1;
    }

    if (tile == owner->tile && elevation == owner->elevation) {
        return 0;
    }

    AnimationSequence* animationSequence = &(gAnimationSequences[gAnimationSequenceCurrentIndex]);
    AnimationDescription* animationDescription = &(animationSequence->animations[gAnimationDescriptionCurrentIndex]);
    animationDescription->kind = ANIM_KIND_MOVE_TO_TILE_STRAIGHT_AND_WAIT_FOR_COMPLETE;
    animationDescription->owner = owner;
    animationDescription->tile = tile;
    animationDescription->elevation = elevation;
    animationDescription->anim = anim;
    animationDescription->delay = delay;

    int fid = buildFid(FID_TYPE(owner->fid), owner->fid & 0xFFF, animationDescription->anim, (owner->fid & 0xF000) >> 12, owner->rotation + 1);

    // NOTE: Uninline.
    if (_anim_preload(owner, fid, &(animationDescription->artCacheKey)) == -1) {
        _anim_cleanup();
        return -1;
    }

    gAnimationDescriptionCurrentIndex++;

    return 0;
}

// 0x4149D0
int animationRegisterAnimate(Object* owner, int anim, int delay)
{
    if (_check_registry(owner) == -1) {
        _anim_cleanup();
        return -1;
    }

    AnimationSequence* animationSequence = &(gAnimationSequences[gAnimationSequenceCurrentIndex]);
    AnimationDescription* animationDescription = &(animationSequence->animations[gAnimationDescriptionCurrentIndex]);
    animationDescription->kind = ANIM_KIND_ANIMATE;
    animationDescription->owner = owner;
    animationDescription->anim = anim;
    animationDescription->delay = delay;

    int fid = buildFid(FID_TYPE(owner->fid), owner->fid & 0xFFF, animationDescription->anim, (owner->fid & 0xF000) >> 12, owner->rotation + 1);

    // NOTE: Uninline.
    if (_anim_preload(owner, fid, &(animationDescription->artCacheKey)) == -1) {
        _anim_cleanup();
        return -1;
    }

    gAnimationDescriptionCurrentIndex++;

    return 0;
}

// 0x414AA8
int animationRegisterAnimateReversed(Object* owner, int anim, int delay)
{
    if (_check_registry(owner) == -1) {
        _anim_cleanup();
        return -1;
    }

    AnimationSequence* animationSequence = &(gAnimationSequences[gAnimationSequenceCurrentIndex]);
    AnimationDescription* animationDescription = &(animationSequence->animations[gAnimationDescriptionCurrentIndex]);
    animationDescription->kind = ANIM_KIND_ANIMATE_REVERSED;
    animationDescription->owner = owner;
    animationDescription->anim = anim;
    animationDescription->delay = delay;
    animationDescription->artCacheKey = nullptr;

    int fid = buildFid(FID_TYPE(owner->fid), owner->fid & 0xFFF, animationDescription->anim, (owner->fid & 0xF000) >> 12, owner->rotation + 1);

    // NOTE: Uninline.
    if (_anim_preload(owner, fid, &(animationDescription->artCacheKey)) == -1) {
        _anim_cleanup();
        return -1;
    }

    gAnimationDescriptionCurrentIndex++;

    return 0;
}

// 0x414B7C
int animationRegisterAnimateAndHide(Object* owner, int anim, int delay)
{
    if (_check_registry(owner) == -1) {
        _anim_cleanup();
        return -1;
    }

    AnimationSequence* animationSequence = &(gAnimationSequences[gAnimationSequenceCurrentIndex]);
    AnimationDescription* animationDescription = &(animationSequence->animations[gAnimationDescriptionCurrentIndex]);
    animationDescription->kind = ANIM_KIND_ANIMATE_AND_HIDE;
    animationDescription->owner = owner;
    animationDescription->anim = anim;
    animationDescription->delay = delay;
    animationDescription->artCacheKey = nullptr;

    int fid = buildFid(FID_TYPE(owner->fid), owner->fid & 0xFFF, anim, (owner->fid & 0xF000) >> 12, owner->rotation + 1);

    // NOTE: Uninline.
    if (_anim_preload(owner, fid, &(animationDescription->artCacheKey)) == -1) {
        _anim_cleanup();
        return -1;
    }

    gAnimationDescriptionCurrentIndex++;

    return 0;
}

// 0x414C50
int animationRegisterRotateToTile(Object* owner, int tile)
{
    if (_check_registry(owner) == -1) {
        _anim_cleanup();
        return -1;
    }

    AnimationSequence* animationSequence = &(gAnimationSequences[gAnimationSequenceCurrentIndex]);
    AnimationDescription* animationDescription = &(animationSequence->animations[gAnimationDescriptionCurrentIndex]);
    animationDescription->kind = ANIM_KIND_ROTATE_TO_TILE;
    animationDescription->delay = -1;
    animationDescription->artCacheKey = nullptr;
    animationDescription->owner = owner;
    animationDescription->tile = tile;

    gAnimationDescriptionCurrentIndex++;

    return 0;
}

// 0x414CC8
int animationRegisterRotateClockwise(Object* owner)
{
    if (_check_registry(owner) == -1) {
        _anim_cleanup();
        return -1;
    }

    AnimationSequence* animationSequence = &(gAnimationSequences[gAnimationSequenceCurrentIndex]);
    AnimationDescription* animationDescription = &(animationSequence->animations[gAnimationDescriptionCurrentIndex]);
    animationDescription->kind = ANIM_KIND_ROTATE_CLOCKWISE;
    animationDescription->delay = -1;
    animationDescription->artCacheKey = nullptr;
    animationDescription->owner = owner;

    gAnimationDescriptionCurrentIndex++;

    return 0;
}

// 0x414D38
int animationRegisterRotateCounterClockwise(Object* owner)
{
    if (_check_registry(owner) == -1) {
        _anim_cleanup();
        return -1;
    }

    AnimationSequence* animationSequence = &(gAnimationSequences[gAnimationSequenceCurrentIndex]);
    AnimationDescription* animationDescription = &(animationSequence->animations[gAnimationDescriptionCurrentIndex]);
    animationDescription->kind = ANIM_KIND_ROTATE_COUNTER_CLOCKWISE;
    animationDescription->delay = -1;
    animationDescription->artCacheKey = nullptr;
    animationDescription->owner = owner;

    gAnimationDescriptionCurrentIndex++;

    return 0;
}

// NOTE: Unused.
//
// 0x414DA8
int animationRegisterHideObject(Object* object)
{
    if (_check_registry(object) == -1) {
        _anim_cleanup();
        return -1;
    }

    AnimationSequence* animationSequence = &(gAnimationSequences[gAnimationSequenceCurrentIndex]);
    AnimationDescription* animationDescription = &(animationSequence->animations[gAnimationDescriptionCurrentIndex]);
    animationDescription->kind = ANIM_KIND_HIDE;
    animationDescription->delay = -1;
    animationDescription->artCacheKey = nullptr;
    animationDescription->extendedFlags = 0;
    animationDescription->owner = object;
    gAnimationDescriptionCurrentIndex++;

    return 0;
}

// 0x414E20
int animationRegisterHideObjectForced(Object* object)
{
    if (_check_registry(object) == -1) {
        _anim_cleanup();
        return -1;
    }

    AnimationSequence* animationSequence = &(gAnimationSequences[gAnimationSequenceCurrentIndex]);
    AnimationDescription* animationDescription = &(animationSequence->animations[gAnimationDescriptionCurrentIndex]);
    animationDescription->kind = ANIM_KIND_HIDE;
    animationDescription->delay = -1;
    animationDescription->artCacheKey = nullptr;
    animationDescription->extendedFlags = ANIMATION_SEQUENCE_FORCED;
    animationDescription->owner = object;
    gAnimationDescriptionCurrentIndex++;

    return 0;
}

// 0x414E98
int animationRegisterCallback(void* param1, void* param2, AnimationCallback* proc, int delay)
{
    if (_check_registry(nullptr) == -1 || proc == nullptr) {
        _anim_cleanup();
        return -1;
    }

    AnimationSequence* animationSequence = &(gAnimationSequences[gAnimationSequenceCurrentIndex]);
    AnimationDescription* animationDescription = &(animationSequence->animations[gAnimationDescriptionCurrentIndex]);
    animationDescription->kind = ANIM_KIND_CALLBACK;
    animationDescription->extendedFlags = 0;
    animationDescription->artCacheKey = nullptr;
    animationDescription->param2 = param2;
    animationDescription->param1 = param1;
    animationDescription->callback = proc;
    animationDescription->delay = delay;

    gAnimationDescriptionCurrentIndex++;

    return 0;
}

// Same as `animationRegisterCallback` but accepting 3 parameters.
//
// 0x414F20
int animationRegisterCallback3(void* param1, void* param2, void* param3, AnimationCallback3* proc, int delay)
{
    if (_check_registry(nullptr) == -1 || proc == nullptr) {
        _anim_cleanup();
        return -1;
    }

    AnimationSequence* animationSequence = &(gAnimationSequences[gAnimationSequenceCurrentIndex]);
    AnimationDescription* animationDescription = &(animationSequence->animations[gAnimationDescriptionCurrentIndex]);
    animationDescription->kind = ANIM_KIND_CALLBACK3;
    animationDescription->extendedFlags = 0;
    animationDescription->artCacheKey = nullptr;
    animationDescription->param2 = param2;
    animationDescription->param1 = param1;
    animationDescription->callback3 = proc;
    animationDescription->param3 = param3;
    animationDescription->delay = delay;

    gAnimationDescriptionCurrentIndex++;

    return 0;
}

// 0x414FAC
int animationRegisterCallbackForced(void* a1, void* a2, AnimationCallback* proc, int delay)
{
    if (_check_registry(nullptr) == -1 || proc == nullptr) {
        _anim_cleanup();
        return -1;
    }

    AnimationSequence* animationSequence = &(gAnimationSequences[gAnimationSequenceCurrentIndex]);
    AnimationDescription* animationDescription = &(animationSequence->animations[gAnimationDescriptionCurrentIndex]);
    animationDescription->kind = ANIM_KIND_CALLBACK;
    animationDescription->extendedFlags = ANIMATION_SEQUENCE_FORCED;
    animationDescription->artCacheKey = nullptr;
    animationDescription->param2 = a2;
    animationDescription->param1 = a1;
    animationDescription->callback = proc;
    animationDescription->delay = delay;

    gAnimationDescriptionCurrentIndex++;

    return 0;
}

// NOTE: Unused.
//
// The [flag] parameter should be one of OBJECT_* flags. The way it's handled
// down the road implies it should not be a group of flags (joined with bitwise
// OR), but a one particular flag.
//
// 0x415034
int animationRegisterSetFlag(Object* object, int flag, int delay)
{
    if (_check_registry(object) == -1) {
        _anim_cleanup();
        return -1;
    }

    AnimationSequence* animationSequence = &(gAnimationSequences[gAnimationSequenceCurrentIndex]);
    AnimationDescription* animationDescription = &(animationSequence->animations[gAnimationDescriptionCurrentIndex]);
    animationDescription->kind = ANIM_KIND_SET_FLAG;
    animationDescription->artCacheKey = nullptr;
    animationDescription->owner = object;
    animationDescription->objectFlag = flag;
    animationDescription->delay = delay;

    gAnimationDescriptionCurrentIndex++;

    return 0;
}

// The [flag] parameter should be one of OBJECT_* flags. The way it's handled
// down the road implies it should not be a group of flags (joined with bitwise
// OR), but a one particular flag.
//
// 0x4150A8
int animationRegisterUnsetFlag(Object* object, int flag, int delay)
{
    if (_check_registry(object) == -1) {
        _anim_cleanup();
        return -1;
    }

    AnimationSequence* animationSequence = &(gAnimationSequences[gAnimationSequenceCurrentIndex]);
    AnimationDescription* animationDescription = &(animationSequence->animations[gAnimationDescriptionCurrentIndex]);
    animationDescription->kind = ANIM_KIND_UNSET_FLAG;
    animationDescription->artCacheKey = nullptr;
    animationDescription->owner = object;
    animationDescription->objectFlag = flag;
    animationDescription->delay = delay;

    gAnimationDescriptionCurrentIndex++;

    return 0;
}

// 0x41518C
int animationRegisterSetFid(Object* owner, int fid, int delay)
{
    if (_check_registry(owner) == -1) {
        _anim_cleanup();
        return -1;
    }

    AnimationSequence* animationSequence = &(gAnimationSequences[gAnimationSequenceCurrentIndex]);
    AnimationDescription* animationDescription = &(animationSequence->animations[gAnimationDescriptionCurrentIndex]);
    animationDescription->kind = ANIM_KIND_SET_FID;
    animationDescription->owner = owner;
    animationDescription->fid = fid;
    animationDescription->delay = delay;

    // NOTE: Uninline.
    if (_anim_preload(owner, fid, &(animationDescription->artCacheKey)) == -1) {
        _anim_cleanup();
        return -1;
    }

    gAnimationDescriptionCurrentIndex++;

    return 0;
}

// 0x415238
int animationRegisterTakeOutWeapon(Object* owner, int weaponAnimationCode, int delay)
{
    const char* sfx = sfxBuildCharName(owner, ANIM_TAKE_OUT, weaponAnimationCode);
    if (animationRegisterPlaySoundEffect(owner, sfx, delay) == -1) {
        return -1;
    }

    if (_check_registry(owner) == -1) {
        _anim_cleanup();
        return -1;
    }

    AnimationSequence* animationSequence = &(gAnimationSequences[gAnimationSequenceCurrentIndex]);
    AnimationDescription* animationDescription = &(animationSequence->animations[gAnimationDescriptionCurrentIndex]);
    animationDescription->kind = ANIM_KIND_TAKE_OUT_WEAPON;
    animationDescription->anim = ANIM_TAKE_OUT;
    animationDescription->delay = 0;
    animationDescription->owner = owner;
    animationDescription->weaponAnimationCode = weaponAnimationCode;

    int fid = buildFid(FID_TYPE(owner->fid), owner->fid & 0xFFF, ANIM_TAKE_OUT, weaponAnimationCode, owner->rotation + 1);

    // NOTE: Uninline.
    if (_anim_preload(owner, fid, &(animationDescription->artCacheKey)) == -1) {
        _anim_cleanup();
        return -1;
    }

    gAnimationDescriptionCurrentIndex++;

    return 0;
}

// 0x415334
int animationRegisterSetLightDistance(Object* owner, int lightDistance, int delay)
{
    if (_check_registry(owner) == -1) {
        _anim_cleanup();
        return -1;
    }

    AnimationSequence* animationSequence = &(gAnimationSequences[gAnimationSequenceCurrentIndex]);
    AnimationDescription* animationDescription = &(animationSequence->animations[gAnimationDescriptionCurrentIndex]);
    animationDescription->kind = ANIM_KIND_SET_LIGHT_DISTANCE;
    animationDescription->artCacheKey = nullptr;
    animationDescription->owner = owner;
    animationDescription->lightDistance = lightDistance;
    animationDescription->delay = delay;

    gAnimationDescriptionCurrentIndex++;

    return 0;
}

// NOTE: Unused.
//
// 0x4153A8
int animationRegisterToggleOutline(Object* object, bool outline, int delay)
{
    if (_check_registry(object) == -1) {
        _anim_cleanup();
        return -1;
    }

    AnimationSequence* animationSequence = &(gAnimationSequences[gAnimationSequenceCurrentIndex]);
    AnimationDescription* animationDescription = &(animationSequence->animations[gAnimationDescriptionCurrentIndex]);
    animationDescription->kind = ANIM_KIND_TOGGLE_OUTLINE;
    animationDescription->artCacheKey = nullptr;
    animationDescription->owner = object;
    animationDescription->outline = outline;
    animationDescription->delay = delay;

    gAnimationDescriptionCurrentIndex++;

    return 0;
}

// 0x41541C
int animationRegisterPlaySoundEffect(Object* owner, const char* soundEffectName, int delay)
{
    if (_check_registry(owner) == -1) {
        _anim_cleanup();
        return -1;
    }

    AnimationSequence* animationSequence = &(gAnimationSequences[gAnimationSequenceCurrentIndex]);
    AnimationDescription* animationDescription = &(animationSequence->animations[gAnimationDescriptionCurrentIndex]);
    animationDescription->kind = ANIM_KIND_CALLBACK;
    animationDescription->owner = owner;
    if (soundEffectName != nullptr) {
        int volume = _gsound_compute_relative_volume(owner);
        animationDescription->param1 = soundEffectLoadWithVolume(soundEffectName, owner, volume);
        if (animationDescription->param1 != nullptr) {
            animationDescription->callback = (AnimationCallback*)_gsnd_anim_sound;
        } else {
            animationDescription->kind = ANIM_KIND_CONTINUE;
        }
    } else {
        animationDescription->kind = ANIM_KIND_CONTINUE;
    }

    animationDescription->artCacheKey = nullptr;
    animationDescription->delay = delay;

    gAnimationDescriptionCurrentIndex++;

    return 0;
}

// 0x4154C4
int animationRegisterAnimateForever(Object* owner, int anim, int delay)
{
    if (_check_registry(owner) == -1) {
        _anim_cleanup();
        return -1;
    }

    AnimationSequence* animationSequence = &(gAnimationSequences[gAnimationSequenceCurrentIndex]);
    AnimationDescription* animationDescription = &(animationSequence->animations[gAnimationDescriptionCurrentIndex]);
    animationDescription->kind = ANIM_KIND_ANIMATE_FOREVER;
    animationDescription->owner = owner;
    animationDescription->anim = anim;
    animationDescription->delay = delay;

    int fid = buildFid(FID_TYPE(owner->fid), owner->fid & 0xFFF, anim, (owner->fid & 0xF000) >> 12, owner->rotation + 1);

    // NOTE: Uninline.
    if (_anim_preload(owner, fid, &(animationDescription->artCacheKey)) == -1) {
        _anim_cleanup();
        return -1;
    }

    gAnimationDescriptionCurrentIndex++;

    return 0;
}

// 0x415598
int animationRegisterPing(int flags, int delay)
{
    if (_check_registry(nullptr) == -1) {
        _anim_cleanup();
        return -1;
    }

    int animationSequenceIndex = _anim_free_slot(flags | ANIMATION_REQUEST_PING);
    if (animationSequenceIndex == -1) {
        return -1;
    }

    gAnimationSequences[animationSequenceIndex].flags = ANIM_SEQ_0x10;

    AnimationSequence* animationSequence = &(gAnimationSequences[gAnimationSequenceCurrentIndex]);
    AnimationDescription* animationDescription = &(animationSequence->animations[gAnimationDescriptionCurrentIndex]);
    animationDescription->owner = nullptr;
    animationDescription->kind = ANIM_KIND_PING;
    animationDescription->artCacheKey = nullptr;
    animationDescription->animationSequenceIndex = animationSequenceIndex;
    animationDescription->delay = delay;

    gAnimationDescriptionCurrentIndex++;

    return 0;
}

// 0x4156A8
static int animationRunSequence(int animationSequenceIndex)
{
    if (animationSequenceIndex == -1) {
        return -1;
    }

    AnimationSequence* animationSequence = &(gAnimationSequences[animationSequenceIndex]);
    if (animationSequence->field_0 == -1000) {
        return -1;
    }

    while (1) {
        if (animationSequence->field_0 >= animationSequence->length) {
            return 0;
        }

        if (animationSequence->field_0 > animationSequence->animationIndex) {
            AnimationDescription* animationDescription = &(animationSequence->animations[animationSequence->field_0]);
            if (animationDescription->delay < 0) {
                return 0;
            }

            if (animationDescription->delay > 0) {
                animationDescription->delay--;
                return 0;
            }
        }

        AnimationDescription* animationDescription = &(animationSequence->animations[animationSequence->field_0++]);

        int rc;
        Rect rect;
        switch (animationDescription->kind) {
        case ANIM_KIND_MOVE_TO_OBJECT:
            rc = animateMoveObjectToObject(animationDescription->owner, animationDescription->destination, animationDescription->actionPoints, animationDescription->anim, animationSequenceIndex);
            break;
        case ANIM_KIND_MOVE_TO_TILE:
            rc = animateMoveObjectToTile(animationDescription->owner, animationDescription->tile, animationDescription->elevation, animationDescription->actionPoints, animationDescription->anim, animationSequenceIndex);
            break;
        case ANIM_KIND_MOVE_TO_TILE_STRAIGHT:
            rc = animateMoveObjectToTileStraight(animationDescription->owner, animationDescription->tile, animationDescription->elevation, animationDescription->anim, animationSequenceIndex, 0);
            break;
        case ANIM_KIND_MOVE_TO_TILE_STRAIGHT_AND_WAIT_FOR_COMPLETE:
            rc = animateMoveObjectToTileStraight(animationDescription->owner, animationDescription->tile, animationDescription->elevation, animationDescription->anim, animationSequenceIndex, ANIM_SAD_WAIT_FOR_COMPLETION);
            break;
        case ANIM_KIND_ANIMATE:
            rc = _anim_animate(animationDescription->owner, animationDescription->anim, animationSequenceIndex, 0);
            break;
        case ANIM_KIND_ANIMATE_REVERSED:
            rc = _anim_animate(animationDescription->owner, animationDescription->anim, animationSequenceIndex, ANIM_SAD_REVERSE);
            break;
        case ANIM_KIND_ANIMATE_AND_HIDE:
            rc = _anim_animate(animationDescription->owner, animationDescription->anim, animationSequenceIndex, ANIM_SAD_HIDE_ON_END);
            if (rc == -1) {
                // NOTE: Uninline.
                rc = _anim_hide(animationDescription->owner, animationSequenceIndex);
            }
            break;
        case ANIM_KIND_ANIMATE_FOREVER:
            rc = _anim_animate(animationDescription->owner, animationDescription->anim, animationSequenceIndex, ANIM_SAD_FOREVER);
            break;
        case ANIM_KIND_ROTATE_TO_TILE:
            if (!_critter_is_prone(animationDescription->owner)) {
                int rotation = tileGetRotationTo(animationDescription->owner->tile, animationDescription->tile);
                _dude_stand(animationDescription->owner, rotation, -1);
            }
            _anim_set_continue(animationSequenceIndex, 0);
            rc = 0;
            break;
        case ANIM_KIND_ROTATE_CLOCKWISE:
            rc = actionRotate(animationDescription->owner, 1, animationSequenceIndex);
            break;
        case ANIM_KIND_ROTATE_COUNTER_CLOCKWISE:
            rc = actionRotate(animationDescription->owner, -1, animationSequenceIndex);
            break;
        case ANIM_KIND_HIDE:
            // NOTE: Uninline.
            rc = _anim_hide(animationDescription->owner, animationSequenceIndex);
            break;
        case ANIM_KIND_CALLBACK:
            rc = animationDescription->callback(animationDescription->param1, animationDescription->param2);
            if (rc == 0) {
                rc = _anim_set_continue(animationSequenceIndex, 0);
            }
            break;
        case ANIM_KIND_CALLBACK3:
            rc = animationDescription->callback3(animationDescription->param1, animationDescription->param2, animationDescription->param3);
            if (rc == 0) {
                rc = _anim_set_continue(animationSequenceIndex, 0);
            }
            break;
        case ANIM_KIND_SET_FLAG:
            if (animationDescription->objectFlag == OBJECT_LIGHTING) {
                if (_obj_turn_on_light(animationDescription->owner, &rect) == 0) {
                    tileWindowRefreshRect(&rect, animationDescription->owner->elevation);
                }
            } else if (animationDescription->objectFlag == OBJECT_HIDDEN) {
                if (objectHide(animationDescription->owner, &rect) == 0) {
                    tileWindowRefreshRect(&rect, animationDescription->owner->elevation);
                }
            } else {
                animationDescription->owner->flags |= animationDescription->objectFlag;
            }

            rc = _anim_set_continue(animationSequenceIndex, 0);
            break;
        case ANIM_KIND_UNSET_FLAG:
            if (animationDescription->objectFlag == OBJECT_LIGHTING) {
                if (_obj_turn_off_light(animationDescription->owner, &rect) == 0) {
                    tileWindowRefreshRect(&rect, animationDescription->owner->elevation);
                }
            } else if (animationDescription->objectFlag == OBJECT_HIDDEN) {
                if (objectShow(animationDescription->owner, &rect) == 0) {
                    tileWindowRefreshRect(&rect, animationDescription->owner->elevation);
                }
            } else {
                animationDescription->owner->flags &= ~animationDescription->objectFlag;
            }

            rc = _anim_set_continue(animationSequenceIndex, 0);
            break;
        case ANIM_KIND_TOGGLE_FLAT:
            if (_obj_toggle_flat(animationDescription->owner, &rect) == 0) {
                tileWindowRefreshRect(&rect, animationDescription->owner->elevation);
            }
            rc = _anim_set_continue(animationSequenceIndex, 0);
            break;
        case ANIM_KIND_SET_FID:
            rc = _anim_change_fid(animationDescription->owner, animationSequenceIndex, animationDescription->fid);
            break;
        case ANIM_KIND_TAKE_OUT_WEAPON:
            rc = _anim_animate(animationDescription->owner, ANIM_TAKE_OUT, animationSequenceIndex, animationDescription->tile);
            break;
        case ANIM_KIND_SET_LIGHT_DISTANCE:
            objectSetLight(animationDescription->owner, animationDescription->lightDistance, animationDescription->owner->lightIntensity, &rect);
            tileWindowRefreshRect(&rect, animationDescription->owner->elevation);
            rc = _anim_set_continue(animationSequenceIndex, 0);
            break;
        case ANIM_KIND_SET_LIGHT_INTENSITY:
            objectSetLight(animationDescription->owner, animationDescription->lightDistance, animationDescription->lightIntensity, &rect);
            tileWindowRefreshRect(&rect, animationDescription->owner->elevation);
            rc = _anim_set_continue(animationSequenceIndex, 0);
            break;
        case ANIM_KIND_MOVE_ON_STAIRS:
            rc = _anim_move_on_stairs(animationDescription->owner, animationDescription->tile, animationDescription->elevation, animationDescription->anim, animationSequenceIndex);
            break;
        case ANIM_KIND_CHECK_FALLING:
            rc = _check_for_falling(animationDescription->owner, animationDescription->anim, animationSequenceIndex);
            break;
        case ANIM_KIND_TOGGLE_OUTLINE:
            if (animationDescription->outline) {
                if (objectEnableOutline(animationDescription->owner, &rect) == 0) {
                    tileWindowRefreshRect(&rect, animationDescription->owner->elevation);
                }
            } else {
                if (objectDisableOutline(animationDescription->owner, &rect) == 0) {
                    tileWindowRefreshRect(&rect, animationDescription->owner->elevation);
                }
            }
            rc = _anim_set_continue(animationSequenceIndex, 0);
            break;
        case ANIM_KIND_PING:
            gAnimationSequences[animationDescription->animationSequenceIndex].flags &= ~ANIM_SEQ_0x10;
            rc = _anim_set_continue(animationDescription->animationSequenceIndex, 1);
            if (rc != -1) {
                rc = _anim_set_continue(animationSequenceIndex, 0);
            }
            break;
        case ANIM_KIND_CONTINUE:
            rc = _anim_set_continue(animationSequenceIndex, 0);
            break;
        default:
            rc = -1;
            break;
        }

        if (rc == -1) {
            _anim_set_end(animationSequenceIndex);
        }

        if (animationSequence->field_0 == -1000) {
            return -1;
        }
    }
}

// 0x415B44
static int _anim_set_continue(int animationSequenceIndex, int a2)
{
    if (animationSequenceIndex == -1) {
        return -1;
    }

    AnimationSequence* animationSequence = &(gAnimationSequences[animationSequenceIndex]);
    if (animationSequence->field_0 == -1000) {
        return -1;
    }

    animationSequence->animationIndex++;
    if (animationSequence->animationIndex == animationSequence->length) {
        return _anim_set_end(animationSequenceIndex);
    } else {
        if (a2) {
            return animationRunSequence(animationSequenceIndex);
        }
    }

    return 0;
}

// 0x415B9C
static int _anim_set_end(int animationSequenceIndex)
{
    AnimationSequence* animationSequence;
    AnimationDescription* animationDescription;
    int i;

    if (animationSequenceIndex == -1) {
        return -1;
    }

    animationSequence = &(gAnimationSequences[animationSequenceIndex]);
    if (animationSequence->field_0 == -1000) {
        return -1;
    }

    for (i = 0; i < gAnimationCurrentSad; i++) {
        AnimationSad* sad = &(gAnimationSads[i]);
        if (sad->animationSequenceIndex == animationSequenceIndex) {
            sad->field_20 = -1000;
        }
    }

    for (i = 0; i < animationSequence->length; i++) {
        animationDescription = &(animationSequence->animations[i]);
        if (animationDescription->kind == ANIM_KIND_HIDE && ((i < animationSequence->animationIndex) || (animationDescription->extendedFlags & ANIMATION_SEQUENCE_FORCED))) {
            Rect rect;
            int elevation = animationDescription->owner->elevation;
            objectDestroy(animationDescription->owner, &rect);
            tileWindowRefreshRect(&rect, elevation);
        }
    }

    for (i = 0; i < animationSequence->length; i++) {
        animationDescription = &(animationSequence->animations[i]);
        if (animationDescription->artCacheKey) {
            artUnlock(animationDescription->artCacheKey);
        }

        if (animationDescription->kind != 11 && animationDescription->kind != 12) {
            // TODO: Check.
            if (animationDescription->kind != ANIM_KIND_PING) {
                Object* owner = animationDescription->owner;
                if (FID_TYPE(owner->fid) == OBJ_TYPE_CRITTER) {
                    int j = 0;
                    for (; j < i; j++) {
                        AnimationDescription* ad = &(animationSequence->animations[j]);
                        if (owner == ad->owner) {
                            if (ad->kind != ANIM_KIND_CALLBACK && ad->kind != ANIM_KIND_CALLBACK3) {
                                break;
                            }
                        }
                    }

                    if (i == j) {
                        int k = 0;
                        for (; k < animationSequence->animationIndex; k++) {
                            AnimationDescription* ad = &(animationSequence->animations[k]);
                            if (ad->kind == ANIM_KIND_HIDE && ad->owner == owner) {
                                break;
                            }
                        }

                        if (k == animationSequence->animationIndex) {
                            for (int m = 0; m < gAnimationCurrentSad; m++) {
                                if (gAnimationSads[m].obj == owner) {
                                    gAnimationSads[m].field_20 = -1000;
                                    break;
                                }
                            }

                            if ((animationSequence->flags & ANIM_SEQ_NO_STAND) == 0 && !_critter_is_prone(owner)) {
                                _dude_stand(owner, owner->rotation, -1);
                            }
                        }
                    }
                }
            }
        } else if (i >= animationSequence->field_0) {
            if (animationDescription->extendedFlags & ANIMATION_SEQUENCE_FORCED) {
                animationDescription->callback(animationDescription->param1, animationDescription->param2);
            } else {
                if (animationDescription->kind == ANIM_KIND_CALLBACK && animationDescription->callback == (AnimationCallback*)_gsnd_anim_sound) {
                    soundEffectDelete((Sound*)animationDescription->param1);
                }
            }
        }
    }

    animationSequence->animationIndex = -1;
    animationSequence->field_0 = -1000;
    if ((animationSequence->flags & ANIM_SEQ_COMBAT_ANIM_STARTED) != 0) {
        _combat_anim_finished();
    }

    if (_anim_in_bk) {
        animationSequence->flags = ANIM_SEQ_0x20;
    } else {
        animationSequence->flags = 0;
    }

    return 0;
}

// 0x415E24
static bool canUseDoor(Object* critter, Object* door)
{
    if (critter == gDude) {
        if (!_obj_portal_is_walk_thru(door)) {
            return false;
        }
    }

    if (FID_TYPE(critter->fid) != OBJ_TYPE_CRITTER) {
        return false;
    }

    if (FID_TYPE(door->fid) != OBJ_TYPE_SCENERY) {
        return false;
    }

    int bodyType = critterGetBodyType(critter);
    if (bodyType != BODY_TYPE_BIPED && bodyType != BODY_TYPE_ROBOTIC) {
        return false;
    }

    Proto* proto;
    if (protoGetProto(door->pid, &proto) == -1) {
        return false;
    }

    if (proto->scenery.type != SCENERY_TYPE_DOOR) {
        return false;
    }

    if (objectIsLocked(door)) {
        return false;
    }

    if (critterGetKillType(critter) == KILL_TYPE_GECKO) {
        return false;
    }

    return true;
}

// 0x415EE8
int _make_path(Object* object, int from, int to, unsigned char* rotations, int a5)
{
    return pathfinderFindPath(object, from, to, rotations, a5, _obj_blocking_at);
}

// TODO: move pathfinding into another unit
// 0x415EFC
int pathfinderFindPath(Object* object, int from, int to, unsigned char* rotations, int a5, PathBuilderCallback* callback)
{
    if (a5) {
        if (callback(object, to, object->elevation) != nullptr) {
            return 0;
        }
    }

    bool isCritter = false;
    int critterType = 0;
    if (PID_TYPE(object->pid) == OBJ_TYPE_CRITTER) {
        isCritter = true;
        critterType = critterGetKillType(object);
    }

    bool isNotInCombat = !isInCombat();

    memset(gPathfinderProcessedTiles, 0, sizeof(gPathfinderProcessedTiles));

    gPathfinderProcessedTiles[from / 8] |= 1 << (from & 7);

    gOpenPathNodeList[0].tile = from;
    gOpenPathNodeList[0].from = -1;
    gOpenPathNodeList[0].rotation = 0;
    gOpenPathNodeList[0].estimate = _tile_idistance(from, to);
    gOpenPathNodeList[0].cost = 0;

    for (int index = 1; index < 2000; index += 1) {
        gOpenPathNodeList[index].tile = -1;
    }

    int toScreenX;
    int toScreenY;
    tileToScreenXY(to, &toScreenX, &toScreenY, object->elevation);

    int closedPathNodeListLength = 0;
    int openPathNodeListLength = 1;
    PathNode temp;

    while (1) {
        int v63 = -1;

        PathNode* prev = nullptr;
        int v12 = 0;
        for (int index = 0; v12 < openPathNodeListLength; index += 1) {
            PathNode* curr = &(gOpenPathNodeList[index]);
            if (curr->tile != -1) {
                v12++;
                if (v63 == -1 || (curr->estimate + curr->cost) < (prev->estimate + prev->cost)) {
                    prev = curr;
                    v63 = index;
                }
            }
        }

        PathNode* curr = &(gOpenPathNodeList[v63]);

        memcpy(&temp, curr, sizeof(temp));

        openPathNodeListLength -= 1;

        curr->tile = -1;

        if (temp.tile == to) {
            if (openPathNodeListLength == 0) {
                openPathNodeListLength = 1;
            }
            break;
        }

        PathNode* curr1 = &(gClosedPathNodeList[closedPathNodeListLength]);
        memcpy(curr1, &temp, sizeof(temp));

        closedPathNodeListLength += 1;

        if (closedPathNodeListLength == 2000) {
            return 0;
        }

        for (int rotation = 0; rotation < ROTATION_COUNT; rotation++) {
            int tile = tileGetTileInDirection(temp.tile, rotation, 1);
            int bit = 1 << (tile & 7);
            if ((gPathfinderProcessedTiles[tile / 8] & bit) != 0) {
                continue;
            }

            if (tile != to) {
                Object* v24 = callback(object, tile, object->elevation);
                if (v24 != nullptr) {
                    if (!canUseDoor(object, v24)) {
                        continue;
                    }
                }
            }

            int v25 = 0;
            for (; v25 < 2000; v25++) {
                if (gOpenPathNodeList[v25].tile == -1) {
                    break;
                }
            }

            openPathNodeListLength += 1;

            if (openPathNodeListLength == 2000) {
                return 0;
            }

            gPathfinderProcessedTiles[tile / 8] |= bit;

            PathNode* v27 = &(gOpenPathNodeList[v25]);
            v27->tile = tile;
            v27->from = temp.tile;
            v27->rotation = rotation;

            int newX;
            int newY;
            tileToScreenXY(tile, &newX, &newY, object->elevation);

            v27->estimate = _idist(newX, newY, toScreenX, toScreenY);
            v27->cost = temp.cost + 50;

            if (isNotInCombat && temp.rotation != rotation) {
                v27->cost += 10;
            }

            if (isCritter) {
                Object* o = objectFindFirstAtLocation(object->elevation, v27->tile);
                while (o != nullptr) {
                    if (o->pid >= FIRST_RADIOACTIVE_GOO_PID && o->pid <= LAST_RADIOACTIVE_GOO_PID) {
                        break;
                    }
                    o = objectFindNextAtLocation();
                }

                if (o != nullptr) {
                    if (critterType == KILL_TYPE_GECKO) {
                        v27->cost += 100;
                    } else {
                        v27->cost += 400;
                    }
                }
            }
        }

        if (openPathNodeListLength == 0) {
            break;
        }
    }

    if (openPathNodeListLength != 0) {
        unsigned char* v39 = rotations;
        int index = 0;
        for (; index < 800; index++) {
            if (temp.tile == from) {
                break;
            }

            if (v39 != nullptr) {
                *v39 = temp.rotation & 0xFF;
                v39 += 1;
            }

            int j = 0;
            while (gClosedPathNodeList[j].tile != temp.from) {
                j++;
            }

            PathNode* v36 = &(gClosedPathNodeList[j]);
            memcpy(&temp, v36, sizeof(temp));
        }

        if (rotations != nullptr) {
            // Looks like array resevering, probably because A* finishes it's path from end to start,
            // this probably reverses it start-to-end.
            unsigned char* beginning = rotations;
            unsigned char* ending = rotations + index - 1;
            int middle = index / 2;
            for (int index = 0; index < middle; index++) {
                unsigned char rotation = *ending;
                *ending = *beginning;
                *beginning = rotation;

                ending -= 1;
                beginning += 1;
            }
        }

        return index;
    }

    return 0;
}

// 0x41633C
static int _idist(int x1, int y1, int x2, int y2)
{
    int dx = x2 - x1;
    if (dx < 0) {
        dx = -dx;
    }

    int dy = y2 - y1;
    if (dy < 0) {
        dy = -dy;
    }

    int dm = (dx <= dy) ? dx : dy;

    return dx + dy - (dm / 2);
}

// 0x416360
static int _tile_idistance(int tile1, int tile2)
{
    int x1;
    int y1;
    tileToScreenXY(tile1, &x1, &y1, gElevation);

    int x2;
    int y2;
    tileToScreenXY(tile2, &x2, &y2, gElevation);

    return _idist(x1, y1, x2, y2);
}

// 0x4163AC
int _make_straight_path(Object* obj, int from, int to, StraightPathNode* straightPathNodeList, Object** obstaclePtr, int a6)
{
    return _make_straight_path_func(obj, from, to, straightPathNodeList, obstaclePtr, a6, _obj_blocking_at);
}

// TODO: Rather complex, but understandable, needs testing.
//
// 0x4163C8
int _make_straight_path_func(Object* obj, int from, int to, StraightPathNode* straightPathNodeList, Object** obstaclePtr, int a6, PathBuilderCallback* callback)
{
    if (obstaclePtr != nullptr) {
        Object* obstacle = callback(obj, from, obj->elevation);
        if (obstacle != nullptr) {
            if (obstacle != *obstaclePtr && (a6 != 32 || (obstacle->flags & OBJECT_SHOOT_THRU) == 0)) {
                *obstaclePtr = obstacle;
                return 0;
            }
        }
    }

    int fromX;
    int fromY;
    tileToScreenXY(from, &fromX, &fromY, obj->elevation);
    fromX += 16;
    fromY += 8;

    int toX;
    int toY;
    tileToScreenXY(to, &toX, &toY, obj->elevation);
    toX += 16;
    toY += 8;

    int stepX;
    int deltaX = toX - fromX;
    if (deltaX > 0) {
        stepX = 1;
    } else if (deltaX < 0) {
        stepX = -1;
    } else {
        stepX = 0;
    }

    int stepY;
    int deltaY = toY - fromY;
    if (deltaY > 0) {
        stepY = 1;
    } else if (deltaY < 0) {
        stepY = -1;
    } else {
        stepY = 0;
    }

    int ddx = 2 * abs(toX - fromX);
    int ddy = 2 * abs(toY - fromY);

    int tileX = fromX;
    int tileY = fromY;

    int pathNodeIndex = 0;
    int prevTile = from;
    int v22 = 0;
    int tile;

    if (ddx <= ddy) {
        int middle = ddx - ddy / 2;
        while (true) {
            tile = tileFromScreenXY(tileX, tileY, obj->elevation);

            v22 += 1;
            if (v22 == a6) {
                if (pathNodeIndex >= 200) {
                    return 0;
                }

                if (straightPathNodeList != nullptr) {
                    StraightPathNode* pathNode = &(straightPathNodeList[pathNodeIndex]);
                    pathNode->tile = tile;
                    pathNode->elevation = obj->elevation;

                    tileToScreenXY(tile, &fromX, &fromY, obj->elevation);
                    pathNode->x = tileX - fromX - 16;
                    pathNode->y = tileY - fromY - 8;
                }

                v22 = 0;
                pathNodeIndex++;
            }

            if (tileY == toY) {
                if (obstaclePtr != nullptr) {
                    *obstaclePtr = nullptr;
                }
                break;
            }

            if (middle >= 0) {
                tileX += stepX;
                middle -= ddy;
            }

            tileY += stepY;
            middle += ddx;

            if (tile != prevTile) {
                if (obstaclePtr != nullptr) {
                    Object* obstacle = callback(obj, tile, obj->elevation);
                    if (obstacle != nullptr) {
                        if (obstacle != *obstaclePtr && (a6 != 32 || (obstacle->flags & OBJECT_SHOOT_THRU) == 0)) {
                            *obstaclePtr = obstacle;
                            break;
                        }
                    }
                }
                prevTile = tile;
            }
        }
    } else {
        int middle = ddy - ddx / 2;
        while (true) {
            tile = tileFromScreenXY(tileX, tileY, obj->elevation);

            v22 += 1;
            if (v22 == a6) {
                if (pathNodeIndex >= 200) {
                    return 0;
                }

                if (straightPathNodeList != nullptr) {
                    StraightPathNode* pathNode = &(straightPathNodeList[pathNodeIndex]);
                    pathNode->tile = tile;
                    pathNode->elevation = obj->elevation;

                    tileToScreenXY(tile, &fromX, &fromY, obj->elevation);
                    pathNode->x = tileX - fromX - 16;
                    pathNode->y = tileY - fromY - 8;
                }

                v22 = 0;
                pathNodeIndex++;
            }

            if (tileX == toX) {
                if (obstaclePtr != nullptr) {
                    *obstaclePtr = nullptr;
                }
                break;
            }

            if (middle >= 0) {
                tileY += stepY;
                middle -= ddx;
            }

            tileX += stepX;
            middle += ddy;

            if (tile != prevTile) {
                if (obstaclePtr != nullptr) {
                    Object* obstacle = callback(obj, tile, obj->elevation);
                    if (obstacle != nullptr) {
                        if (obstacle != *obstaclePtr && (a6 != 32 || (obstacle->flags & OBJECT_SHOOT_THRU) == 0)) {
                            *obstaclePtr = obstacle;
                            break;
                        }
                    }
                }
                prevTile = tile;
            }
        }
    }

    if (v22 != 0) {
        if (pathNodeIndex >= 200) {
            return 0;
        }

        if (straightPathNodeList != nullptr) {
            StraightPathNode* pathNode = &(straightPathNodeList[pathNodeIndex]);
            pathNode->tile = tile;
            pathNode->elevation = obj->elevation;

            tileToScreenXY(tile, &fromX, &fromY, obj->elevation);
            pathNode->x = tileX - fromX - 16;
            pathNode->y = tileY - fromY - 8;
        }

        pathNodeIndex += 1;
    } else {
        if (pathNodeIndex > 0 && straightPathNodeList != nullptr) {
            straightPathNodeList[pathNodeIndex - 1].elevation = obj->elevation;
        }
    }

    return pathNodeIndex;
}

// 0x4167F8
static int animateMoveObjectToObject(Object* from, Object* to, int actionPoints, int anim, int animationSequenceIndex)
{
    bool hidden = (to->flags & OBJECT_HIDDEN);
    to->flags |= OBJECT_HIDDEN;

    int moveSadIndex = _anim_move(from, to->tile, to->elevation, -1, anim, 0, animationSequenceIndex);

    if (!hidden) {
        to->flags &= ~OBJECT_HIDDEN;
    }

    if (moveSadIndex == -1) {
        return -1;
    }

    AnimationSad* sad = &(gAnimationSads[moveSadIndex]);
    // NOTE: Original code is somewhat different. Due to some kind of
    // optimization this value is either 1 or 2, which is later used in
    // subsequent calculations and rotations array lookup.
    bool isMultihex = (from->flags & OBJECT_MULTIHEX);
    sad->field_1C -= (isMultihex ? 2 : 1);
    if (sad->field_1C <= 0) {
        sad->field_20 = -1000;
        _anim_set_continue(animationSequenceIndex, 0);
    }

    sad->field_24 = tileGetTileInDirection(to->tile, sad->rotations[isMultihex ? sad->field_1C + 1 : sad->field_1C], 1);

    if (isMultihex) {
        sad->field_24 = tileGetTileInDirection(sad->field_24, sad->rotations[sad->field_1C], 1);
    }

    if (actionPoints != -1 && actionPoints < sad->field_1C) {
        sad->field_1C = actionPoints;
    }

    return 0;
}

// 0x41695C
int _make_stair_path(Object* object, int from, int fromElevation, int to, int toElevation, StraightPathNode* a6, Object** obstaclePtr)
{
    int elevation = fromElevation;
    if (elevation > toElevation) {
        elevation = toElevation;
    }

    int fromX;
    int fromY;
    tileToScreenXY(from, &fromX, &fromY, fromElevation);
    fromX += 16;
    fromY += 8;

    int toX;
    int toY;
    tileToScreenXY(to, &toX, &toY, toElevation);
    toX += 16;
    toY += 8;

    if (obstaclePtr != nullptr) {
        *obstaclePtr = nullptr;
    }

    int ddx = 2 * abs(toX - fromX);

    int stepX;
    int deltaX = toX - fromX;
    if (deltaX > 0) {
        stepX = 1;
    } else if (deltaX < 0) {
        stepX = -1;
    } else {
        stepX = 0;
    }

    int ddy = 2 * abs(toY - fromY);

    int stepY;
    int deltaY = toY - fromY;
    if (deltaY > 0) {
        stepY = 1;
    } else if (deltaY < 0) {
        stepY = -1;
    } else {
        stepY = 0;
    }

    int tileX = fromX;
    int tileY = fromY;

    int pathNodeIndex = 0;
    int prevTile = from;
    int iteration = 0;
    int tile;

    if (ddx > ddy) {
        int middle = ddy - ddx / 2;
        while (true) {
            tile = tileFromScreenXY(tileX, tileY, elevation);

            iteration += 1;
            if (iteration == 16) {
                if (pathNodeIndex >= 200) {
                    return 0;
                }

                if (a6 != nullptr) {
                    StraightPathNode* pathNode = &(a6[pathNodeIndex]);
                    pathNode->tile = tile;
                    pathNode->elevation = elevation;

                    tileToScreenXY(tile, &fromX, &fromY, elevation);
                    pathNode->x = tileX - fromX - 16;
                    pathNode->y = tileY - fromY - 8;
                }

                iteration = 0;
                pathNodeIndex++;
            }

            if (tileX == toX) {
                break;
            }

            if (middle >= 0) {
                tileY += stepY;
                middle -= ddx;
            }

            tileX += stepX;
            middle += ddy;

            if (tile != prevTile) {
                if (obstaclePtr != nullptr) {
                    *obstaclePtr = _obj_blocking_at(object, tile, object->elevation);
                    if (*obstaclePtr != nullptr) {
                        break;
                    }
                }
                prevTile = tile;
            }
        }
    } else {
        int middle = ddx - ddy / 2;
        while (true) {
            tile = tileFromScreenXY(tileX, tileY, elevation);

            iteration += 1;
            if (iteration == 16) {
                if (pathNodeIndex >= 200) {
                    return 0;
                }

                if (a6 != nullptr) {
                    StraightPathNode* pathNode = &(a6[pathNodeIndex]);
                    pathNode->tile = tile;
                    pathNode->elevation = elevation;

                    tileToScreenXY(tile, &fromX, &fromY, elevation);
                    pathNode->x = tileX - fromX - 16;
                    pathNode->y = tileY - fromY - 8;
                }

                iteration = 0;
                pathNodeIndex++;
            }

            if (tileY == toY) {
                break;
            }

            if (middle >= 0) {
                tileX += stepX;
                middle -= ddy;
            }

            tileY += stepY;
            middle += ddx;

            if (tile != prevTile) {
                if (obstaclePtr != nullptr) {
                    *obstaclePtr = _obj_blocking_at(object, tile, object->elevation);
                    if (*obstaclePtr != nullptr) {
                        break;
                    }
                }
                prevTile = tile;
            }
        }
    }

    if (iteration != 0) {
        if (pathNodeIndex >= 200) {
            return 0;
        }

        if (a6 != nullptr) {
            StraightPathNode* pathNode = &(a6[pathNodeIndex]);
            pathNode->tile = tile;
            pathNode->elevation = elevation;

            tileToScreenXY(tile, &fromX, &fromY, elevation);
            pathNode->x = tileX - fromX - 16;
            pathNode->y = tileY - fromY - 8;
        }

        pathNodeIndex++;
    } else {
        if (pathNodeIndex > 0) {
            if (a6 != nullptr) {
                a6[pathNodeIndex - 1].elevation = toElevation;
            }
        }
    }

    return pathNodeIndex;
}

// 0x416CFC
static int animateMoveObjectToTile(Object* obj, int tile, int elev, int actionPoints, int anim, int animationSequenceIndex)
{
    int index = _anim_move(obj, tile, elev, -1, anim, 0, animationSequenceIndex);
    if (index == -1) {
        return -1;
    }

    if (_obj_blocking_at(obj, tile, elev)) {
        AnimationSad* sad = &(gAnimationSads[index]);
        sad->field_1C--;
        if (sad->field_1C <= 0) {
            sad->field_20 = -1000;
            _anim_set_continue(animationSequenceIndex, 0);
        }

        sad->field_24 = tileGetTileInDirection(tile, sad->rotations[sad->field_1C], 1);
        if (actionPoints != -1 && actionPoints < sad->field_1C) {
            sad->field_1C = actionPoints;
        }
    }

    return 0;
}

// 0x416DFC
static int _anim_move(Object* obj, int tile, int elev, int a3, int anim, int a5, int animationSequenceIndex)
{
    if (gAnimationCurrentSad == ANIMATION_SAD_LIST_CAPACITY) {
        return -1;
    }

    AnimationSad* sad = &(gAnimationSads[gAnimationCurrentSad]);
    sad->obj = obj;

    if (a5) {
        sad->flags = ANIM_SAD_0x20;
    } else {
        sad->flags = 0;
    }

    sad->field_20 = -2000;
    sad->fid = buildFid(FID_TYPE(obj->fid), obj->fid & 0xFFF, anim, (obj->fid & 0xF000) >> 12, obj->rotation + 1);
    sad->animationTimestamp = 0;
    sad->ticksPerFrame = animationComputeTicksPerFrame(obj, sad->fid);
    sad->field_24 = tile;
    sad->animationSequenceIndex = animationSequenceIndex;
    sad->anim = anim;

    sad->field_1C = _make_path(obj, obj->tile, tile, sad->rotations, a5);
    if (sad->field_1C == 0) {
        sad->field_20 = -1000;
        return -1;
    }

    if (a3 != -1 && sad->field_1C > a3) {
        sad->field_1C = a3;
    }

    return gAnimationCurrentSad++;
}

// 0x416F54
static int animateMoveObjectToTileStraight(Object* obj, int tile, int elevation, int anim, int animationSequenceIndex, int flags)
{
    if (gAnimationCurrentSad == ANIMATION_SAD_LIST_CAPACITY) {
        return -1;
    }

    AnimationSad* sad = &(gAnimationSads[gAnimationCurrentSad]);
    sad->obj = obj;
    sad->flags = flags | ANIM_SAD_STRAIGHT;
    if (anim == -1) {
        sad->fid = obj->fid;
        sad->flags |= ANIM_SAD_NO_ANIM;
    } else {
        sad->fid = buildFid(FID_TYPE(obj->fid), obj->fid & 0xFFF, anim, (obj->fid & 0xF000) >> 12, obj->rotation + 1);
    }
    sad->field_20 = -2000;
    sad->animationTimestamp = 0;
    sad->ticksPerFrame = animationComputeTicksPerFrame(obj, sad->fid);
    sad->animationSequenceIndex = animationSequenceIndex;

    int v15;
    if (FID_TYPE(obj->fid) == OBJ_TYPE_CRITTER) {
        if (FID_ANIM_TYPE(obj->fid) == ANIM_JUMP_BEGIN)
            v15 = 16;
        else
            v15 = 4;
    } else {
        v15 = 32;
    }

    sad->field_1C = _make_straight_path(obj, obj->tile, tile, sad->straightPathNodeList, nullptr, v15);
    if (sad->field_1C == 0) {
        sad->field_20 = -1000;
        return -1;
    }

    gAnimationCurrentSad++;

    return 0;
}

// 0x41712C
static int _anim_move_on_stairs(Object* obj, int tile, int elevation, int anim, int animationSequenceIndex)
{
    if (gAnimationCurrentSad == ANIMATION_SAD_LIST_CAPACITY) {
        return -1;
    }

    AnimationSad* sad = &(gAnimationSads[gAnimationCurrentSad]);
    sad->flags = ANIM_SAD_STRAIGHT;
    sad->obj = obj;
    if (anim == -1) {
        sad->fid = obj->fid;
        sad->flags |= ANIM_SAD_NO_ANIM;
    } else {
        sad->fid = buildFid(FID_TYPE(obj->fid), obj->fid & 0xFFF, anim, (obj->fid & 0xF000) >> 12, obj->rotation + 1);
    }
    sad->field_20 = -2000;
    sad->animationTimestamp = 0;
    sad->ticksPerFrame = animationComputeTicksPerFrame(obj, sad->fid);
    sad->animationSequenceIndex = animationSequenceIndex;
    sad->field_1C = _make_stair_path(obj, obj->tile, obj->elevation, tile, elevation, sad->straightPathNodeList, nullptr);
    if (sad->field_1C == 0) {
        sad->field_20 = -1000;
        return -1;
    }

    gAnimationCurrentSad++;

    return 0;
}

// 0x417248
static int _check_for_falling(Object* obj, int anim, int a3)
{
    if (gAnimationCurrentSad == ANIMATION_SAD_LIST_CAPACITY) {
        return -1;
    }

    if (_check_gravity(obj->tile, obj->elevation) == obj->elevation) {
        return -1;
    }

    AnimationSad* sad = &(gAnimationSads[gAnimationCurrentSad]);
    sad->flags = ANIM_SAD_STRAIGHT;
    sad->obj = obj;
    if (anim == -1) {
        sad->fid = obj->fid;
        sad->flags |= ANIM_SAD_NO_ANIM;
    } else {
        sad->fid = buildFid(FID_TYPE(obj->fid), obj->fid & 0xFFF, anim, (obj->fid & 0xF000) >> 12, obj->rotation + 1);
    }
    sad->field_20 = -2000;
    sad->animationTimestamp = 0;
    sad->ticksPerFrame = animationComputeTicksPerFrame(obj, sad->fid);
    sad->animationSequenceIndex = a3;
    sad->field_1C = _make_straight_path_func(obj, obj->tile, obj->tile, sad->straightPathNodeList, nullptr, 16, _obj_blocking_at);
    if (sad->field_1C == 0) {
        sad->field_20 = -1000;
        return -1;
    }

    gAnimationCurrentSad++;

    return 0;
}

// 0x417360
static void _object_move(int index)
{
    AnimationSad* sad = &(gAnimationSads[index]);
    Object* object = sad->obj;

    Rect dirtyRect;
    Rect tempRect;

    if (sad->field_20 == -2000) {
        objectSetLocation(object, object->tile, object->elevation, &dirtyRect);

        objectSetFrame(object, 0, &tempRect);
        rectUnion(&dirtyRect, &tempRect, &dirtyRect);

        objectSetRotation(object, sad->rotations[0], &tempRect);
        rectUnion(&dirtyRect, &tempRect, &dirtyRect);

        int fid = buildFid(FID_TYPE(object->fid), object->fid & 0xFFF, sad->anim, (object->fid & 0xF000) >> 12, object->rotation + 1);
        objectSetFid(object, fid, &tempRect);
        rectUnion(&dirtyRect, &tempRect, &dirtyRect);

        sad->field_20 = 0;
    } else {
        objectSetNextFrame(object, &dirtyRect);
    }

    int frameX;
    int frameY;

    CacheEntry* cacheHandle;
    Art* art = artLock(object->fid, &cacheHandle);
    if (art != nullptr) {
        artGetFrameOffsets(art, object->frame, object->rotation, &frameX, &frameY);
        artUnlock(cacheHandle);
    } else {
        frameX = 0;
        frameY = 0;
    }

    _obj_offset(object, frameX, frameY, &tempRect);
    rectUnion(&dirtyRect, &tempRect, &dirtyRect);

    int rotation = sad->rotations[sad->field_20];
    int y = dword_51D984[rotation];
    int x = _off_tile[rotation];
    if ((x > 0 && x <= object->x) || (x < 0 && x >= object->x) || (y > 0 && y <= object->y) || (y < 0 && y >= object->y)) {
        x = object->x - x;
        y = object->y - y;

        int nextTile = tileGetTileInDirection(object->tile, rotation, 1);
        Object* obstacle = _obj_blocking_at(object, nextTile, object->elevation);
        if (obstacle != nullptr) {
            if (!canUseDoor(object, obstacle)) {
                sad->field_1C = _make_path(object, object->tile, sad->field_24, sad->rotations, 1);
                if (sad->field_1C != 0) {
                    objectSetLocation(object, object->tile, object->elevation, &tempRect);
                    rectUnion(&dirtyRect, &tempRect, &dirtyRect);

                    objectSetFrame(object, 0, &tempRect);
                    rectUnion(&dirtyRect, &tempRect, &dirtyRect);

                    objectSetRotation(object, sad->rotations[0], &tempRect);
                    rectUnion(&dirtyRect, &tempRect, &dirtyRect);

                    sad->field_20 = 0;
                } else {
                    sad->field_20 = -1000;
                }
                nextTile = -1;
            } else {
                _obj_use_door(object, obstacle, 0);
            }
        }

        if (nextTile != -1) {
            objectSetLocation(object, nextTile, object->elevation, &tempRect);
            rectUnion(&dirtyRect, &tempRect, &dirtyRect);

            bool cannotMove = false;
            if (isInCombat() && FID_TYPE(object->fid) == OBJ_TYPE_CRITTER) {
                int actionPointsRequired = critterGetMovementPointCostAdjustedForCrippledLegs(object, 1);
                if (actionPointsRequired > _combat_free_move) {
                    actionPointsRequired -= _combat_free_move;
                    _combat_free_move = 0;
                    if (actionPointsRequired > object->data.critter.combat.ap) {
                        object->data.critter.combat.ap = 0;
                    } else {
                        object->data.critter.combat.ap -= actionPointsRequired;
                    }
                } else {
                    _combat_free_move -= actionPointsRequired;
                }

                if (object == gDude) {
                    interfaceRenderActionPoints(gDude->data.critter.combat.ap, _combat_free_move);
                }

                cannotMove = (object->data.critter.combat.ap + _combat_free_move) <= 0;
            }

            sad->field_20 += 1;

            if (sad->field_20 == sad->field_1C || cannotMove) {
                sad->field_20 = -1000;
            } else {
                objectSetRotation(object, sad->rotations[sad->field_20], &tempRect);
                rectUnion(&dirtyRect, &tempRect, &dirtyRect);

                _obj_offset(object, x, y, &tempRect);
                rectUnion(&dirtyRect, &tempRect, &dirtyRect);
            }
        }
    }

    tileWindowRefreshRect(&dirtyRect, object->elevation);
    if (sad->field_20 == -1000) {
        _anim_set_continue(sad->animationSequenceIndex, 1);
    }
}

// 0x4177C0
static void _object_straight_move(int index)
{
    AnimationSad* sad = &(gAnimationSads[index]);
    Object* object = sad->obj;

    Rect dirtyRect;
    Rect tempRect;

    if (sad->field_20 == -2000) {
        objectSetFid(object, sad->fid, &dirtyRect);
        sad->field_20 = 0;
    } else {
        objectGetRect(object, &dirtyRect);
    }

    CacheEntry* cacheHandle;
    Art* art = artLock(object->fid, &cacheHandle);
    if (art != nullptr) {
        int lastFrame = artGetFrameCount(art) - 1;
        artUnlock(cacheHandle);

        if ((sad->flags & ANIM_SAD_NO_ANIM) == 0) {
            if ((sad->flags & ANIM_SAD_WAIT_FOR_COMPLETION) == 0 || object->frame < lastFrame) {
                objectSetNextFrame(object, &tempRect);
                rectUnion(&dirtyRect, &tempRect, &dirtyRect);
            }
        }

        if (sad->field_20 < sad->field_1C) {
            StraightPathNode* straightPathNode = &(sad->straightPathNodeList[sad->field_20]);

            objectSetLocation(object, straightPathNode->tile, straightPathNode->elevation, &tempRect);
            rectUnion(&dirtyRect, &tempRect, &dirtyRect);

            _obj_offset(object, straightPathNode->x, straightPathNode->y, &tempRect);
            rectUnion(&dirtyRect, &tempRect, &dirtyRect);

            sad->field_20++;
        }

        if (sad->field_20 == sad->field_1C) {
            if ((sad->flags & ANIM_SAD_WAIT_FOR_COMPLETION) == 0 || object->frame == lastFrame) {
                sad->field_20 = -1000;
            }
        }

        tileWindowRefreshRect(&dirtyRect, sad->obj->elevation);

        if (sad->field_20 == -1000) {
            _anim_set_continue(sad->animationSequenceIndex, 1);
        }
    }
}

// 0x4179B8
static int _anim_animate(Object* obj, int anim, int animationSequenceIndex, int flags)
{
    if (gAnimationCurrentSad == ANIMATION_SAD_LIST_CAPACITY) {
        return -1;
    }

    AnimationSad* sad = &(gAnimationSads[gAnimationCurrentSad]);

    int fid;
    if (anim == ANIM_TAKE_OUT) {
        sad->flags = 0;
        fid = buildFid(FID_TYPE(obj->fid), obj->fid & 0xFFF, ANIM_TAKE_OUT, flags, obj->rotation + 1);
    } else {
        sad->flags = flags;
        fid = buildFid(FID_TYPE(obj->fid), obj->fid & 0xFFF, anim, (obj->fid & 0xF000) >> 12, obj->rotation + 1);
    }

    if (!artExists(fid)) {
        return -1;
    }

    sad->obj = obj;
    sad->fid = fid;
    sad->animationSequenceIndex = animationSequenceIndex;
    sad->animationTimestamp = 0;
    sad->ticksPerFrame = animationComputeTicksPerFrame(obj, sad->fid);
    sad->field_20 = 0;
    sad->field_1C = 0;

    gAnimationCurrentSad++;

    return 0;
}

// 0x417B30
void _object_animate()
{
    if (gAnimationCurrentSad == 0) {
        return;
    }

    _anim_in_bk = true;

    for (int index = 0; index < gAnimationCurrentSad; index++) {
        AnimationSad* sad = &(gAnimationSads[index]);
        if (sad->field_20 == -1000) {
            continue;
        }

        Object* object = sad->obj;

        unsigned int time = getTicks();
        if (getTicksBetween(time, sad->animationTimestamp) < sad->ticksPerFrame) {
            continue;
        }

        sad->animationTimestamp = time;

        if (animationRunSequence(sad->animationSequenceIndex) == -1) {
            continue;
        }

        if (sad->field_1C > 0) {
            if ((sad->flags & ANIM_SAD_STRAIGHT) != 0) {
                _object_straight_move(index);
            } else {
                int savedTile = object->tile;
                _object_move(index);
                if (savedTile != object->tile) {
                    scriptsExecSpatialProc(object, object->tile, object->elevation);
                }
            }
            continue;
        }

        if (sad->field_20 == 0) {
            for (int index = 0; index < gAnimationCurrentSad; index++) {
                AnimationSad* otherSad = &(gAnimationSads[index]);
                if (object == otherSad->obj && otherSad->field_20 == -2000) {
                    otherSad->field_20 = -1000;
                    _anim_set_continue(otherSad->animationSequenceIndex, 1);
                }
            }
            sad->field_20 = -2000;
        }

        Rect dirtyRect;
        Rect tempRect;

        objectGetRect(object, &dirtyRect);

        if (object->fid == sad->fid) {
            if ((sad->flags & ANIM_SAD_REVERSE) == 0) {
                CacheEntry* cacheHandle;
                Art* art = artLock(object->fid, &cacheHandle);
                if (art != nullptr) {
                    if ((sad->flags & ANIM_SAD_FOREVER) == 0 && object->frame == artGetFrameCount(art) - 1) {
                        sad->field_20 = -1000;
                        artUnlock(cacheHandle);

                        if ((sad->flags & ANIM_SAD_HIDE_ON_END) != 0) {
                            // NOTE: Uninline.
                            _anim_hide(object, -1);
                        }

                        _anim_set_continue(sad->animationSequenceIndex, 1);
                        continue;
                    } else {
                        objectSetNextFrame(object, &tempRect);
                        rectUnion(&dirtyRect, &tempRect, &dirtyRect);

                        int frameX;
                        int frameY;
                        artGetFrameOffsets(art, object->frame, object->rotation, &frameX, &frameY);

                        _obj_offset(object, frameX, frameY, &tempRect);
                        rectUnion(&dirtyRect, &tempRect, &dirtyRect);

                        artUnlock(cacheHandle);
                    }
                }

                tileWindowRefreshRect(&dirtyRect, gElevation);

                continue;
            }

            if ((sad->flags & ANIM_SAD_FOREVER) != 0 || object->frame != 0) {
                int x;
                int y;

                CacheEntry* cacheHandle;
                Art* art = artLock(object->fid, &cacheHandle);
                if (art != nullptr) {
                    artGetFrameOffsets(art, object->frame, object->rotation, &x, &y);
                    artUnlock(cacheHandle);
                }

                objectSetPrevFrame(object, &tempRect);
                rectUnion(&dirtyRect, &tempRect, &dirtyRect);

                _obj_offset(object, -x, -y, &tempRect);
                rectUnion(&dirtyRect, &tempRect, &dirtyRect);

                tileWindowRefreshRect(&dirtyRect, gElevation);
                continue;
            }

            sad->field_20 = -1000;
            _anim_set_continue(sad->animationSequenceIndex, 1);
        } else {
            int x;
            int y;

            CacheEntry* cacheHandle;
            Art* art = artLock(object->fid, &cacheHandle);
            if (art != nullptr) {
                artGetRotationOffsets(art, object->rotation, &x, &y);
                artUnlock(cacheHandle);
            } else {
                x = 0;
                y = 0;
            }

            objectSetFid(object, sad->fid, &tempRect);
            rectUnion(&dirtyRect, &tempRect, &dirtyRect);

            art = artLock(object->fid, &cacheHandle);
            if (art != nullptr) {
                int frame;
                if ((sad->flags & ANIM_SAD_REVERSE) != 0) {
                    frame = artGetFrameCount(art) - 1;
                } else {
                    frame = 0;
                }

                objectSetFrame(object, frame, &tempRect);
                rectUnion(&dirtyRect, &tempRect, &dirtyRect);

                int frameX;
                int frameY;
                artGetFrameOffsets(art, object->frame, object->rotation, &frameX, &frameY);

                Rect tempRect;
                _obj_offset(object, x + frameX, y + frameY, &tempRect);
                rectUnion(&dirtyRect, &tempRect, &dirtyRect);

                artUnlock(cacheHandle);
            } else {
                objectSetFrame(object, 0, &tempRect);
                rectUnion(&dirtyRect, &tempRect, &dirtyRect);
            }

            tileWindowRefreshRect(&dirtyRect, gElevation);
        }
    }

    _anim_in_bk = 0;

    _object_anim_compact();
}

// 0x417F18
static void _object_anim_compact()
{
    for (int index = 0; index < ANIMATION_SEQUENCE_LIST_CAPACITY; index++) {
        AnimationSequence* animationSequence = &(gAnimationSequences[index]);
        if ((animationSequence->flags & ANIM_SEQ_0x20) != 0) {
            animationSequence->flags = 0;
        }
    }

    int index = 0;
    for (; index < gAnimationCurrentSad; index++) {
        if (gAnimationSads[index].field_20 == -1000) {
            int nextIndex = index + 1;
            for (; nextIndex < gAnimationCurrentSad; nextIndex++) {
                if (gAnimationSads[nextIndex].field_20 != -1000) {
                    break;
                }
            }

            if (nextIndex == gAnimationCurrentSad) {
                break;
            }

            if (index != nextIndex) {
                memcpy(&(gAnimationSads[index]), &(gAnimationSads[nextIndex]), sizeof(AnimationSad));
                gAnimationSads[nextIndex].field_20 = -1000;
                gAnimationSads[nextIndex].flags = 0;
            }
        }
    }
    gAnimationCurrentSad = index;
}

// 0x417FFC
int _check_move(int* actionPointsPtr)
{
    int x;
    int y;
    mouseGetPosition(&x, &y);

    int tile = tileFromScreenXY(x, y, gElevation);
    if (tile == -1) {
        return -1;
    }

    if (isInCombat()) {
        if (*actionPointsPtr != -1) {
            if (gPressedPhysicalKeys[SDL_SCANCODE_RCTRL] || gPressedPhysicalKeys[SDL_SCANCODE_LCTRL]) {
                int hitMode;
                bool aiming;
                interfaceGetCurrentHitMode(&hitMode, &aiming);

                *actionPointsPtr -= itemGetActionPointCost(gDude, hitMode, aiming);
                if (*actionPointsPtr <= 0) {
                    return -1;
                }
            }
        }
    } else {
        if (settings.system.interrupt_walk) {
            reg_anim_clear(gDude);
        }
    }

    return tile;
}

// 0x4180B4
int _dude_move(int actionPoints)
{
    // 0x51072C
    static int lastDestination = -2;

    int tile = _check_move(&actionPoints);
    if (tile == -1) {
        return -1;
    }

    if (lastDestination == tile) {
        return _dude_run(actionPoints);
    }

    lastDestination = tile;

    reg_anim_begin(ANIMATION_REQUEST_RESERVED);

    animationRegisterMoveToTile(gDude, tile, gDude->elevation, actionPoints, 0);

    return reg_anim_end();
}

// 0x41810C
int _dude_run(int actionPoints)
{
    int tile = _check_move(&actionPoints);
    if (tile == -1) {
        return -1;
    }

    if (!perkGetRank(gDude, PERK_SILENT_RUNNING)) {
        dudeDisableState(DUDE_STATE_SNEAKING);
    }

    reg_anim_begin(ANIMATION_REQUEST_RESERVED);

    animationRegisterRunToTile(gDude, tile, gDude->elevation, actionPoints, 0);

    return reg_anim_end();
}

// 0x418168
void _dude_fidget()
{
    // 0x510730
    static unsigned int lastTime = 0;

    // 0x510734
    static unsigned int nextTime = 0;

    // 0x56C7E0
    static Object* candidates[100];

    if (_game_user_wants_to_quit != 0) {
        return;
    }

    if (isInCombat()) {
        return;
    }

    if ((gDude->flags & OBJECT_HIDDEN) != 0) {
        return;
    }

    unsigned int currentTime = _get_bk_time();
    if (getTicksBetween(currentTime, lastTime) <= nextTime) {
        return;
    }

    lastTime = currentTime;

    int candidatesLength = 0;
    Object* object = objectFindFirstAtElevation(gDude->elevation);
    while (object != nullptr) {
        if (candidatesLength >= 100) {
            break;
        }

        if ((object->flags & OBJECT_HIDDEN) == 0 && FID_TYPE(object->fid) == OBJ_TYPE_CRITTER && FID_ANIM_TYPE(object->fid) == ANIM_STAND && !critterIsDead(object)) {
            Rect rect;
            objectGetRect(object, &rect);

            Rect intersection;
            if (rectIntersection(&rect, &_scr_size, &intersection) == 0 && (gMapHeader.index != MAP_SPECIAL_RND_WOODSMAN || object->pid != 0x10000FA)) {
                candidates[candidatesLength++] = object;
            }
        }

        object = objectFindNextAtElevation();
    }

    int delayInSeconds;
    if (candidatesLength != 0) {
        int index = randomBetween(0, candidatesLength - 1);
        Object* object = candidates[index];

        reg_anim_begin(ANIMATION_REQUEST_UNRESERVED | ANIMATION_REQUEST_INSIGNIFICANT);

        bool shoudPlaySound = false;
        if (object == gDude) {
            shoudPlaySound = true;
        } else {
            char fileName[16];
            fileName[0] = '\0';
            artCopyFileName(1, object->fid & 0xFFF, fileName);
            if (fileName[0] == 'm' || fileName[0] == 'M') {
                if (objectGetDistanceBetween(object, gDude) < critterGetStat(gDude, STAT_PERCEPTION) * 2) {
                    shoudPlaySound = true;
                }
            }
        }

        if (shoudPlaySound) {
            const char* sfx = sfxBuildCharName(object, ANIM_STAND, CHARACTER_SOUND_EFFECT_UNUSED);
            animationRegisterPlaySoundEffect(object, sfx, 0);
        }

        animationRegisterAnimate(object, ANIM_STAND, 0);
        reg_anim_end();

        delayInSeconds = 20 / candidatesLength;
    } else {
        delayInSeconds = 7;
    }

    if (delayInSeconds < 1) {
        delayInSeconds = 1;
    } else if (delayInSeconds > 7) {
        delayInSeconds = 7;
    }

    nextTime = randomBetween(0, 3000) + 1000 * delayInSeconds;
}

// 0x418378
void _dude_stand(Object* obj, int rotation, int fid)
{
    Rect rect;

    objectSetRotation(obj, rotation, &rect);

    int x = 0;
    int y = 0;

    int weaponAnimationCode = (obj->fid & 0xF000) >> 12;
    if (weaponAnimationCode != 0) {
        if (fid == -1) {
            int takeOutFid = buildFid(FID_TYPE(obj->fid), obj->fid & 0xFFF, ANIM_TAKE_OUT, weaponAnimationCode, obj->rotation + 1);
            CacheEntry* takeOutFrmHandle;
            Art* takeOutFrm = artLock(takeOutFid, &takeOutFrmHandle);
            if (takeOutFrm != nullptr) {
                int frameCount = artGetFrameCount(takeOutFrm);
                for (int frame = 0; frame < frameCount; frame++) {
                    int offsetX;
                    int offsetY;
                    artGetFrameOffsets(takeOutFrm, frame, obj->rotation, &offsetX, &offsetY);
                    x += offsetX;
                    y += offsetY;
                }
                artUnlock(takeOutFrmHandle);

                CacheEntry* standFrmHandle;
                int standFid = buildFid(FID_TYPE(obj->fid), obj->fid & 0xFFF, ANIM_STAND, 0, obj->rotation + 1);
                Art* standFrm = artLock(standFid, &standFrmHandle);
                if (standFrm != nullptr) {
                    int offsetX;
                    int offsetY;
                    if (artGetRotationOffsets(standFrm, obj->rotation, &offsetX, &offsetY) == 0) {
                        x += offsetX;
                        y += offsetY;
                    }
                    artUnlock(standFrmHandle);
                }
            }
        }
    }

    if (fid == -1) {
        int anim;
        if (FID_ANIM_TYPE(obj->fid) == ANIM_FIRE_DANCE) {
            anim = ANIM_FIRE_DANCE;
        } else {
            anim = ANIM_STAND;
        }
        fid = buildFid(FID_TYPE(obj->fid), (obj->fid & 0xFFF), anim, (obj->fid & 0xF000) >> 12, obj->rotation + 1);
    }

    Rect temp;
    objectSetFid(obj, fid, &temp);
    rectUnion(&rect, &temp, &rect);

    objectSetLocation(obj, obj->tile, obj->elevation, &temp);
    rectUnion(&rect, &temp, &rect);

    objectSetFrame(obj, 0, &temp);
    rectUnion(&rect, &temp, &rect);

    _obj_offset(obj, x, y, &temp);
    rectUnion(&rect, &temp, &rect);

    tileWindowRefreshRect(&rect, obj->elevation);
}

// 0x418574
void _dude_standup(Object* a1)
{
    reg_anim_begin(ANIMATION_REQUEST_RESERVED);

    int anim;
    if (FID_ANIM_TYPE(a1->fid) == ANIM_FALL_BACK) {
        anim = ANIM_BACK_TO_STANDING;
    } else {
        anim = ANIM_PRONE_TO_STANDING;
    }

    animationRegisterAnimate(a1, anim, 0);
    reg_anim_end();
    a1->data.critter.combat.results &= ~DAM_KNOCKED_DOWN;
}

// 0x4185EC
static int actionRotate(Object* obj, int delta, int animationSequenceIndex)
{
    if (!_critter_is_prone(obj)) {
        int rotation = obj->rotation + delta;
        if (rotation >= ROTATION_COUNT) {
            rotation = ROTATION_NE;
        } else if (rotation < 0) {
            rotation = ROTATION_NW;
        }

        _dude_stand(obj, rotation, -1);
    }

    _anim_set_continue(animationSequenceIndex, 0);

    return 0;
}

// NOTE: Inlined.
//
// 0x41862C
static int _anim_hide(Object* object, int animationSequenceIndex)
{
    Rect rect;

    if (objectHide(object, &rect) == 0) {
        tileWindowRefreshRect(&rect, object->elevation);
    }

    if (animationSequenceIndex != -1) {
        _anim_set_continue(animationSequenceIndex, 0);
    }

    return 0;
}

// 0x418660
static int _anim_change_fid(Object* obj, int animationSequenceIndex, int fid)
{
    if (FID_ANIM_TYPE(fid)) {
        Rect dirtyRect;
        Rect tempRect;

        objectSetFid(obj, fid, &dirtyRect);
        objectSetFrame(obj, 0, &tempRect);
        rectUnion(&dirtyRect, &tempRect, &dirtyRect);
        tileWindowRefreshRect(&dirtyRect, obj->elevation);
    } else {
        _dude_stand(obj, obj->rotation, fid);
    }

    _anim_set_continue(animationSequenceIndex, 0);

    return 0;
}

// 0x4186CC
void animationStop()
{
    gAnimationInStop = true;
    gAnimationSequenceCurrentIndex = -1;

    for (int index = 0; index < ANIMATION_SEQUENCE_LIST_CAPACITY; index++) {
        _anim_set_end(index);
    }

    gAnimationInStop = false;
    gAnimationCurrentSad = 0;
}

// 0x418708
static int _check_gravity(int tile, int elevation)
{
    for (; elevation > 0; elevation--) {
        int x;
        int y;
        tileToScreenXY(tile, &x, &y, elevation);

        int squareTile = squareTileFromScreenXY(x + 2, y + 8, elevation);
        int fid = buildFid(OBJ_TYPE_TILE, _square[elevation]->field_0[squareTile] & 0xFFF, 0, 0, 0);
        if (fid != buildFid(OBJ_TYPE_TILE, 1, 0, 0, 0)) {
            break;
        }
    }
    return elevation;
}

// 0x418794
static unsigned int animationComputeTicksPerFrame(Object* object, int fid)
{
    int fps;

    CacheEntry* handle;
    Art* frm = artLock(fid, &handle);
    if (frm != nullptr) {
        fps = artGetFramesPerSecond(frm);
        artUnlock(handle);
    } else {
        fps = 10;
    }

    if (isInCombat()) {
        if (FID_ANIM_TYPE(fid) == ANIM_WALK) {
            if (object != gDude || settings.preferences.player_speedup) {
                fps += settings.preferences.combat_speed;
            }
        }
    }

    return 1000 / fps;
}

int animationRegisterSetLightIntensity(Object* owner, int lightDistance, int lightIntensity, int delay)
{
    if (_check_registry(owner) == -1) {
        _anim_cleanup();
        return -1;
    }

    AnimationSequence* animationSequence = &(gAnimationSequences[gAnimationSequenceCurrentIndex]);
    AnimationDescription* animationDescription = &(animationSequence->animations[gAnimationDescriptionCurrentIndex]);
    animationDescription->kind = ANIM_KIND_SET_LIGHT_INTENSITY;
    animationDescription->artCacheKey = nullptr;
    animationDescription->owner = owner;
    animationDescription->lightDistance = lightDistance;
    animationDescription->lightIntensity = lightIntensity;
    animationDescription->delay = delay;

    gAnimationDescriptionCurrentIndex++;

    return 0;
}

static void reportOverloaded(Object* critter)
{
    MessageListItem messageListItem;
    char formattedText[100];

    if (critter == gDude) {
        // You are overloaded.
        snprintf(formattedText, sizeof(formattedText),
            "%s",
            getmsg(&gMiscMessageList, &messageListItem, 8000));
    } else {
        // %s is overloaded.
        snprintf(formattedText, sizeof(formattedText),
            getmsg(&gMiscMessageList, &messageListItem, 8001),
            critterGetName(critter));
    }

    displayMonitorAddMessage(formattedText);
}

} // namespace fallout
