#ifndef CREDITS_H
#define CREDITS_H

#include "db.h"

#define CREDITS_WINDOW_WIDTH (640)
#define CREDITS_WINDOW_HEIGHT (480)
#define CREDITS_WINDOW_SCROLLING_DELAY (38)

extern File* gCreditsFile;
extern int gCreditsWindowNameColor;
extern int gCreditsWindowTitleFont;
extern int gCreditsWindowNameFont;
extern int gCreditsWindowTitleColor;

void creditsOpen(const char* path, int fid, bool useReversedStyle);
bool creditsFileParseNextLine(char* dest, int* font, int* color);

#endif /* CREDITS_H */
