#include "interpreter.h"
#include <cstdint>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace fallout {

using ArrayId = unsigned int;

static ArrayId nextArrayID = 1;
static ArrayId stackArrayId = 1;

class SFallScriptValue : public ProgramValue {
public:
    SFallScriptValue()
    {
        opcode = VALUE_TYPE_INT;
        integerValue = 0;
    }
    SFallScriptValue(int value)
    {
        opcode = VALUE_TYPE_INT;
        integerValue = value;
    }
    SFallScriptValue(ProgramValue& value)
    {
        // Assuming that pointer is the biggest in size
        static_assert(sizeof(decltype(value.floatValue)) <= sizeof(decltype(value.pointerValue)));
        static_assert(sizeof(decltype(value.integerValue)) <= sizeof(decltype(value.pointerValue)));
        opcode = value.opcode;
        pointerValue = value.pointerValue;
    }

    bool isInt() const
    {
        return opcode == VALUE_TYPE_INT;
    }
    bool isFloat() const
    {
        return opcode == VALUE_TYPE_FLOAT;
    }
    bool isPointer() const
    {
        return opcode == VALUE_TYPE_PTR;
    }

    int asInt() const
    {
        switch (opcode) {
        case VALUE_TYPE_INT:
            return integerValue;
        case VALUE_TYPE_FLOAT:
            return static_cast<int>(floatValue);
        default:
            return 0;
        }
    }
};

#define ARRAYFLAG_ASSOC (1) // is map
#define ARRAYFLAG_CONSTVAL (2) // don't update value of key if the key exists in map
#define ARRAYFLAG_RESERVED (4)

#define ARRAY_MAX_STRING (255) // maximum length of string to be stored as array key or value
#define ARRAY_MAX_SIZE (100000) // maximum number of array elements,

class SFallArray {
private:
public:
    uint32_t flags;

    SFallArray() = delete;

    SFallArray(unsigned int len, uint32_t flags)
        : flags(flags)
    {
        data.resize(len);
    }
    std::vector<SFallScriptValue> data;
    int size()
    {
        return data.size();
    }
};

using ArraysMap = std::unordered_map<ArrayId, SFallArray>;

ArraysMap arrays;
std::unordered_set<ArrayId> temporaryArrays;

ArrayId CreateArray(int len, uint32_t flags)
{
    flags = (flags & ~1); // reset 1 bit

    if (len < 0) {
        flags |= ARRAYFLAG_ASSOC;
        throw(std::invalid_argument("Not implemented yet"));
    };

    if (len > ARRAY_MAX_SIZE) {
        len = ARRAY_MAX_SIZE; // safecheck
    }

    ArrayId array_id = nextArrayID++;

    stackArrayId = array_id;

    arrays.emplace(std::make_pair(array_id, SFallArray { len, flags }));

    return array_id;
}

ArrayId CreateTempArray(int len, uint32_t flags)
{
    ArrayId array_id = CreateArray(len, flags);
    temporaryArrays.insert(array_id);
    return array_id;
}

static SFallArray* get_array_by_id(ArrayId array_id)
{
    auto iter = arrays.find(array_id);
    if (iter == arrays.end()) {
        return nullptr;
    };
    return &iter->second;
}

ProgramValue GetArrayKey(ArrayId array_id, int index)
{
    auto arr = get_array_by_id(array_id);
    if (arr == nullptr || index < -1 || index > arr->size()) {
        return SFallScriptValue(0);
    };
    if (index == -1) { // special index to indicate if array is associative
        throw(std::invalid_argument("Not implemented yet"));
    };
    // TODO: if assoc
    return SFallScriptValue(index);
}

int LenArray(ArrayId array_id)
{
    auto arr = get_array_by_id(array_id);
    if (arr == nullptr) {
        return -1;
    };
    return arr->size();
}

ProgramValue GetArray(ArrayId array_id, ProgramValue key)
{
    auto arr = get_array_by_id(array_id);
    if (arr == nullptr) {
        return SFallScriptValue(0);
    };

    auto skey = SFallScriptValue(key);
    // TODO assoc

    auto element_index = skey.asInt();
    if (element_index < 0 || element_index >= arr->size()) {
        return SFallScriptValue(0);
    };
    return arr->data[element_index];
}
}