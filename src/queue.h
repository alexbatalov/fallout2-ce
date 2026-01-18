#ifndef QUEUE_H
#define QUEUE_H

#include "db.h"
#include "obj_types.h"

namespace fallout {

typedef enum EventType {
    EVENT_TYPE_DRUG = 0,
    EVENT_TYPE_KNOCKOUT = 1,
    EVENT_TYPE_WITHDRAWAL = 2,
    EVENT_TYPE_SCRIPT = 3,
    EVENT_TYPE_GAME_TIME = 4,
    EVENT_TYPE_POISON = 5,
    EVENT_TYPE_RADIATION = 6,
    EVENT_TYPE_FLARE = 7,
    EVENT_TYPE_EXPLOSION = 8,
    EVENT_TYPE_ITEM_TRICKLE = 9,
    EVENT_TYPE_SNEAK = 10,
    EVENT_TYPE_EXPLOSION_FAILURE = 11,
    EVENT_TYPE_MAP_UPDATE_EVENT = 12,
    EVENT_TYPE_GSOUND_SFX_EVENT = 13,
    EVENT_TYPE_COUNT,
} EventType;

typedef struct DrugEffectEvent {
    int drugPid;
    int stats[3];
    int modifiers[3];
} DrugEffectEvent;

typedef struct WithdrawalEvent {
    int active; // 0 == end withdrawal, 1 == start withdrawal
    int pid;
    int perk;
} WithdrawalEvent;

typedef struct ScriptEvent {
    int sid;
    int fixedParam;
} ScriptEvent;

typedef struct RadiationEvent {
    int radiationLevel;
    int isHealing;
} RadiationEvent;

typedef struct AmbientSoundEffectEvent {
    int ambientSoundEffectIndex;
} AmbientSoundEffectEvent;

typedef int QueueEventHandler(Object* owner, void* data);
typedef void QueueEventDataFreeProc(void* data);
typedef int QueueEventDataReadProc(File* stream, void** dataPtr);
typedef int QueueEventDataWriteProc(File* stream, void* data);

void queueInit();
int queueExit();
int queueLoad(File* stream);
int queueSave(File* stream);
int queueAddEvent(int delay, Object* owner, void* data, int eventType);
int queueRemoveEvents(Object* owner);
int queueRemoveEventsByType(Object* owner, int eventType);
bool queueHasEvent(Object* owner, int eventType);
int queueProcessEvents();
void queueClear();
void _queue_clear_type(int eventType, QueueEventHandler* fn);
unsigned int queueGetNextEventTime();
void _queue_leaving_map();
bool queueIsEmpty();
void* queueFindFirstEvent(Object* owner, int eventType);
void* queueFindNextEvent(Object* owner, int eventType);

} // namespace fallout

#endif /* QUEUE_H */
