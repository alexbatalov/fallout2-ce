#ifndef FALLOUT_PREFERENCES_H_
#define FALLOUT_PREFERENCES_H_

#include "db.h"

namespace fallout {

int preferencesInit();
int doPreferences(bool animated);
int preferencesSave(File* stream);
int preferencesLoad(File* stream);
void brightnessIncrease();
void brightnessDecrease();

} // namespace fallout

#endif /* FALLOUT_PREFERENCES_H_ */
