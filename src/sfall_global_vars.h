#ifndef FALLOUT_SFALL_GLOBAL_VARS_H_
#define FALLOUT_SFALL_GLOBAL_VARS_H_

namespace fallout {

bool sfallGlobalVarsInit();
void sfallGlobalVarsReset();
void sfallGlobalVarsExit();
bool sfallGlobalVarsStore(const char* key, int value);
bool sfallGlobalVarsStore(int key, int value);
bool sfallGlobalVarsFetch(const char* key, int& value);
bool sfallGlobalVarsFetch(int key, int& value);

} // namespace fallout

#endif /* FALLOUT_SFALL_GLOBAL_VARS_H_ */
