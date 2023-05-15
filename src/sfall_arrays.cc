#include "sfall_arrays.h"
#include "interpreter.h"

#include <algorithm>
#include <cstdint>
#include <map>
#include <memory>
#include <random>
#include <stdexcept>
#include <string.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>

/*
Used https://gist.github.com/phobos2077/6bf357c49caaf515371a13f9a2d74a41 for testing
*/

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

/**
 * This is mostly the same as ProgramValue but it owns strings.
 *
 * This is done because when we pop dynamic string element from
 * the stack we decrease ref count for this string and it memory
 * can be freed.
 *
 * In theory arrays can be shared between programs so we also
 * have to copy static strings.
 *
 */
class ArrayElement {
private:
    ArrayElementType type;
    union {
        int integerValue;
        float floatValue;
        char* stringValue;
        void* pointerValue;
    };

    void init_from_string(const char* str, int sLen)
    {
        type = ArrayElementType::STRING;

        if (sLen == -1) sLen = strlen(str);
        if (sLen >= ARRAY_MAX_STRING) sLen = ARRAY_MAX_STRING - 1; // memory safety

        stringValue = (char*)malloc(sLen + 1);
        memcpy(stringValue, str, sLen);
        stringValue[sLen] = '\0';
    }

public:
    ArrayElement()
        : type(ArrayElementType::INT)
        , integerValue(0) {
            // nothing here
        };

    ArrayElement(const ArrayElement& other) = delete;
    ArrayElement& operator=(const ArrayElement& rhs) = delete;
    ArrayElement(ArrayElement&& other)
    {
        // Maybe this can be done simpler way?
        type = other.type;
        switch (type) {
        case ArrayElementType::INT:
            integerValue = other.integerValue;
            break;
        case ArrayElementType::FLOAT:
            floatValue = other.floatValue;
            break;
        case ArrayElementType::POINTER:
            pointerValue = other.pointerValue;
            break;
        case ArrayElementType::STRING:
            stringValue = other.stringValue;
            other.stringValue = nullptr;
            break;
        default:
            throw(std::exception());
        }
    };
    ArrayElement& operator=(ArrayElement&& other)
    {
        // Maybe this can be done simpler way?
        type = other.type;
        switch (type) {
        case ArrayElementType::INT:
            integerValue = other.integerValue;
            break;
        case ArrayElementType::FLOAT:
            floatValue = other.floatValue;
            break;
        case ArrayElementType::POINTER:
            pointerValue = other.pointerValue;
            break;
        case ArrayElementType::STRING:
            stringValue = other.stringValue;
            other.stringValue = nullptr;
            break;
        default:
            throw(std::exception());
        }
        return *this;
    }

    ArrayElement(ProgramValue programValue, Program* program)
    {
        switch (programValue.opcode) {
        case VALUE_TYPE_INT:
            type = ArrayElementType::INT;
            integerValue = programValue.integerValue;
            break;
        case VALUE_TYPE_FLOAT:
            type = ArrayElementType::FLOAT;
            floatValue = programValue.floatValue;
            break;
        case VALUE_TYPE_PTR:
            type = ArrayElementType::POINTER;
            pointerValue = programValue.pointerValue;
            break;
        case VALUE_TYPE_STRING:
        case VALUE_TYPE_DYNAMIC_STRING:
            auto str = programGetString(program, programValue.opcode, programValue.integerValue);
            init_from_string(str, -1);
            break;
        }
    }

    ArrayElement(const char* str)
    {
        init_from_string(str, -1);
    }
    ArrayElement(const char* str, int sLen)
    {
        init_from_string(str, sLen);
    }

    ProgramValue toValue(Program* program) const
    {
        ProgramValue out;
        switch (type) {
        case ArrayElementType::INT:
            out.opcode = VALUE_TYPE_INT;
            out.integerValue = integerValue;
            return out;
        case ArrayElementType::FLOAT:
            out.opcode = VALUE_TYPE_FLOAT;
            out.floatValue = floatValue;
            return out;
        case ArrayElementType::POINTER:
            out.opcode = VALUE_TYPE_PTR;
            out.pointerValue = pointerValue;
            return out;
        case ArrayElementType::STRING:
            out.opcode = VALUE_TYPE_DYNAMIC_STRING;
            out.integerValue = programPushString(program, stringValue);
            return out;
        default:
            throw(std::exception());
        }
    }

    bool operator<(ArrayElement const& other) const
    {
        if (type != other.type) {
            return type < other.type;
        };
        switch (type) {
        case ArrayElementType::INT:
            return integerValue < other.integerValue;
        case ArrayElementType::FLOAT:
            return floatValue < other.floatValue;
        case ArrayElementType::POINTER:
            return pointerValue < other.pointerValue;
        case ArrayElementType::STRING:
            return strcmp(stringValue, other.stringValue) < 0;
        default:
            throw(std::exception());
        }
    }
    bool operator==(ArrayElement const& other) const
    {
        if (type != other.type) {
            return false;
        };
        switch (type) {
        case ArrayElementType::INT:
            return integerValue == other.integerValue;
        case ArrayElementType::FLOAT:
            return floatValue == other.floatValue;
        case ArrayElementType::POINTER:
            return pointerValue == other.pointerValue;
        case ArrayElementType::STRING:
            return strcmp(stringValue, other.stringValue) == 0;
        default:
            throw(std::exception());
        }
    }

    ~ArrayElement()
    {
        if (type == ArrayElementType::STRING) {
            free(stringValue);
        };
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
    virtual void SetArray(const ProgramValue& key, ArrayElement&& val, bool allowUnset) = 0;
    virtual void ResizeArray(int newLen) = 0;
    virtual ProgramValue ScanArray(const ProgramValue& value, Program* program) = 0;

    virtual ~SFallArray() = default;
};

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
        SetArray(key, ArrayElement { val, program }, allowUnset);
    }
    void SetArray(const ProgramValue& key, ArrayElement&& val, bool allowUnset)
    {
        if (key.isInt()) {
            auto index = key.asInt();
            if (index >= 0 && index < size()) {
                std::swap(values[index], val);
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

            values.resize(newLen);
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

                if (keys.size() == 0) {
                    throw(std::exception());
                }
                auto it = std::find(keys.begin(),
                    keys.end(), keyEl);
                if (it == keys.end()) {
                    throw(std::exception());
                };
                auto idx = it - keys.begin();
                if (idx != keys.size() - 1) {
                    std::swap(keys[idx], keys[size() - 1]);
                    keys.resize(keys.size() - 1);
                }
            }
        } else {
            if (iter == map.end()) {
                // size check
                if (size() >= ARRAY_MAX_SIZE) return;

                keys.push_back(std::move(keyEl));

                // Not very good that we copy string into map key and into keys array
                map.emplace(ArrayElement { key, program }, ArrayElement { val, program });
            } else {
                auto newValue = ArrayElement { val, program };
                std::swap(map.at(keyEl), newValue);
            }
        }
    }
    void SetArray(const ProgramValue& key, ArrayElement&& val, bool allowUnset)
    {
        // TODO
    }
    void ResizeArray(int newLen)
    {
        if (newLen == -1 || size() == newLen) {
            return;
        };

        // only allow to reduce number of elements (adding range of elements is meaningless for maps)
        if (newLen >= 0 && newLen < size()) {
            for (size_t i = newLen; i < size(); i++) {
                map.erase(keys[i]);
            };
            keys.resize(newLen);
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
            // Why return this->map[a] < this->map[b] does not work? Why it requires copy constructor?
            auto itA = this->map.find(a);
            auto itB = this->map.find(a);
            if (itA == map.end() || itB == map.end()) {
                throw(std::exception());
            };
            return itA->second < itB->second;
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

ArrayId StringSplit(const char* str, const char* split)
{
    int splitLen = strlen(split);

    ArrayId array_id = CreateTempArray(0, 0);
    auto arr = get_array_by_id(array_id);

    if (!splitLen) {
        int count = strlen(str);

        arr->ResizeArray(count);
        for (int i = 0; i < count; i++) {
            arr->SetArray(ProgramValue { i }, ArrayElement { &str[i], 1 }, false);
        }
    } else {
        int count = 1;
        const char *ptr = str, *newptr;
        while (true) {
            newptr = strstr(ptr, split);
            if (!newptr) break;
            count++;
            ptr = newptr + splitLen;
        }
        arr->ResizeArray(count);

        ptr = str;
        count = 0;
        while (true) {
            newptr = strstr(ptr, split);
            int len = (newptr) ? newptr - ptr : strlen(ptr);

            arr->SetArray(ProgramValue { count++ }, ArrayElement { ptr, len }, false);

            if (!newptr) {
                break;
            }
            ptr = newptr + splitLen;
        }
    }
    return array_id;
}

}