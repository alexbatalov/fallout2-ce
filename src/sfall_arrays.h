#ifndef SFALL_ARRAYS
#define SFALL_ARRAYS

#include "interpreter.h"
#include "object.h"
#include "sfall_script_value.h"
#include <cstdint>

namespace fallout {

#define SFALL_ARRAYFLAG_ASSOC (1) // is map
#define SFALL_ARRAYFLAG_CONSTVAL (2) // don't update value of key if the key exists in map
#define SFALL_ARRAYFLAG_RESERVED (4)

using ArrayId = unsigned int;

ArrayId CreateArray(int len, uint32_t flags);
ArrayId CreateTempArray(int len, uint32_t flags);
ProgramValue GetArrayKey(ArrayId array_id, int index);
int LenArray(ArrayId array_id);
ProgramValue GetArray(ArrayId array_id, const SFallScriptValue& key);
void SetArray(ArrayId array_id, const SFallScriptValue& key, const SFallScriptValue& val, bool allowUnset);

}
#endif /* SFALL_ARRAYS */