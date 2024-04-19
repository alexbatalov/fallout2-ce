#include "sfall_arrays.h"

#include <assert.h>
#include <string.h>

#include <algorithm>
#include <memory>
#include <random>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "interpreter.h"
#include "sfall_lists.h"

namespace fallout {

static constexpr ArrayId kInitialArrayId = 1;

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
public:
    ArrayElement()
        : type(ArrayElementType::INT)
        , value({ 0 })
    {
    }

    ArrayElement(const ArrayElement& other) = delete;

    ArrayElement& operator=(const ArrayElement& rhs) = delete;

    ArrayElement(ArrayElement&& other) noexcept
        : type(ArrayElementType::INT)
        , value({ 0 })
    {
        std::swap(type, other.type);
        std::swap(value, other.value);
    }

    ArrayElement& operator=(ArrayElement&& other) noexcept
    {
        std::swap(type, other.type);
        std::swap(value, other.value);
        return *this;
    }

    ArrayElement(ProgramValue programValue, Program* program)
    {
        switch (programValue.opcode) {
        case VALUE_TYPE_INT:
            type = ArrayElementType::INT;
            value.integerValue = programValue.integerValue;
            break;
        case VALUE_TYPE_FLOAT:
            type = ArrayElementType::FLOAT;
            value.floatValue = programValue.floatValue;
            break;
        case VALUE_TYPE_PTR:
            type = ArrayElementType::POINTER;
            value.pointerValue = programValue.pointerValue;
            break;
        case VALUE_TYPE_STRING:
        case VALUE_TYPE_DYNAMIC_STRING:
            setString(programGetString(program, programValue.opcode, programValue.integerValue), -1);
            break;
        default:
            type = ArrayElementType::INT;
            value.integerValue = 0;
            break;
        }
    }

    ArrayElement(const char* str)
    {
        setString(str, -1);
    }

    ArrayElement(const char* str, size_t sLen)
    {
        setString(str, sLen);
    }

    ProgramValue toValue(Program* program) const
    {
        ProgramValue out;
        switch (type) {
        case ArrayElementType::INT:
            out.opcode = VALUE_TYPE_INT;
            out.integerValue = value.integerValue;
            break;
        case ArrayElementType::FLOAT:
            out.opcode = VALUE_TYPE_FLOAT;
            out.floatValue = value.floatValue;
            break;
        case ArrayElementType::POINTER:
            out.opcode = VALUE_TYPE_PTR;
            out.pointerValue = value.pointerValue;
            break;
        case ArrayElementType::STRING:
            out.opcode = VALUE_TYPE_DYNAMIC_STRING;
            out.integerValue = programPushString(program, value.stringValue);
            break;
        }
        return out;
    }

    bool operator<(ArrayElement const& other) const
    {
        if (type != other.type) {
            return type < other.type;
        }

        switch (type) {
        case ArrayElementType::INT:
            return value.integerValue < other.value.integerValue;
        case ArrayElementType::FLOAT:
            return value.floatValue < other.value.floatValue;
        case ArrayElementType::POINTER:
            return value.pointerValue < other.value.pointerValue;
        case ArrayElementType::STRING:
            return strcmp(value.stringValue, other.value.stringValue) < 0;
        default:
            return false;
        }
    }

    bool operator==(ArrayElement const& other) const
    {
        if (type != other.type) {
            return false;
        }

        switch (type) {
        case ArrayElementType::INT:
            return value.integerValue == other.value.integerValue;
        case ArrayElementType::FLOAT:
            return value.floatValue == other.value.floatValue;
        case ArrayElementType::POINTER:
            return value.pointerValue == other.value.pointerValue;
        case ArrayElementType::STRING:
            return strcmp(value.stringValue, other.value.stringValue) == 0;
        default:
            return false;
        }
    }

    ~ArrayElement()
    {
        if (type == ArrayElementType::STRING) {
            free(value.stringValue);
        }
    }

private:
    void setString(const char* str, size_t sLen)
    {
        type = ArrayElementType::STRING;

        if (sLen == -1) {
            sLen = strlen(str);
        }

        if (sLen >= ARRAY_MAX_STRING) {
            sLen = ARRAY_MAX_STRING - 1; // memory safety
        }

        value.stringValue = (char*)malloc(sLen + 1);
        memcpy(value.stringValue, str, sLen);
        value.stringValue[sLen] = '\0';
    }

    ArrayElementType type;
    union {
        int integerValue;
        float floatValue;
        char* stringValue;
        void* pointerValue;
    } value;
};

class SFallArray {
public:
    SFallArray(unsigned int flags)
        : flags(flags)
    {
    }
    virtual ~SFallArray() = default;
    virtual ProgramValue GetArrayKey(int index, Program* program) = 0;
    virtual ProgramValue GetArray(const ProgramValue& key, Program* program) = 0;
    virtual void SetArray(const ProgramValue& key, const ProgramValue& val, bool allowUnset, Program* program) = 0;
    virtual void SetArray(const ProgramValue& key, ArrayElement&& val, bool allowUnset) = 0;
    virtual void ResizeArray(int newLen) = 0;
    virtual ProgramValue ScanArray(const ProgramValue& value, Program* program) = 0;
    virtual int size() = 0;

    bool isReadOnly() const
    {
        return (flags & SFALL_ARRAYFLAG_CONSTVAL) != 0;
    }

protected:
    unsigned int flags;
};

class SFallArrayList : public SFallArray {
public:
    SFallArrayList() = delete;

    SFallArrayList(unsigned int len, unsigned int flags)
        : SFallArray(flags)
    {
        values.resize(len);
    }

    int size()
    {
        return static_cast<int>(values.size());
    }

    ProgramValue GetArrayKey(int index, Program* program)
    {
        if (index < -1 || index > size()) {
            return ProgramValue(0);
        }

        if (index == -1) { // special index to indicate if array is associative
            return ProgramValue(0);
        }

        return ProgramValue(index);
    }

    ProgramValue GetArray(const ProgramValue& key, Program* program)
    {
        auto index = key.asInt();
        if (index < 0 || index >= size()) {
            return ProgramValue(0);
        }

        return values[index].toValue(program);
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
        }

        if (newLen == 0) {
            values.clear();
        } else if (newLen > 0) {
            if (newLen > ARRAY_MAX_SIZE) {
                newLen = ARRAY_MAX_SIZE; // safety
            }

            values.resize(newLen);
        } else if (newLen >= ARRAY_ACTION_SHUFFLE) {
            ListSort(values, newLen, std::less<ArrayElement>());
        }
    }

    ProgramValue ScanArray(const ProgramValue& value, Program* program)
    {
        auto element = ArrayElement { value, program };
        for (int i = 0; i < size(); i++) {
            if (element == values[i]) {
                return ProgramValue(i);
            }
        }
        return ProgramValue(-1);
    }

private:
    std::vector<ArrayElement> values;
};

class SFallArrayAssoc : public SFallArray {
public:
    SFallArrayAssoc() = delete;

    SFallArrayAssoc(unsigned int flags)
        : SFallArray(flags)
    {
    }

    int size()
    {
        return static_cast<int>(pairs.size());
    }

    ProgramValue GetArrayKey(int index, Program* program)
    {
        if (index < -1 || index > size()) {
            return ProgramValue(0);
        }

        if (index == -1) { // special index to indicate if array is associative
            return ProgramValue(1);
        }

        return pairs[index].key.toValue(program);
    }

    ProgramValue GetArray(const ProgramValue& key, Program* program)
    {
        auto keyEl = ArrayElement { key, program };
        auto it = std::find_if(pairs.begin(), pairs.end(), [&keyEl](const KeyValuePair& pair) -> bool {
            return pair.key == keyEl;
        });

        if (it == pairs.end()) {
            return ProgramValue(0);
        }

        auto index = it - pairs.begin();
        return pairs[index].value.toValue(program);
    }

    void SetArray(const ProgramValue& key, const ProgramValue& val, bool allowUnset, Program* program)
    {
        auto keyEl = ArrayElement { key, program };
        auto it = std::find_if(pairs.begin(), pairs.end(), [&keyEl](const KeyValuePair& pair) -> bool {
            return pair.key == keyEl;
        });

        if (it != pairs.end() && isReadOnly()) {
            // don't update value of key
            return;
        }

        if (allowUnset && !isReadOnly() && val.isInt() && val.asInt() == 0) {
            // after assigning zero to a key, no need to store it, because "get_array" returns 0 for non-existent keys: try unset
            if (it != pairs.end()) {
                pairs.erase(it);
            }
        } else {
            if (it == pairs.end()) {
                // size check
                if (size() >= ARRAY_MAX_SIZE) {
                    return;
                }

                pairs.push_back(KeyValuePair { std::move(keyEl), ArrayElement { val, program } });
            } else {
                auto index = it - pairs.begin();
                pairs[index].value = ArrayElement { val, program };
            }
        }
    }

    void SetArray(const ProgramValue& key, ArrayElement&& val, bool allowUnset)
    {
        assert(false && "This method is not used for associative arrays thus it is not implemented");
    }

    void ResizeArray(int newLen)
    {
        if (newLen == -1 || size() == newLen) {
            return;
        }

        // only allow to reduce number of elements (adding range of elements is meaningless for maps)
        if (newLen >= 0 && newLen < size()) {
            pairs.resize(newLen);
        } else if (newLen < 0) {
            if (newLen < (ARRAY_ACTION_SHUFFLE - 2)) return;
            MapSort(newLen);
        }
    }

    ProgramValue ScanArray(const ProgramValue& value, Program* program)
    {
        auto valueEl = ArrayElement { value, program };
        auto it = std::find_if(pairs.begin(), pairs.end(), [&valueEl](const KeyValuePair& pair) {
            return pair.value == valueEl;
        });

        if (it == pairs.end()) {
            return ProgramValue(-1);
        }

        return it->key.toValue(program);
    }

private:
    struct KeyValuePair {
        ArrayElement key;
        ArrayElement value;
    };

    void MapSort(int type)
    {
        bool sortByValue = false;
        if (type < ARRAY_ACTION_SHUFFLE) {
            type += 4;
            sortByValue = true;
        }

        if (sortByValue) {
            ListSort(pairs, type, [](const KeyValuePair& a, const KeyValuePair& b) -> bool {
                return a.value < b.value;
            });
        } else {
            ListSort(pairs, type, [](const KeyValuePair& a, const KeyValuePair& b) -> bool {
                return a.key < b.key;
            });
        }
    }

    std::vector<KeyValuePair> pairs;
};

struct SfallArraysState {
    std::unordered_map<ArrayId, std::unique_ptr<SFallArray>> arrays;
    std::unordered_set<ArrayId> temporaryArrayIds;
    int nextArrayId = kInitialArrayId;
};

static SfallArraysState* _state = nullptr;

bool sfallArraysInit()
{
    _state = new (std::nothrow) SfallArraysState();
    if (_state == nullptr) {
        return false;
    }

    return true;
}

void sfallArraysReset()
{
    if (_state != nullptr) {
        _state->arrays.clear();
        _state->temporaryArrayIds.clear();
        _state->nextArrayId = kInitialArrayId;
    }
}

void sfallArraysExit()
{
    if (_state != nullptr) {
        delete _state;
    }
}

ArrayId CreateArray(int len, unsigned int flags)
{
    flags = (flags & ~1); // reset 1 bit

    if (len < 0) {
        flags |= SFALL_ARRAYFLAG_ASSOC;
    } else if (len > ARRAY_MAX_SIZE) {
        len = ARRAY_MAX_SIZE; // safecheck
    }

    ArrayId arrayId = _state->nextArrayId++;

    if (flags & SFALL_ARRAYFLAG_ASSOC) {
        _state->arrays.emplace(std::make_pair(arrayId, std::make_unique<SFallArrayAssoc>(flags)));
    } else {
        _state->arrays.emplace(std::make_pair(arrayId, std::make_unique<SFallArrayList>(len, flags)));
    }

    return arrayId;
}

ArrayId CreateTempArray(int len, unsigned int flags)
{
    ArrayId arrayId = CreateArray(len, flags);
    _state->temporaryArrayIds.insert(arrayId);
    return arrayId;
}

SFallArray* get_array_by_id(ArrayId arrayId)
{
    auto it = _state->arrays.find(arrayId);
    if (it == _state->arrays.end()) {
        return nullptr;
    }

    return it->second.get();
}

ProgramValue GetArrayKey(ArrayId arrayId, int index, Program* program)
{
    auto arr = get_array_by_id(arrayId);
    if (arr == nullptr) {
        return ProgramValue(0);
    }

    return arr->GetArrayKey(index, program);
}

int LenArray(ArrayId arrayId)
{
    auto arr = get_array_by_id(arrayId);
    if (arr == nullptr) {
        return -1;
    }

    return arr->size();
}

ProgramValue GetArray(ArrayId arrayId, const ProgramValue& key, Program* program)
{
    auto arr = get_array_by_id(arrayId);
    if (arr == nullptr) {
        return ProgramValue(0);
    }

    return arr->GetArray(key, program);
}

void SetArray(ArrayId arrayId, const ProgramValue& key, const ProgramValue& val, bool allowUnset, Program* program)
{
    auto arr = get_array_by_id(arrayId);
    if (arr == nullptr) {
        return;
    }

    arr->SetArray(key, val, allowUnset, program);
}

void FreeArray(ArrayId arrayId)
{
    // TODO: remove from saved_arrays
    _state->arrays.erase(arrayId);
}

void FixArray(ArrayId arrayId)
{
    _state->temporaryArrayIds.erase(arrayId);
}

void DeleteAllTempArrays()
{
    for (auto it = _state->temporaryArrayIds.begin(); it != _state->temporaryArrayIds.end(); ++it) {
        FreeArray(*it);
    }
    _state->temporaryArrayIds.clear();
}

void ResizeArray(ArrayId arrayId, int newLen)
{
    auto arr = get_array_by_id(arrayId);
    if (arr == nullptr) {
        return;
    }

    arr->ResizeArray(newLen);
}

int StackArray(const ProgramValue& key, const ProgramValue& val, Program* program)
{
    // CE: Sfall uses eponymous global variable which is always the id of the
    // last created array.
    ArrayId stackArrayId = _state->nextArrayId - 1;

    auto arr = get_array_by_id(stackArrayId);
    if (arr == nullptr) {
        return 0;
    }

    auto size = arr->size();
    if (size >= ARRAY_MAX_SIZE) {
        return 0;
    }

    if (key.asInt() >= size) {
        arr->ResizeArray(size + 1);
    }

    SetArray(stackArrayId, key, val, false, program);
    return 0;
}

ProgramValue ScanArray(ArrayId arrayId, const ProgramValue& val, Program* program)
{
    auto arr = get_array_by_id(arrayId);
    if (arr == nullptr) {
        return ProgramValue(-1);
    }

    return arr->ScanArray(val, program);
}

ArrayId ListAsArray(int type)
{
    std::vector<Object*> objects;
    sfall_lists_fill(type, objects);

    int count = static_cast<int>(objects.size());
    ArrayId arrayId = CreateTempArray(count, 0);
    auto arr = get_array_by_id(arrayId);

    // A little bit ugly and likely inefficient.
    for (int index = 0; index < count; index++) {
        arr->SetArray(ProgramValue { index },
            ArrayElement { ProgramValue { objects[index] }, nullptr },
            false);
    }

    return arrayId;
}

ArrayId StringSplit(const char* str, const char* split)
{
    size_t splitLen = strlen(split);

    ArrayId arrayId = CreateTempArray(0, 0);
    auto arr = get_array_by_id(arrayId);

    if (splitLen == 0) {
        int count = static_cast<int>(strlen(str));

        arr->ResizeArray(count);
        for (int i = 0; i < count; i++) {
            arr->SetArray(ProgramValue { i }, ArrayElement { &str[i], 1 }, false);
        }
    } else {
        int count = 1;
        const char* ptr = str;
        while (true) {
            const char* newptr = strstr(ptr, split);
            if (newptr == nullptr) {
                break;
            }

            count++;
            ptr = newptr + splitLen;
        }
        arr->ResizeArray(count);

        count = 0;
        ptr = str;
        while (true) {
            const char* newptr = strstr(ptr, split);
            size_t len = (newptr != nullptr) ? newptr - ptr : strlen(ptr);

            arr->SetArray(ProgramValue { count }, ArrayElement { ptr, len }, false);

            if (newptr == nullptr) {
                break;
            }

            count++;
            ptr = newptr + splitLen;
        }
    }
    return arrayId;
}

} // namespace fallout
