#ifndef DISPLAY_MONITOR_H
#define DISPLAY_MONITOR_H

#include "geometry.h"

// The maximum number of lines display monitor can hold. Once this value
// is reached earlier messages are thrown away.
#define DISPLAY_MONITOR_LINES_CAPACITY (100)

// The maximum length of a string in display monitor (in characters).
#define DISPLAY_MONITOR_LINE_LENGTH (80)

#define DISPLAY_MONITOR_X (23)
#define DISPLAY_MONITOR_Y (24)
#define DISPLAY_MONITOR_WIDTH (167)
#define DISPLAY_MONITOR_HEIGHT (60)

#define DISPLAY_MONITOR_HALF_HEIGHT (DISPLAY_MONITOR_HEIGHT / 2)

#define DISPLAY_MONITOR_FONT (101)

#define DISPLAY_MONITOR_BEEP_DELAY (500U)

extern bool gDisplayMonitorInitialized;
extern const Rect gDisplayMonitorRect;
extern int gDisplayMonitorScrollDownButton;
extern int gDisplayMonitorScrollUpButton;

extern char gDisplayMonitorLines[DISPLAY_MONITOR_LINES_CAPACITY][DISPLAY_MONITOR_LINE_LENGTH];
extern unsigned char* gDisplayMonitorBackgroundFrmData;
extern int _max_disp;
extern bool gDisplayMonitorEnabled;
extern int _disp_curr;
extern int _intface_full_width;
extern int gDisplayMonitorLinesCapacity;
extern int _disp_start;
extern unsigned int gDisplayMonitorLastBeepTimestamp;

int displayMonitorInit();
int displayMonitorReset();
void displayMonitorExit();
void displayMonitorAddMessage(char* string);
void displayMonitorRefresh();
void displayMonitorScrollUpOnMouseDown(int btn, int keyCode);
void displayMonitorScrollDownOnMouseDown(int btn, int keyCode);
void displayMonitorScrollUpOnMouseEnter(int btn, int keyCode);
void displayMonitorScrollDownOnMouseEnter(int btn, int keyCode);
void displayMonitorOnMouseExit(int btn, int keyCode);
void displayMonitorDisable();
void displayMonitorEnable();

#endif /* DISPLAY_MONITOR_H */
