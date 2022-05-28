#ifndef WINDOW_MANAGER_PRIVATE_H
#define WINDOW_MANAGER_PRIVATE_H

typedef struct struc_177 struc_177;

typedef struct STRUCT_6B2340 {
    int field_0;
    int field_4;
} STRUCT_6B2340;

typedef struct STRUCT_6B2370 {
    int field_0;
    // win
    int field_4;
    int field_8;
} STRUCT_6B2370;

extern int _wd;
extern int _curr_menu;
extern bool _tm_watch_active;

extern STRUCT_6B2340 _tm_location[5];
extern int _tm_text_x;
extern int _tm_h;
extern STRUCT_6B2370 _tm_queue[5];
extern int _tm_persistence;
extern int _scr_center_x;
extern int _tm_text_y;
extern int _tm_kill;
extern int _tm_add;
extern int _curry;
extern int _currx;
extern char gProgramWindowTitle[256];

int _win_debug(char* a1);
void _win_debug_delete();
int _win_register_menu_bar(int win, int x, int y, int width, int height, int a6, int a7);
int _win_register_menu_pulldown(int win, int x, char* str, int a4);
int _win_width_needed(char** fileNameList, int fileNameListLength);
int _GNW_process_menu(struc_177* ptr, int i);
int _calc_max_field_chars_wcursor(int a1, int a2);
void _GNW_intr_init();
void _GNW_intr_exit();
void _tm_watch_msgs();
void _tm_kill_msg();
void _tm_kill_out_of_order(int a1);
void _tm_click_response(int btn);
int _tm_index_active(int a1);

#endif /* WINDOW_MANAGER_PRIVATE_H */
