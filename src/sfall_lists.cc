#include "sfall_lists.h"

#include <unordered_map>

#include "object.h"
#include "scripts.h"

namespace fallout {

// Due to bad design of |ListType| it's |LIST_ITEMS| and |LIST_CRITTERS| do not
// match |OBJ_TYPE_CRITTER| and |OBJ_TYPE_ITEM|.
static constexpr int kObjectTypeToListType[] = {
    /*    OBJ_TYPE_ITEM */ LIST_ITEMS,
    /* OBJ_TYPE_CRITTER */ LIST_CRITTERS,
    /* OBJ_TYPE_SCENERY */ LIST_SCENERY,
    /*    OBJ_TYPE_WALL */ LIST_WALLS,
    /*    OBJ_TYPE_TILE */ LIST_TILES,
    /*    OBJ_TYPE_MISC */ LIST_MISC,
};

static constexpr int kObjectTypeToListTypeSize = sizeof(kObjectTypeToListType) / sizeof(kObjectTypeToListType[0]);

// As in Sfall.
static constexpr int kInitialListId = 0xCCCCCC;

// Loosely based on [sList] from Sfall.
struct List {
    std::vector<Object*> objects;
    size_t pos = 0;
};

struct SfallListsState {
    std::unordered_map<int, List> lists;
    int nextListId = kInitialListId;
};

static SfallListsState* _state = nullptr;

bool sfallListsInit()
{
    _state = new (std::nothrow) SfallListsState();
    if (_state == nullptr) {
        return false;
    }

    return true;
}

void sfallListsReset()
{
    _state->nextListId = kInitialListId;
    _state->lists.clear();
}

void sfallListsExit()
{
    if (_state != nullptr) {
        delete _state;
        _state = nullptr;
    }
}

int sfallListsCreate(int listType)
{
    int listId = _state->nextListId++;
    List& list = _state->lists[listId];

    sfall_lists_fill(listType, list.objects);

    return listId;
}

Object* sfallListsGetNext(int listId)
{
    auto it = _state->lists.find(listId);
    if (it != _state->lists.end()) {
        List& list = it->second;
        if (list.pos < list.objects.size()) {
            return list.objects[list.pos++];
        }
    }

    return nullptr;
}

void sfallListsDestroy(int listId)
{
    auto it = _state->lists.find(listId);
    if (it != _state->lists.end()) {
        _state->lists.erase(it);
    }
}

void sfall_lists_fill(int type, std::vector<Object*>& objects)
{
    if (type == LIST_TILES) {
        // For unknown reason this list type is not implemented in Sfall.
        return;
    }

    objects.reserve(100);

    if (type == LIST_SPATIAL) {
        for (int elevation = 0; elevation < ELEVATION_COUNT; elevation++) {
            Script* script = scriptGetFirstSpatialScript(elevation);
            while (script != nullptr) {
                Object* obj = script->owner;
                if (obj == nullptr) {
                    obj = scriptGetSelf(script->program);
                }
                objects.push_back(obj);
                script = scriptGetNextSpatialScript();
            }
        }
    } else {
        // CE: Implementation is slightly different. Sfall manually loops thru
        // elevations (3) and hexes (40000) and use |objectFindFirstAtLocation|
        // (originally |obj_find_first_at_tile|) to obtain next object. This
        // functionality is already implemented in |objectFindFirst| and
        // |objectFindNext|.
        //
        // As a small optimization |LIST_ALL| is handled separately since there
        // is no need to check object type.
        if (type == LIST_ALL) {
            Object* obj = objectFindFirst();
            while (obj != nullptr) {
                objects.push_back(obj);
                obj = objectFindNext();
            }
        } else {
            Object* obj = objectFindFirst();
            while (obj != nullptr) {
                int objectType = PID_TYPE(obj->pid);
                if (objectType < kObjectTypeToListTypeSize
                    && kObjectTypeToListType[objectType] == type) {
                    objects.push_back(obj);
                }
                obj = objectFindNext();
            }
        }
    }
}

} // namespace fallout
