#include "queue.h"

#include "actions.h"
#include "critter.h"
#include "display_monitor.h"
#include "game.h"
#include "game_sound.h"
#include "item.h"
#include "map.h"
#include "memory.h"
#include "message.h"
#include "object.h"
#include "perk.h"
#include "proto.h"
#include "proto_instance.h"
#include "scripts.h"

typedef struct QueueListNode {
    // TODO: Make unsigned.
    int time;
    int type;
    Object* owner;
    void* data;
    struct QueueListNode* next;
} QueueListNode;

typedef struct EventTypeDescription {
    QueueEventHandler* handlerProc;
    QueueEventDataFreeProc* freeProc;
    QueueEventDataReadProc* readProc;
    QueueEventDataWriteProc* writeProc;
    bool field_10;
    QueueEventHandler* field_14;
} EventTypeDescription;

static int flareEventProcess(Object* obj, void* data);
static int explosionEventProcess(Object* obj, void* data);
static int _queue_explode_exit(Object* obj, void* data);
static int _queue_do_explosion_(Object* obj, bool a2);
static int explosionFailureEventProcess(Object* obj, void* data);

// Last queue list node found during [queueFindFirstEvent] and
// [queueFindNextEvent] calls.
//
// 0x51C690
static QueueListNode* gLastFoundQueueListNode = NULL;

// 0x6648C0
static QueueListNode* gQueueListHead;

// 0x51C540
static EventTypeDescription gEventTypeDescriptions[EVENT_TYPE_COUNT] = {
    { drugEffectEventProcess, internal_free, drugEffectEventRead, drugEffectEventWrite, true, _item_d_clear },
    { knockoutEventProcess, NULL, NULL, NULL, true, _critter_wake_clear },
    { withdrawalEventProcess, internal_free, withdrawalEventRead, withdrawalEventWrite, true, _item_wd_clear },
    { scriptEventProcess, internal_free, scriptEventRead, scriptEventWrite, true, NULL },
    { gameTimeEventProcess, NULL, NULL, NULL, true, NULL },
    { poisonEventProcess, NULL, NULL, NULL, false, NULL },
    { radiationEventProcess, internal_free, radiationEventRead, radiationEventWrite, false, NULL },
    { flareEventProcess, NULL, NULL, NULL, true, flareEventProcess },
    { explosionEventProcess, NULL, NULL, NULL, true, _queue_explode_exit },
    { miscItemTrickleEventProcess, NULL, NULL, NULL, true, _item_m_turn_off_from_queue },
    { sneakEventProcess, NULL, NULL, NULL, true, _critter_sneak_clear },
    { explosionFailureEventProcess, NULL, NULL, NULL, true, _queue_explode_exit },
    { mapUpdateEventProcess, NULL, NULL, NULL, true, NULL },
    { ambientSoundEffectEventProcess, internal_free, NULL, NULL, true, NULL },
};

// 0x4A2320
void queueInit()
{
    gQueueListHead = NULL;
}

// 0x4A2330
int queueExit()
{
    queueClear();
    return 0;
}

// 0x4A2338
int queueLoad(File* stream)
{
    int count;
    if (fileReadInt32(stream, &count) == -1) {
        return -1;
    }

    QueueListNode* oldListHead = gQueueListHead;
    gQueueListHead = NULL;

    QueueListNode** nextPtr = &gQueueListHead;

    int rc = 0;
    for (int index = 0; index < count; index += 1) {
        QueueListNode* queueListNode = (QueueListNode*)internal_malloc(sizeof(*queueListNode));
        if (queueListNode == NULL) {
            rc = -1;
            break;
        }

        if (fileReadInt32(stream, &(queueListNode->time)) == -1) {
            internal_free(queueListNode);
            rc = -1;
            break;
        }

        if (fileReadInt32(stream, &(queueListNode->type)) == -1) {
            internal_free(queueListNode);
            rc = -1;
            break;
        }

        int objectId;
        if (fileReadInt32(stream, &objectId) == -1) {
            internal_free(queueListNode);
            rc = -1;
            break;
        }

        Object* obj;
        if (objectId == -2) {
            obj = NULL;
        } else {
            obj = objectFindFirst();
            while (obj != NULL) {
                obj = _inven_find_id(obj, objectId);
                if (obj != NULL) {
                    break;
                }
                obj = objectFindNext();
            }
        }

        queueListNode->owner = obj;

        EventTypeDescription* eventTypeDescription = &(gEventTypeDescriptions[queueListNode->type]);
        if (eventTypeDescription->readProc != NULL) {
            if (eventTypeDescription->readProc(stream, &(queueListNode->data)) == -1) {
                internal_free(queueListNode);
                rc = -1;
                break;
            }
        } else {
            queueListNode->data = NULL;
        }

        queueListNode->next = NULL;

        *nextPtr = queueListNode;
        nextPtr = &(queueListNode->next);
    }

    if (rc == -1) {
        while (gQueueListHead != NULL) {
            QueueListNode* next = gQueueListHead->next;

            EventTypeDescription* eventTypeDescription = &(gEventTypeDescriptions[gQueueListHead->type]);
            if (eventTypeDescription->freeProc != NULL) {
                eventTypeDescription->freeProc(gQueueListHead->data);
            }

            internal_free(gQueueListHead);

            gQueueListHead = next;
        }
    }

    if (oldListHead != NULL) {
        QueueListNode** v13 = &gQueueListHead;
        QueueListNode* v15;
        do {
            while (true) {
                QueueListNode* v14 = *v13;
                if (v14 == NULL) {
                    break;
                }

                if (v14->time > oldListHead->time) {
                    break;
                }

                v13 = &(v14->next);
            }
            v15 = oldListHead->next;
            oldListHead->next = *v13;
            *v13 = oldListHead;
            oldListHead = v15;
        } while (v15 != NULL);
    }

    return rc;
}

// 0x4A24E0
int queueSave(File* stream)
{
    QueueListNode* queueListNode;

    int count = 0;

    queueListNode = gQueueListHead;
    while (queueListNode != NULL) {
        count += 1;
        queueListNode = queueListNode->next;
    }

    if (fileWriteInt32(stream, count) == -1) {
        return -1;
    }

    queueListNode = gQueueListHead;
    while (queueListNode != NULL) {
        Object* object = queueListNode->owner;
        int objectId = object != NULL ? object->id : -2;

        if (fileWriteInt32(stream, queueListNode->time) == -1) {
            return -1;
        }

        if (fileWriteInt32(stream, queueListNode->type) == -1) {
            return -1;
        }

        if (fileWriteInt32(stream, objectId) == -1) {
            return -1;
        }

        EventTypeDescription* eventTypeDescription = &(gEventTypeDescriptions[queueListNode->type]);
        if (eventTypeDescription->writeProc != NULL) {
            if (eventTypeDescription->writeProc(stream, queueListNode->data) == -1) {
                return -1;
            }
        }

        queueListNode = queueListNode->next;
    }

    return 0;
}

// 0x4A258C
int queueAddEvent(int delay, Object* obj, void* data, int eventType)
{
    QueueListNode* newQueueListNode = (QueueListNode*)internal_malloc(sizeof(QueueListNode));
    if (newQueueListNode == NULL) {
        return -1;
    }

    int v1 = gameTimeGetTime();
    int v2 = v1 + delay;
    newQueueListNode->time = v2;
    newQueueListNode->type = eventType;
    newQueueListNode->owner = obj;
    newQueueListNode->data = data;

    if (obj != NULL) {
        obj->flags |= OBJECT_USED;
    }

    QueueListNode** v3 = &gQueueListHead;

    if (gQueueListHead != NULL) {
        QueueListNode* v4;

        do {
            v4 = *v3;
            if (v2 < v4->time) {
                break;
            }
            v3 = &(v4->next);
        } while (v4->next != NULL);
    }

    newQueueListNode->next = *v3;
    *v3 = newQueueListNode;

    return 0;
}

// 0x4A25F4
int queueRemoveEvents(Object* owner)
{
    QueueListNode* queueListNode = gQueueListHead;
    QueueListNode** queueListNodePtr = &gQueueListHead;

    while (queueListNode) {
        if (queueListNode->owner == owner) {
            QueueListNode* temp = queueListNode;

            queueListNode = queueListNode->next;
            *queueListNodePtr = queueListNode;

            EventTypeDescription* eventTypeDescription = &(gEventTypeDescriptions[temp->type]);
            if (eventTypeDescription->freeProc != NULL) {
                eventTypeDescription->freeProc(temp->data);
            }

            internal_free(temp);
        } else {
            queueListNodePtr = &(queueListNode->next);
            queueListNode = queueListNode->next;
        }
    }

    return 0;
}

// 0x4A264C
int queueRemoveEventsByType(Object* owner, int eventType)
{
    QueueListNode* queueListNode = gQueueListHead;
    QueueListNode** queueListNodePtr = &gQueueListHead;

    while (queueListNode) {
        if (queueListNode->owner == owner && queueListNode->type == eventType) {
            QueueListNode* temp = queueListNode;

            queueListNode = queueListNode->next;
            *queueListNodePtr = queueListNode;

            EventTypeDescription* eventTypeDescription = &(gEventTypeDescriptions[temp->type]);
            if (eventTypeDescription->freeProc != NULL) {
                eventTypeDescription->freeProc(temp->data);
            }

            internal_free(temp);
        } else {
            queueListNodePtr = &(queueListNode->next);
            queueListNode = queueListNode->next;
        }
    }

    return 0;
}

// Returns true if there is at least one event of given type scheduled.
//
// 0x4A26A8
bool queueHasEvent(Object* owner, int eventType)
{
    QueueListNode* queueListEvent = gQueueListHead;
    while (queueListEvent != NULL) {
        if (owner == queueListEvent->owner && eventType == queueListEvent->type) {
            return true;
        }

        queueListEvent = queueListEvent->next;
    }

    return false;
}

// 0x4A26D0
int queueProcessEvents()
{
    int time = gameTimeGetTime();
    int v1 = 0;

    while (gQueueListHead != NULL) {
        QueueListNode* queueListNode = gQueueListHead;
        if (time < queueListNode->time || v1 != 0) {
            break;
        }

        gQueueListHead = queueListNode->next;

        EventTypeDescription* eventTypeDescription = &(gEventTypeDescriptions[queueListNode->type]);
        v1 = eventTypeDescription->handlerProc(queueListNode->owner, queueListNode->data);

        if (eventTypeDescription->freeProc != NULL) {
            eventTypeDescription->freeProc(queueListNode->data);
        }

        internal_free(queueListNode);
    }

    return v1;
}

// 0x4A2748
void queueClear()
{
    QueueListNode* queueListNode = gQueueListHead;
    while (queueListNode != NULL) {
        QueueListNode* next = queueListNode->next;

        EventTypeDescription* eventTypeDescription = &(gEventTypeDescriptions[queueListNode->type]);
        if (eventTypeDescription->freeProc != NULL) {
            eventTypeDescription->freeProc(queueListNode->data);
        }

        internal_free(queueListNode);

        queueListNode = next;
    }

    gQueueListHead = NULL;
}

// 0x4A2790
void _queue_clear_type(int eventType, QueueEventHandler* fn)
{
    QueueListNode** ptr = &gQueueListHead;
    QueueListNode* curr = *ptr;

    while (curr != NULL) {
        if (eventType == curr->type) {
            QueueListNode* tmp = curr;

            *ptr = curr->next;
            curr = *ptr;

            if (fn != NULL && fn(tmp->owner, tmp->data) != 1) {
                *ptr = tmp;
                ptr = &(tmp->next);
            } else {
                EventTypeDescription* eventTypeDescription = &(gEventTypeDescriptions[tmp->type]);
                if (eventTypeDescription->freeProc != NULL) {
                    eventTypeDescription->freeProc(tmp->data);
                }

                internal_free(tmp);

                // SFALL: Re-read next event since `fn` handler can change it.
                // This fixes crash when leaving the map while waiting for
                // someone to die of a super stimpak overdose.
                curr = *ptr;
            }
        } else {
            ptr = &(curr->next);
            curr = *ptr;
        }
    }
}

// TODO: Make unsigned.
//
// 0x4A2808
int queueGetNextEventTime()
{
    if (gQueueListHead == NULL) {
        return 0;
    }

    return gQueueListHead->time;
}

// 0x4A281C
static int flareEventProcess(Object* obj, void* data)
{
    _obj_destroy(obj);
    return 1;
}

// 0x4A2828
static int explosionEventProcess(Object* obj, void* data)
{
    return _queue_do_explosion_(obj, true);
}

// 0x4A2830
static int _queue_explode_exit(Object* obj, void* data)
{
    return _queue_do_explosion_(obj, false);
}

// 0x4A2834
static int _queue_do_explosion_(Object* explosive, bool a2)
{
    int tile;
    int elevation;

    Object* owner = objectGetOwner(explosive);
    if (owner) {
        tile = owner->tile;
        elevation = owner->elevation;
    } else {
        tile = explosive->tile;
        elevation = explosive->elevation;
    }

    int maxDamage;
    int minDamage;

    // SFALL
    explosiveGetDamage(explosive->pid, &minDamage, &maxDamage);

    // FIXME: I guess this is a little bit wrong, dude can never be null, I
    // guess it needs to check if owner is dude.
    if (gDude != NULL) {
        if (perkHasRank(gDude, PERK_DEMOLITION_EXPERT)) {
            maxDamage += 10;
            minDamage += 10;
        }
    }

    if (actionExplode(tile, elevation, minDamage, maxDamage, gDude, a2) == -2) {
        queueAddEvent(50, explosive, NULL, EVENT_TYPE_EXPLOSION);
    } else {
        _obj_destroy(explosive);
    }

    return 1;
}

// 0x4A28E4
static int explosionFailureEventProcess(Object* obj, void* data)
{
    MessageListItem msg;

    // Due to your inept handling, the explosive detonates prematurely.
    msg.num = 4000;
    if (messageListGetItem(&gMiscMessageList, &msg)) {
        displayMonitorAddMessage(msg.text);
    }

    return _queue_do_explosion_(obj, true);
}

// 0x4A2920
void _queue_leaving_map()
{
    for (int eventType = 0; eventType < EVENT_TYPE_COUNT; eventType++) {
        EventTypeDescription* eventTypeDescription = &(gEventTypeDescriptions[eventType]);
        if (eventTypeDescription->field_10) {
            _queue_clear_type(eventType, eventTypeDescription->field_14);
        }
    }
}

// 0x4A294C
bool queueIsEmpty()
{
    return gQueueListHead == NULL;
}

// 0x4A295C
void* queueFindFirstEvent(Object* owner, int eventType)
{
    QueueListNode* queueListNode = gQueueListHead;
    while (queueListNode != NULL) {
        if (owner == queueListNode->owner && eventType == queueListNode->type) {
            gLastFoundQueueListNode = queueListNode;
            return queueListNode->data;
        }
        queueListNode = queueListNode->next;
    }

    gLastFoundQueueListNode = NULL;
    return NULL;
}

// 0x4A2994
void* queueFindNextEvent(Object* owner, int eventType)
{
    if (gLastFoundQueueListNode != NULL) {
        QueueListNode* queueListNode = gLastFoundQueueListNode->next;
        while (queueListNode != NULL) {
            if (owner == queueListNode->owner && eventType == queueListNode->type) {
                gLastFoundQueueListNode = queueListNode;
                return queueListNode->data;
            }
            queueListNode = queueListNode->next;
        }
    }

    gLastFoundQueueListNode = NULL;

    return NULL;
}
