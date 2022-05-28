#ifndef FONT_MANAGER_H
#define FONT_MANAGER_H

#include "text_font.h"

// The maximum number of interface fonts.
#define INTERFACE_FONT_MAX (16)

typedef struct InterfaceFontGlyph {
    short width;
    short field_2;
    int field_4;
} InterfaceFontGlyph;

typedef struct InterfaceFontDescriptor {
    short field_0;
    short letterSpacing;
    short wordSpacing;
    short field_6;
    short field_8;
    short field_A;
    InterfaceFontGlyph glyphs[256];
    unsigned char* data;
} InterfaceFontDescriptor;

extern bool gInterfaceFontsInitialized;
extern int gInterfaceFontsLength;
extern FontManager gModernFontManager;

extern InterfaceFontDescriptor gInterfaceFontDescriptors[INTERFACE_FONT_MAX];
extern int gCurrentInterfaceFont;
extern InterfaceFontDescriptor* gCurrentInterfaceFontDescriptor;

int interfaceFontsInit();
void interfaceFontsExit();
int interfaceFontLoad(int font);
void interfaceFontSetCurrentImpl(int font);
int interfaceFontGetLineHeightImpl();
int interfaceFontGetStringWidthImpl(const char* string);
int interfaceFontGetCharacterWidthImpl(int ch);
int interfaceFontGetMonospacedStringWidthImpl(const char* string);
int interfaceFontGetLetterSpacingImpl();
int interfaceFontGetBufferSizeImpl(const char* string);
int interfaceFontGetMonospacedCharacterWidthImpl();
void interfaceFontDrawImpl(unsigned char* buf, const char* string, int length, int pitch, int color);

#endif /* FONT_MANAGER_H */
