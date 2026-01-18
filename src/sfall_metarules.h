#ifndef FALLOUT_SFALL_METARULES_H_
#define FALLOUT_SFALL_METARULES_H_

#include "interpreter.h"

namespace fallout {

typedef void(MetaruleHandler)(Program* program, int args);

// Simplified cousin of `SfallMetarule` from Sfall.
typedef struct MetaruleInfo {
    const char* name;
    MetaruleHandler* handler;
    int minArgs;
    int maxArgs;
} MetaruleInfo;

extern const MetaruleInfo kMetarules[];
extern const std::size_t kMetarulesCount;

void sfall_metarule(Program* program, int args);

void sprintf_lite(Program* program, int args, const char* infoOpcodeName);

} // namespace fallout

#endif /* FALLOUT_SFALL_METARULES_H_ */
