#ifndef WINDOW_MANAGER_PRIVATE_H
#define WINDOW_MANAGER_PRIVATE_H

#include "geometry.h"

typedef struct MenuBar MenuBar;

typedef void(ListSelectionHandler)(char** items, int index);

extern char gProgramWindowTitle[256];

int _win_list_select(const char* title, char** fileList, int fileListLength, ListSelectionHandler* callback, int x, int y, int a7);
int _win_list_select_at(const char* title, char** items, int itemsLength, ListSelectionHandler* callback, int x, int y, int a7, int a8);
int _win_get_str(char* dest, int length, const char* title, int x, int y);
int _win_msg(const char* string, int x, int y, int flags);
int _win_pull_down(char** items, int itemsLength, int x, int y, int a5);
int _create_pull_down(char** stringList, int stringListLength, int x, int y, int a5, int a6, Rect* rect);
int _win_debug(char* string);
void _win_debug_delete(int btn, int keyCode);
int _win_register_menu_bar(int win, int x, int y, int width, int height, int borderColor, int backgroundColor);
int _win_register_menu_pulldown(int win, int x, char* title, int keyCode, int itemsLength, char** items, int a7, int a8);
void _win_delete_menu_bar(int win);
int _find_first_letter(int ch, char** stringList, int stringListLength);
int _win_width_needed(char** fileNameList, int fileNameListLength);
int _win_input_str(int win, char* dest, int maxLength, int x, int y, int textColor, int backgroundColor);
int sub_4DBD04(int win, Rect* rect, char** items, int itemsLength, int a5, int a6, MenuBar* menuBar, int pulldownIndex);
int _GNW_process_menu(MenuBar* menuBar, int pulldownIndex);
int _calc_max_field_chars_wcursor(int a1, int a2);
void _GNW_intr_init();
void _GNW_intr_exit();
void _tm_watch_msgs();
void _tm_kill_msg();
void _tm_kill_out_of_order(int a1);
void _tm_click_response(int btn);
int _tm_index_active(int a1);

#endif /* WINDOW_MANAGER_PRIVATE_H */
