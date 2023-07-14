#ifndef FREETYPE_MANAGER_H
#define FREETYPE_MANAGER_H

#include "text_font.h"

namespace fallout {

extern FontManager gFtFontManager;

int FtFontsInit();
void FtFontsExit();

} // namespace fallout

#endif /* FONT_MANAGER_H */
