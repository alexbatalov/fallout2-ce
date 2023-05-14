#ifndef SFALL_ARRAYS
#define SFALL_ARRAYS

#include "interpreter.h"
#include "object.h"
#include <cstdint>

namespace fallout {

#define SFALL_ARRAYFLAG_ASSOC (1) // is map
#define SFALL_ARRAYFLAG_CONSTVAL (2) // don't update value of key if the key exists in map
#define SFALL_ARRAYFLAG_RESERVED (4)

using ArrayId = unsigned int;

ArrayId CreateArray(int len, uint32_t flags);
ArrayId CreateTempArray(int len, uint32_t flags);
ProgramValue GetArrayKey(ArrayId array_id, int index, Program* program);
int LenArray(ArrayId array_id);
ProgramValue GetArray(ArrayId array_id, const ProgramValue& key, Program* program);
void SetArray(ArrayId array_id, const ProgramValue& key, const ProgramValue& val, bool allowUnset, Program* program);
void FreeArray(ArrayId array_id);
void FixArray(ArrayId id);
void ResizeArray(ArrayId array_id, int newLen);
void DeleteAllTempArrays();
void sfallArraysReset();
int StackArray(const ProgramValue& key, const ProgramValue& val, Program* program);
ProgramValue ScanArray(ArrayId array_id, const ProgramValue& val, Program* program);

}
#endif /* SFALL_ARRAYS */