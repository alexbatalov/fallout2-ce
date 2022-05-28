#ifndef CYCLE_H
#define CYCLE_H

#define COLOR_CYCLE_PERIOD_1 (200U)
#define COLOR_CYCLE_PERIOD_2 (142U)
#define COLOR_CYCLE_PERIOD_3 (100U)
#define COLOR_CYCLE_PERIOD_4 (33U)

extern int gColorCycleSpeedFactor;
extern unsigned char _slime[12];
extern unsigned char _shoreline[18];
extern unsigned char _fire_slow[15];
extern unsigned char _fire_fast[15];
extern unsigned char _monitors[15];
extern bool gColorCycleEnabled;
extern bool gColorCycleInitialized;

extern unsigned int gColorCycleTimestamp3;
extern unsigned int gColorCycleTimestamp1;
extern unsigned int gColorCycleTimestamp2;
extern unsigned int gColorCycleTimestamp4;

void colorCycleInit();
void colorCycleReset();
void colorCycleFree();
void colorCycleDisable();
void colorCycleEnable();
bool colorCycleEnabled();
void cycleSetSpeedFactor(int value);
void colorCycleTicker();

#endif /* CYCLE_H */
