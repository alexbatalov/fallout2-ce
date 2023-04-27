#include "sfall_arrays.h"
#include "interpreter.h"

#include <algorithm>
#include <cstdint>
#include <map>
#include <memory>
#include <random>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <vector>
namespace fallout {

static ArrayId nextArrayID = 1;
static ArrayId stackArrayId = 1;

#define ARRAY_MAX_STRING (255) // maximum length of string to be stored as array key or value
#define ARRAY_MAX_SIZE (100000) // maximum number of array elements,

// special actions for arrays using array_resize operator
#define ARRAY_ACTION_SORT (-2)
#define ARRAY_ACTION_RSORT (-3)
#define ARRAY_ACTION_REVERSE (-4)
#define ARRAY_ACTION_SHUFFLE (-5)

template <class T, typename Compare>
static void ListSort(std::vector<T>& arr, int type, Compare cmp)
{
    switch (type) {
    case ARRAY_ACTION_SORT: // sort ascending
        std::sort(arr.begin(), arr.end(), cmp);
        break;
    case ARRAY_ACTION_RSORT: // sort descending
        std::sort(arr.rbegin(), arr.rend(), cmp);
        break;
    case ARRAY_ACTION_REVERSE: // reverse elements
        std::reverse(arr.rbegin(), arr.rend());
        break;
    case ARRAY_ACTION_SHUFFLE: // shuffle elements
        std::random_device rd;
        std::mt19937 g(rd());
        std::shuffle(arr.begin(), arr.end(), g);
        break;
    }
}

class SFallArray {
protected:
    uint32_t mFlags;

public:
    virtual int size() = 0;
    virtual ProgramValue GetArrayKey(int index) = 0;
    virtual ProgramValue GetArray(const ProgramValue& key) = 0;
    virtual void SetArray(const ProgramValue& key, const ProgramValue& val, bool allowUnset) = 0;
    virtual void ResizeArray(int newLen) = 0;
};

class SFallArrayList : public SFallArray {
private:
    // TODO: SFall copies strings
    std::vector<ProgramValue> values;

public:
    SFallArrayList() = delete;

    SFallArrayList(unsigned int len, uint32_t flags)
    {
        values.resize(len);
        mFlags = flags;
    }

    int size()
    {
        return values.size();
    }

    ProgramValue GetArrayKey(int index)
    {
        if (index < -1 || index > size()) {
            return ProgramValue(0);
        };
        if (index == -1) { // special index to indicate if array is associative
            return ProgramValue(0);
        };
        return ProgramValue(index);
    }

    ProgramValue GetArray(const ProgramValue& key)
    {
        auto element_index = key.asInt();
        if (element_index < 0 || element_index >= size()) {
            return ProgramValue(0);
        };
        return values[element_index];
    }

    void SetArray(const ProgramValue& key, const ProgramValue& val, bool allowUnset)
    {
        if (key.isInt()) {
            auto index = key.asInt();
            if (index >= 0 && index < size()) {
                values[index] = key;
            }
        }
    }

    void ResizeArray(int newLen)
    {
        if (newLen == -1 || size() == newLen) {
            return;
        };
        if (newLen == 0) {
            values.clear();
        } else if (newLen > 0) {
            if (newLen > ARRAY_MAX_SIZE) newLen = ARRAY_MAX_SIZE; // safety

            std::vector<ProgramValue> newValues;
            newValues.reserve(newLen);
            for (size_t i = 0; i < std::min(newLen, size()); i++) {
                newValues.push_back(values[i]);
            };
            values = newValues;
        } else if (newLen >= ARRAY_ACTION_SHUFFLE) {
            ListSort(values, newLen, std::less<ProgramValue>());
        }
    }
};

class SFallArrayAssoc : public SFallArray {
private:
    // TODO: SFall copies strings
    std::vector<ProgramValue> keys;
    std::map<ProgramValue, ProgramValue> map;

    void MapSort(int newLen);

public:
    SFallArrayAssoc() = delete;

    SFallArrayAssoc(uint32_t flags)
    {
        mFlags = flags;
    }

    int size()
    {
        return keys.size();
    }

    ProgramValue GetArrayKey(int index)
    {
        if (index < -1 || index > size()) {
            return ProgramValue(0);
        };
        if (index == -1) { // special index to indicate if array is associative
            return ProgramValue(1);
        };

        return keys[index];
    }

    ProgramValue GetArray(const ProgramValue& key)
    {
        auto iter = map.find(key);
        if (iter == map.end()) {
            return ProgramValue(0);
        };

        return iter->second;
    }

    void SetArray(const ProgramValue& key, const ProgramValue& val, bool allowUnset)
    {
        auto iter = map.find(key);

        bool lookupMap = (mFlags & SFALL_ARRAYFLAG_CONSTVAL) != 0;

        if (iter != map.end() && lookupMap) {
            // don't update value of key
            return;
        }

        if (allowUnset && !lookupMap && val.isInt() && val.asInt() == 0) {
            // after assigning zero to a key, no need to store it, because "get_array" returns 0 for non-existent keys: try unset
            if (iter != map.end()) {
                map.erase(iter);
                std::vector<ProgramValue> newKeys;
                newKeys.reserve(keys.size() - 1);
                for (auto keyCandidate : keys) {
                    if (keyCandidate == key) {
                        // skip this key
                    } else {
                        newKeys.push_back(keyCandidate);
                    }
                };
                keys = newKeys;
            }
        } else {
            if (iter == map.end()) {
                // size check
                if (size() >= ARRAY_MAX_SIZE) return;

                keys.push_back(key);
                map[key] = val;
            }
        }
    }
    void ResizeArray(int newLen)
    {
        if (newLen == -1 || size() == newLen) {
            return;
        };

        // only allow to reduce number of elements (adding range of elements is meaningless for maps)
        if (newLen >= 0 && newLen < size()) {
            std::vector<ProgramValue> newKeys;
            newKeys.reserve(newLen);

            for (size_t i = 0; i < newLen; i++) {
                newKeys[i] = keys[i];
            };

            for (size_t i = newLen; i < size(); i++) {
                map.erase(keys[i]);
            };
            keys = newKeys;
        } else if (newLen < 0) {
            if (newLen < (ARRAY_ACTION_SHUFFLE - 2)) return;
            MapSort(newLen);
        }
    };
};

void SFallArrayAssoc::MapSort(int type)
{
    bool sortByValue = false;
    if (type < ARRAY_ACTION_SHUFFLE) {
        type += 4;
        sortByValue = true;
    }

    if (sortByValue) {
        ListSort(keys, type, [this](const ProgramValue& a, const ProgramValue& b) -> bool {
            return this->map[a] < this->map[b];
        });
    } else {
        ListSort(keys, type, std::less<ProgramValue>());
    }
}

std::unordered_map<ArrayId, std::unique_ptr<SFallArray>> arrays;
std::unordered_set<ArrayId> temporaryArrays;

ArrayId CreateArray(int len, uint32_t flags)
{
    flags = (flags & ~1); // reset 1 bit

    if (len < 0) {
        flags |= SFALL_ARRAYFLAG_ASSOC;
    } else if (len > ARRAY_MAX_SIZE) {
        len = ARRAY_MAX_SIZE; // safecheck
    };

    ArrayId array_id = nextArrayID++;

    stackArrayId = array_id;

    if (flags & SFALL_ARRAYFLAG_ASSOC) {
        arrays.emplace(std::make_pair(array_id, std::make_unique<SFallArrayAssoc>(flags)));
    } else {
        arrays.emplace(std::make_pair(array_id, std::make_unique<SFallArrayList>(len, flags)));
    }

    return array_id;
}

ArrayId CreateTempArray(int len, uint32_t flags)
{
    ArrayId array_id = CreateArray(len, flags);
    temporaryArrays.insert(array_id);
    return array_id;
}

std::unique_ptr<SFallArray>& get_array_by_id(ArrayId array_id)
{
    static auto not_found = std::unique_ptr<SFallArray>(nullptr);
    auto it = arrays.find(array_id);
    if (it == arrays.end()) {
        return not_found;
    } else {
        return it->second;
    }
}

ProgramValue GetArrayKey(ArrayId array_id, int index)
{
    auto& arr = get_array_by_id(array_id);
    if (!arr) {
        return ProgramValue(0);
    };
    return arr->GetArrayKey(index);
}

int LenArray(ArrayId array_id)
{
    auto& arr = get_array_by_id(array_id);
    if (!arr) {
        return -1;
    };

    return arr->size();
}

ProgramValue GetArray(ArrayId array_id, const ProgramValue& key)
{
    auto& arr = get_array_by_id(array_id);

    if (!arr) {
        return ProgramValue(0);
    };

    return arr->GetArray(key);
}

void SetArray(ArrayId array_id, const ProgramValue& key, const ProgramValue& val, bool allowUnset)
{
    auto& arr = get_array_by_id(array_id);
    if (!arr) {
        return;
    }

    arr->SetArray(key, val, allowUnset);
}

void FreeArray(ArrayId array_id)
{
    // TODO: remove from saved_arrays
    arrays.erase(array_id);
}

void FixArray(ArrayId id)
{
    temporaryArrays.erase(id);
}

void DeleteAllTempArrays()
{
    for (auto it = temporaryArrays.begin(); it != temporaryArrays.end(); ++it) {
        FreeArray(*it);
    }
    temporaryArrays.clear();
}

void sfallArraysReset()
{
    temporaryArrays.clear();
    arrays.clear();
    nextArrayID = 1;
    stackArrayId = 1;
}

void ResizeArray(ArrayId array_id, int newLen)
{
    auto& arr = get_array_by_id(array_id);
    if (!arr) {
        return;
    };
    arr->ResizeArray(newLen);
}
}