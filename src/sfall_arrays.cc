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
static ArrayId stackArrayId = 0;

#define ARRAY_MAX_STRING (255) // maximum length of string to be stored as array key or value
#define ARRAY_MAX_SIZE (100000) // maximum number of array elements,

// special actions for arrays using array_resize operator
#define ARRAY_ACTION_SORT (-2)
#define ARRAY_ACTION_RSORT (-3)
#define ARRAY_ACTION_REVERSE (-4)
#define ARRAY_ACTION_SHUFFLE (-5)

// TODO: Move me into separate file "sfall_arrays_utils"
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

enum class ArrayElementType {
    INT,
    FLOAT,
    STRING,
    POINTER
};

class ArrayElement {
private:
    ArrayElementType type;
    union {
        int integerValue;
        float floatValue;
        unsigned char* stringValue;
        void* pointerValue;
    };

public:
    ArrayElement() {
        // todo
    };
    
    // TODO: Remove all other constructors

    ArrayElement(ProgramValue programValue, Program* program)
    {
        // todo
    }
    ProgramValue toValue(Program* program) const
    {
        // todo
        return ProgramValue(0);
    }

    bool operator<(ArrayElement const& other) const
    {
        // todo
        return false;
    }
    bool operator==(ArrayElement const& other) const
    {
        // todo
        /*
        if (!a.isString()) {
            return a == b;
        };
        if (!b.isString()) {
            return false;
        };
        auto str1 = programGetString(program, a.opcode, a.integerValue);
        auto str2 = programGetString(program, a.opcode, a.integerValue);
        if (!str1 || !str2) {
            return false;
        };
        return strcmp(str1, str2) == 0;
        */
        return false;
    }
};

class SFallArray {
protected:
    uint32_t mFlags;

public:
    virtual int size() = 0;
    virtual ProgramValue GetArrayKey(int index, Program* program) = 0;
    virtual ProgramValue GetArray(const ProgramValue& key, Program* program) = 0;
    virtual void SetArray(const ProgramValue& key, const ProgramValue& val, bool allowUnset, Program* program) = 0;
    virtual void ResizeArray(int newLen) = 0;
    virtual ProgramValue ScanArray(const ProgramValue& value, Program* program) = 0;

    virtual ~SFallArray() = default;
};

/*
bool ProgramValue::operator<(ProgramValue const& other) const
{
    if (opcode != other.opcode) {
        return opcode < other.opcode;
    }

    switch (opcode) {
    case VALUE_TYPE_INT:
    case VALUE_TYPE_STRING:
    case VALUE_TYPE_DYNAMIC_STRING:
        return integerValue < other.integerValue;
    case VALUE_TYPE_PTR:
        return pointerValue < other.pointerValue;
    case VALUE_TYPE_FLOAT:
        return floatValue < other.floatValue;
    default:
        throw(std::exception());
    }
}

bool ProgramValue::operator==(ProgramValue const& other) const
{
    if (opcode != other.opcode) {
        return false;
    }

    switch (opcode) {
    case VALUE_TYPE_INT:
    case VALUE_TYPE_STRING:
    case VALUE_TYPE_DYNAMIC_STRING:
        return integerValue == other.integerValue;
    case VALUE_TYPE_PTR:
        return pointerValue == other.pointerValue;
    case VALUE_TYPE_FLOAT:
        return floatValue == other.floatValue;
    default:
        throw(std::exception());
    }
}
*/

class SFallArrayList : public SFallArray {
private:
    std::vector<ArrayElement> values;

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

    ProgramValue GetArrayKey(int index, Program* program)
    {
        if (index < -1 || index > size()) {
            return ProgramValue(0);
        };
        if (index == -1) { // special index to indicate if array is associative
            return ProgramValue(0);
        };
        return ProgramValue(index);
    }

    ProgramValue GetArray(const ProgramValue& key, Program* program)
    {
        auto element_index = key.asInt();
        if (element_index < 0 || element_index >= size()) {
            return ProgramValue(0);
        };
        return values[element_index].toValue(program);
    }

    void SetArray(const ProgramValue& key, const ProgramValue& val, bool allowUnset, Program* program)
    {
        if (key.isInt()) {
            auto index = key.asInt();
            if (index >= 0 && index < size()) {
                values[index] = ArrayElement { key, program };
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

            std::vector<ArrayElement> newValues;
            newValues.reserve(newLen);
            for (size_t i = 0; i < std::min(newLen, size()); i++) {
                newValues.push_back(values[i]);
            };
            values = newValues;
        } else if (newLen >= ARRAY_ACTION_SHUFFLE) {
            ListSort(values, newLen, std::less<ArrayElement>());
        }
    }

    ProgramValue ScanArray(const ProgramValue& value, Program* program)
    {
        auto arrValue = ArrayElement { value, program };
        for (size_t i = 0; i < size(); i++) {
            if (arrValue == values[i]) {
                return ProgramValue(i);
            }
        }
        return ProgramValue(-1);
    }
};

class SFallArrayAssoc : public SFallArray {
private:
    // TODO: SFall copies strings
    std::vector<ArrayElement> keys;
    std::map<ArrayElement, ArrayElement> map;

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

    ProgramValue GetArrayKey(int index, Program* program)
    {
        if (index < -1 || index > size()) {
            return ProgramValue(0);
        };
        if (index == -1) { // special index to indicate if array is associative
            return ProgramValue(1);
        };

        return keys[index].toValue(program);
    }

    ProgramValue GetArray(const ProgramValue& key, Program* program)
    {
        auto iter = map.find(ArrayElement { key, program });
        if (iter == map.end()) {
            return ProgramValue(0);
        };

        return iter->second.toValue(program);
    }

    void SetArray(const ProgramValue& key, const ProgramValue& val, bool allowUnset, Program* program)
    {
        auto keyEl = ArrayElement { key, program };

        auto iter = map.find(keyEl);

        bool lookupMap = (mFlags & SFALL_ARRAYFLAG_CONSTVAL) != 0;

        if (iter != map.end() && lookupMap) {
            // don't update value of key
            return;
        }

        if (allowUnset && !lookupMap && val.isInt() && val.asInt() == 0) {
            // after assigning zero to a key, no need to store it, because "get_array" returns 0 for non-existent keys: try unset
            if (iter != map.end()) {
                map.erase(iter);
                std::vector<ArrayElement> newKeys;
                newKeys.reserve(keys.size() - 1);
                for (auto keyCandidate : keys) {
                    if (keyCandidate == keyEl) {
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

                keys.push_back(keyEl);
                map[keyEl] = ArrayElement { val, program };
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
            std::vector<ArrayElement> newKeys;
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

    ProgramValue ScanArray(const ProgramValue& value, Program* program)
    {
        auto valueEl = ArrayElement { value, program };
        for (const auto& pair : map) {
            if (valueEl == pair.second) {
                return pair.first.toValue(program);
            }
        }
        return ProgramValue(-1);
    }
};

void SFallArrayAssoc::MapSort(int type)
{
    bool sortByValue = false;
    if (type < ARRAY_ACTION_SHUFFLE) {
        type += 4;
        sortByValue = true;
    }

    if (sortByValue) {
        ListSort(keys, type, [this](const ArrayElement& a, const ArrayElement& b) -> bool {
            return this->map[a] < this->map[b];
        });
    } else {
        ListSort(keys, type, std::less<ArrayElement>());
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

SFallArray* get_array_by_id(ArrayId array_id)
{
    auto it = arrays.find(array_id);
    if (it == arrays.end()) {
        return nullptr;
    } else {
        return it->second.get();
    }
}

ProgramValue GetArrayKey(ArrayId array_id, int index, Program* program)
{
    auto arr = get_array_by_id(array_id);
    if (!arr) {
        return ProgramValue(0);
    };
    return arr->GetArrayKey(index, program);
}

int LenArray(ArrayId array_id)
{
    auto arr = get_array_by_id(array_id);
    if (!arr) {
        return -1;
    };

    return arr->size();
}

ProgramValue GetArray(ArrayId array_id, const ProgramValue& key, Program* program)
{
    auto arr = get_array_by_id(array_id);

    if (!arr) {
        return ProgramValue(0);
    };

    return arr->GetArray(key, program);
}

void SetArray(ArrayId array_id, const ProgramValue& key, const ProgramValue& val, bool allowUnset, Program* program)
{
    auto arr = get_array_by_id(array_id);
    if (!arr) {
        return;
    }

    arr->SetArray(key, val, allowUnset, program);
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
    stackArrayId = 0;
}

void ResizeArray(ArrayId array_id, int newLen)
{
    auto arr = get_array_by_id(array_id);
    if (!arr) {
        return;
    };
    arr->ResizeArray(newLen);
}

int StackArray(const ProgramValue& key, const ProgramValue& val, Program* program)
{
    if (stackArrayId == 0) {
        return 0;
    }

    auto arr = get_array_by_id(stackArrayId);
    if (!arr) {
        return 0;
    };

    auto size = arr->size();
    if (size >= ARRAY_MAX_SIZE) return 0;
    if (key.asInt() >= size) arr->ResizeArray(size + 1);

    SetArray(stackArrayId, key, val, false, program);
    return 0;
}

ProgramValue ScanArray(ArrayId array_id, const ProgramValue& val, Program* program)
{
    auto arr = get_array_by_id(array_id);
    if (!arr) {
        return ProgramValue(-1);
    };

    return arr->ScanArray(val, program);
}

}