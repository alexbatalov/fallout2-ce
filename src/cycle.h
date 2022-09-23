#ifndef CYCLE_H
#define CYCLE_H

namespace fallout {

void colorCycleInit();
void colorCycleReset();
void colorCycleFree();
void colorCycleDisable();
void colorCycleEnable();
bool colorCycleEnabled();
void cycleSetSpeedFactor(int value);
void colorCycleTicker();

} // namespace fallout

#endif /* CYCLE_H */
