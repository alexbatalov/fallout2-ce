#ifndef FALLOUT_SFALL_ARRAYS_H_
#define FALLOUT_SFALL_ARRAYS_H_

#include "interpreter.h"

namespace fallout {

#define SFALL_ARRAYFLAG_ASSOC (1) // is map
#define SFALL_ARRAYFLAG_CONSTVAL (2) // don't update value of key if the key exists in map
#define SFALL_ARRAYFLAG_RESERVED (4)

using ArrayId = unsigned int;

bool sfallArraysInit();
void sfallArraysReset();
void sfallArraysExit();
ArrayId CreateArray(int len, unsigned int flags);
ArrayId CreateTempArray(int len, unsigned int flags);
ProgramValue GetArrayKey(ArrayId arrayId, int index, Program* program);
int LenArray(ArrayId arrayId);
ProgramValue GetArray(ArrayId arrayId, const ProgramValue& key, Program* program);
void SetArray(ArrayId arrayId, const ProgramValue& key, const ProgramValue& val, bool allowUnset, Program* program);
void FreeArray(ArrayId arrayId);
void FixArray(ArrayId id);
void ResizeArray(ArrayId arrayId, int newLen);
void DeleteAllTempArrays();
int StackArray(const ProgramValue& key, const ProgramValue& val, Program* program);
ProgramValue ScanArray(ArrayId arrayId, const ProgramValue& val, Program* program);
ArrayId ListAsArray(int type);

ArrayId StringSplit(const char* str, const char* split);

} // namespace fallout

#endif /* FALLOUT_SFALL_ARRAYS_H_ */
