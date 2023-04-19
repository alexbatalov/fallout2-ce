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

using ArraysMap = std::unordered_map<ArrayId, SFallArray>;

ArraysMap arrays;
std::unordered_set<ArrayId> temporaryArrays;

class SFallArrayElement : public ProgramValue {
public:
    SFallArrayElement()
    {
        opcode = VALUE_TYPE_INT;
        integerValue = 0;
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
    std::vector<SFallArrayElement> data;
};

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

    SFallArray arr { len, flags };

    ArrayId array_id = nextArrayID++;

    stackArrayId = array_id;

    arrays[array_id] = std::move(arr);

    return array_id;
}

ArrayId CreateTempArray(int len, uint32_t flags)
{
    ArrayId array_id = CreateArray(len, flags);
    temporaryArrays.insert(array_id);
    return array_id;
}

}