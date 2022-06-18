#ifndef DISPLAY_MONITOR_H
#define DISPLAY_MONITOR_H

int displayMonitorInit();
int displayMonitorReset();
void displayMonitorExit();
void displayMonitorAddMessage(char* string);
void displayMonitorDisable();
void displayMonitorEnable();

#endif /* DISPLAY_MONITOR_H */
