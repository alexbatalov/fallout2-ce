#ifndef WINDOW_MANAGER_PRIVATE_H
#define WINDOW_MANAGER_PRIVATE_H

typedef struct struc_177 struc_177;

extern char gProgramWindowTitle[256];

int _win_debug(char* a1);
int _GNW_process_menu(struc_177* ptr, int i);
void _GNW_intr_init();
void _GNW_intr_exit();

#endif /* WINDOW_MANAGER_PRIVATE_H */
