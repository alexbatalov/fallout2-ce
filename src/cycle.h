#ifndef CYCLE_H
#define CYCLE_H

void colorCycleInit();
void colorCycleReset();
void colorCycleFree();
void colorCycleDisable();
void colorCycleEnable();
bool colorCycleEnabled();
void cycleSetSpeedFactor(int value);
void colorCycleTicker();

#endif /* CYCLE_H */
