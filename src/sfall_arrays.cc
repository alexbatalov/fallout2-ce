#include "sfall_arrays.h"
#include "interpreter.h"
#include "sfall_script_value.h"

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace fallout {

static ArrayId nextArrayID = 1;
static ArrayId stackArrayId = 1;

#define ARRAY_MAX_STRING (255) // maximum length of string to be stored as array key or value
#define ARRAY_MAX_SIZE (100000) // maximum number of array elements,

class SFallArray {
protected:
    uint32_t mFlags;

public:
    virtual int size() = 0;
    virtual ProgramValue GetArrayKey(int index) = 0;
    virtual ProgramValue GetArray(const SFallScriptValue& key) = 0;
    virtual void SetArray(const SFallScriptValue& key, const SFallScriptValue& val, bool allowUnset) = 0;
};

class SFallArrayList : public SFallArray {
private:
public:
    SFallArrayList() = delete;

    SFallArrayList(unsigned int len, uint32_t flags)
    {
        values.resize(len);
        mFlags = flags;
    }

    // TODO: SFall copies strings
    std::vector<SFallScriptValue> values;

    int size()
    {
        return values.size();
    }

    ProgramValue GetArrayKey(int index)
    {
        if (index < -1 || index > size()) {
            return SFallScriptValue(0);
        };
        if (index == -1) { // special index to indicate if array is associative
            throw(std::invalid_argument("Not implemented yet"));
        };
        // TODO: assoc
        return SFallScriptValue(index);
    }

    ProgramValue GetArray(const SFallScriptValue& key)
    {
        // TODO assoc

        auto element_index = key.asInt();
        if (element_index < 0 || element_index >= size()) {
            return SFallScriptValue(0);
        };
        return values[element_index];
    }

    void SetArray(const SFallScriptValue& key, const SFallScriptValue& val, bool allowUnset)
    {

        // TODO: assoc

        if (key.isInt()) {
            auto index = key.asInt();
            if (index >= 0 && index < size()) {
                values[index] = key;
            }
        }
    }
};

using ArraysMap = std::unordered_map<ArrayId, std::unique_ptr<SFallArray>>;

ArraysMap arrays;
std::unordered_set<ArrayId> temporaryArrays;

ArrayId CreateArray(int len, uint32_t flags)
{
    flags = (flags & ~1); // reset 1 bit

    if (len < 0) {
        flags |= SFALL_ARRAYFLAG_ASSOC;
        // TODO: Implement
        throw(std::invalid_argument("Not implemented yet"));
    };

    if (len > ARRAY_MAX_SIZE) {
        len = ARRAY_MAX_SIZE; // safecheck
    }

    ArrayId array_id = nextArrayID++;

    stackArrayId = array_id;

    arrays.emplace(std::make_pair(array_id, std::make_unique<SFallArrayList>(len, flags)));

    return array_id;
}

ArrayId CreateTempArray(int len, uint32_t flags)
{
    ArrayId array_id = CreateArray(len, flags);
    temporaryArrays.insert(array_id);
    return array_id;
}

ProgramValue GetArrayKey(ArrayId array_id, int index)
{
    auto& arr = arrays[array_id];
    if (!arr) {
        return SFallScriptValue(0);
    };
    return arr->GetArrayKey(index);
}

int LenArray(ArrayId array_id)
{
    auto& arr = arrays[array_id];
    if (!arr) {
        return -1;
    };

    // TODO: assoc

    return arr->size();
}

ProgramValue GetArray(ArrayId array_id, const SFallScriptValue& key)
{
    // TODO: If type is string then do substr

    auto& arr = arrays[array_id];

    if (!arr) {
        return SFallScriptValue(0);
    };

    return arr->GetArray(key);
}

void SetArray(ArrayId array_id, const SFallScriptValue& key, const SFallScriptValue& val, bool allowUnset)
{
    auto& arr = arrays[array_id];
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

}