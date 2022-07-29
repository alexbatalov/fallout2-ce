#ifndef WIDGET_H
#define WIDGET_H

int _update_widgets();
int widgetGetFont();
int widgetSetFont(int a1);
int widgetGetTextFlags();
int widgetSetTextFlags(int a1);
unsigned char widgetGetTextColor();
int widgetSetTextColor(float a1, float a2, float a3);
int widgetSetHighlightColor(float a1, float a2, float a3);
void sub_4B5998(int win);

#endif /* WIDGET_H */
