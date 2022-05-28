#ifndef TEXT_FONT_H
#define TEXT_FONT_H

// The maximum number of text fonts.
#define TEXT_FONT_MAX (10)

// The maximum number of font managers.
#define FONT_MANAGER_MAX (10)

typedef void FontManagerSetCurrentFontProc(int font);
typedef void FontManagerDrawTextProc(unsigned char* buffer, const char* string, int length, int pitch, int color);
typedef int FontManagerGetLineHeightProc();
typedef int FontManagerGetStringWidthProc(const char* string);
typedef int FontManagerGetCharacterWidthProc(int ch);
typedef int FontManagerGetMonospacedStringWidthProc(const char* string);
typedef int FontManagerGetLetterSpacingProc();
typedef int FontManagerGetBufferSizeProc(const char* string);
typedef int FontManagerGetMonospacedCharacterWidth();

typedef struct FontManager {
    int minFont;
    int maxFont;
    FontManagerSetCurrentFontProc* setCurrentProc;
    FontManagerDrawTextProc* drawTextProc;
    FontManagerGetLineHeightProc* getLineHeightProc;
    FontManagerGetStringWidthProc* getStringWidthProc;
    FontManagerGetCharacterWidthProc* getCharacterWidthProc;
    FontManagerGetMonospacedStringWidthProc* getMonospacedStringWidthProc;
    FontManagerGetLetterSpacingProc* getLetterSpacingProc;
    FontManagerGetBufferSizeProc* getBufferSizeProc;
    FontManagerGetMonospacedCharacterWidth* getMonospacedCharacterWidthProc;
} FontManager;

typedef struct TextFontGlyph {
    // The width of the glyph in pixels.
    int width;

    // Data offset into [TextFont.data].
    int dataOffset;
} TextFontGlyph;

typedef struct TextFontDescriptor {
    // The number of glyphs in the font.
    int glyphCount;

    // The height of the font.
    int lineHeight;

    // Horizontal spacing between characters in pixels.
    int letterSpacing;

    TextFontGlyph* glyphs;
    unsigned char* data;
} TextFontDescriptor;

#define FONT_SHADOW (0x10000)
#define FONT_UNDERLINE (0x20000)
#define FONT_MONO (0x40000)

extern FontManager gTextFontManager;
extern int gCurrentFont;
extern int gFontManagersCount;
extern FontManagerDrawTextProc* fontDrawText;
extern FontManagerGetLineHeightProc* fontGetLineHeight;
extern FontManagerGetStringWidthProc* fontGetStringWidth;
extern FontManagerGetCharacterWidthProc* fontGetCharacterWidth;
extern FontManagerGetMonospacedStringWidthProc* fontGetMonospacedStringWidth;
extern FontManagerGetLetterSpacingProc* fontGetLetterSpacing;
extern FontManagerGetBufferSizeProc* fontGetBufferSize;
extern FontManagerGetMonospacedCharacterWidth* fontGetMonospacedCharacterWidth;

extern TextFontDescriptor gTextFontDescriptors[TEXT_FONT_MAX];
extern FontManager gFontManagers[FONT_MANAGER_MAX];
extern TextFontDescriptor* gCurrentTextFontDescriptor;

int textFontsInit();
void textFontsExit();
int textFontLoad(int font);
int fontManagerAdd(FontManager* fontManager);
void textFontSetCurrentImpl(int font);
int fontGetCurrent();
void fontSetCurrent(int font);
bool fontManagerFind(int font, FontManager** fontManagerPtr);
void textFontDrawImpl(unsigned char* buf, const char* string, int length, int pitch, int color);
int textFontGetLineHeightImpl();
int textFontGeStringWidthImpl(const char* string);
int textFontGetCharacterWidthImpl(int ch);
int textFontGetMonospacedStringWidthImpl(const char* string);
int textFontGetLetterSpacingImpl();
int textFontGetBufferSizeImpl(const char* string);
int textFontGetMonospacedCharacterWidthImpl();

#endif /* TEXT_FONT_H */
