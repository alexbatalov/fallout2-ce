#include "text_font.h"

#include <stdio.h>
#include <string.h>

#include "color.h"
#include "db.h"
#include "memory.h"
#include "platform_compat.h"
#include "settings.h"

namespace fallout {

// The maximum number of text fonts.
#define TEXT_FONT_MAX (10)

// The maximum number of font managers.
#define FONT_MANAGER_MAX (10)

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

static void textFontSetCurrentImpl(int font);
static bool fontManagerFind(int font, FontManager** fontManagerPtr);
static void textFontDrawImpl(unsigned char* buf, const char* string, int length, int pitch, int color);
static int textFontGetLineHeightImpl();
static int textFontGeStringWidthImpl(const char* string);
static int textFontGetCharacterWidthImpl(int ch);
static int textFontGetMonospacedStringWidthImpl(const char* string);
static int textFontGetLetterSpacingImpl();
static int textFontGetBufferSizeImpl(const char* string);
static int textFontGetMonospacedCharacterWidthImpl();

// 0x4D5530
FontManager gTextFontManager = {
    0,
    9,
    textFontSetCurrentImpl,
    textFontDrawImpl,
    textFontGetLineHeightImpl,
    textFontGeStringWidthImpl,
    textFontGetCharacterWidthImpl,
    textFontGetMonospacedStringWidthImpl,
    textFontGetLetterSpacingImpl,
    textFontGetBufferSizeImpl,
    textFontGetMonospacedCharacterWidthImpl,
};

// 0x51E3B0
int gCurrentFont = -1;

// 0x51E3B4
int gFontManagersCount = 0;

// 0x51E3B8
FontManagerDrawTextProc* fontDrawText = nullptr;

// 0x51E3BC
FontManagerGetLineHeightProc* fontGetLineHeight = nullptr;

// 0x51E3C0
FontManagerGetStringWidthProc* fontGetStringWidth = nullptr;

// 0x51E3C4
FontManagerGetCharacterWidthProc* fontGetCharacterWidth = nullptr;

// 0x51E3C8
FontManagerGetMonospacedStringWidthProc* fontGetMonospacedStringWidth = nullptr;

// 0x51E3CC
FontManagerGetLetterSpacingProc* fontGetLetterSpacing = nullptr;

// 0x51E3D0
FontManagerGetBufferSizeProc* fontGetBufferSize = nullptr;

// 0x51E3D4
FontManagerGetMonospacedCharacterWidth* fontGetMonospacedCharacterWidth = nullptr;

// 0x6ADB08
static TextFontDescriptor gTextFontDescriptors[TEXT_FONT_MAX];

// 0x6ADBD0
static FontManager gFontManagers[FONT_MANAGER_MAX];

// 0x6ADD88
static TextFontDescriptor* gCurrentTextFontDescriptor;

// 0x4D555C
int textFontsInit()
{
    int currentFont = -1;

    FontManager fontManager;
    memcpy(&fontManager, &gTextFontManager, sizeof(fontManager));

    for (int font = 0; font < TEXT_FONT_MAX; font++) {
        if (textFontLoad(font) == -1) {
            gTextFontDescriptors[font].glyphCount = 0;
        } else {
            if (currentFont == -1) {
                currentFont = font;
            }
        }
    }

    if (currentFont == -1) {
        return -1;
    }

    if (fontManagerAdd(&fontManager) == -1) {
        return -1;
    }

    fontSetCurrent(currentFont);

    return 0;
}

// 0x4D55CC
void textFontsExit()
{
    for (int index = 0; index < TEXT_FONT_MAX; index++) {
        TextFontDescriptor* textFontDescriptor = &(gTextFontDescriptors[index]);
        if (textFontDescriptor->glyphCount != 0) {
            internal_free(textFontDescriptor->glyphs);
            internal_free(textFontDescriptor->data);
        }
    }
}

// 0x4D55FC
int textFontLoad(int font)
{
    int rc = -1;

    TextFontDescriptor* textFontDescriptor = &(gTextFontDescriptors[font]);
    textFontDescriptor->data = nullptr;
    textFontDescriptor->glyphs = nullptr;

    File* stream = nullptr;
    char path[COMPAT_MAX_PATH];

    // Try set language path first
    snprintf(path, sizeof(path), "text/%s/font%d.fon", settings.system.language.c_str(), font);
    stream = fileOpen(path, "rb");

    // Fallback to English if needed
    if (stream == nullptr && compat_stricmp(settings.system.language.c_str(), ENGLISH) != 0) {
        snprintf(path, sizeof(path), "text/%s/font%d.fon", ENGLISH, font);
        stream = fileOpen(path, "rb");
    }

    // fallback to original path
    if (stream == nullptr) {
        snprintf(path, sizeof(path), "font%d.fon", font);
        stream = fileOpen(path, "rb");
        if (stream == nullptr) {
            goto out;
        }
    }

    int dataSize;

    // NOTE: Original code reads entire descriptor in one go. This does not work
    // in x64 because of the two pointers.

    if (fileRead(&(textFontDescriptor->glyphCount), 4, 1, stream) != 1) goto out;
    if (fileRead(&(textFontDescriptor->lineHeight), 4, 1, stream) != 1) goto out;
    if (fileRead(&(textFontDescriptor->letterSpacing), 4, 1, stream) != 1) goto out;

    int glyphsPtr;
    if (fileRead(&glyphsPtr, 4, 1, stream) != 1) goto out;

    int dataPtr;
    if (fileRead(&dataPtr, 4, 1, stream) != 1) goto out;

    textFontDescriptor->glyphs = (TextFontGlyph*)internal_malloc(textFontDescriptor->glyphCount * sizeof(TextFontGlyph));
    if (textFontDescriptor->glyphs == nullptr) {
        goto out;
    }

    if (fileRead(textFontDescriptor->glyphs, sizeof(TextFontGlyph), textFontDescriptor->glyphCount, stream) != textFontDescriptor->glyphCount) {
        goto out;
    }

    dataSize = textFontDescriptor->lineHeight * ((textFontDescriptor->glyphs[textFontDescriptor->glyphCount - 1].width + 7) >> 3) + textFontDescriptor->glyphs[textFontDescriptor->glyphCount - 1].dataOffset;
    textFontDescriptor->data = (unsigned char*)internal_malloc(dataSize);
    if (textFontDescriptor->data == nullptr) {
        goto out;
    }

    if (fileRead(textFontDescriptor->data, 1, dataSize, stream) != dataSize) {
        goto out;
    }

    rc = 0;

out:

    if (rc != 0) {
        if (textFontDescriptor->data != nullptr) {
            internal_free(textFontDescriptor->data);
            textFontDescriptor->data = nullptr;
        }

        if (textFontDescriptor->glyphs != nullptr) {
            internal_free(textFontDescriptor->glyphs);
            textFontDescriptor->glyphs = nullptr;
        }
    }

    if (stream != nullptr) {
        fileClose(stream);
    }

    return rc;
}

// 0x4D5780
int fontManagerAdd(FontManager* fontManager)
{
    if (fontManager == nullptr) {
        return -1;
    }

    if (gFontManagersCount >= FONT_MANAGER_MAX) {
        return -1;
    }

    // Check if a font manager exists for any font in the specified range.
    for (int index = fontManager->minFont; index < fontManager->maxFont; index++) {
        FontManager* existingFontManager;
        if (fontManagerFind(index, &existingFontManager)) {
            return -1;
        }
    }

    memcpy(&(gFontManagers[gFontManagersCount]), fontManager, sizeof(*fontManager));
    gFontManagersCount++;

    return 0;
}

// 0x4D58AC
static void textFontSetCurrentImpl(int font)
{
    if (font >= TEXT_FONT_MAX) {
        return;
    }

    TextFontDescriptor* textFontDescriptor = &(gTextFontDescriptors[font]);
    if (textFontDescriptor->glyphCount == 0) {
        return;
    }

    gCurrentTextFontDescriptor = textFontDescriptor;
}

// 0x4D58D4
int fontGetCurrent()
{
    return gCurrentFont;
}

// 0x4D58DC
void fontSetCurrent(int font)
{
    FontManager* fontManager;

    if (fontManagerFind(font, &fontManager)) {
        fontDrawText = fontManager->drawTextProc;
        fontGetLineHeight = fontManager->getLineHeightProc;
        fontGetStringWidth = fontManager->getStringWidthProc;
        fontGetCharacterWidth = fontManager->getCharacterWidthProc;
        fontGetMonospacedStringWidth = fontManager->getMonospacedStringWidthProc;
        fontGetLetterSpacing = fontManager->getLetterSpacingProc;
        fontGetBufferSize = fontManager->getBufferSizeProc;
        fontGetMonospacedCharacterWidth = fontManager->getMonospacedCharacterWidthProc;

        gCurrentFont = font;

        fontManager->setCurrentProc(font);
    }
}

// 0x4D595C
static bool fontManagerFind(int font, FontManager** fontManagerPtr)
{
    for (int index = 0; index < gFontManagersCount; index++) {
        FontManager* fontManager = &(gFontManagers[index]);
        if (font >= fontManager->minFont && font <= fontManager->maxFont) {
            *fontManagerPtr = fontManager;
            return true;
        }
    }

    return false;
}

// 0x4D59B0
static void textFontDrawImpl(unsigned char* buf, const char* string, int length, int pitch, int color)
{
    if ((color & FONT_SHADOW) != 0) {
        color &= ~FONT_SHADOW;
        fontDrawText(buf + pitch + 1, string, length, pitch, _colorTable[0]);
    }

    int monospacedCharacterWidth;
    if ((color & FONT_MONO) != 0) {
        monospacedCharacterWidth = fontGetMonospacedCharacterWidth();
    }

    unsigned char* ptr = buf;
    while (*string != '\0') {
        char ch = *string++;
        if (ch < gCurrentTextFontDescriptor->glyphCount) {
            TextFontGlyph* glyph = &(gCurrentTextFontDescriptor->glyphs[ch & 0xFF]);

            unsigned char* end;
            if ((color & FONT_MONO) != 0) {
                end = ptr + monospacedCharacterWidth;
                ptr += (monospacedCharacterWidth - gCurrentTextFontDescriptor->letterSpacing - glyph->width) / 2;
            } else {
                end = ptr + glyph->width + gCurrentTextFontDescriptor->letterSpacing;
            }

            if (end - buf > length) {
                break;
            }

            unsigned char* glyphData = gCurrentTextFontDescriptor->data + glyph->dataOffset;
            for (int y = 0; y < gCurrentTextFontDescriptor->lineHeight; y++) {
                int bits = 0x80;
                for (int x = 0; x < glyph->width; x++) {
                    if (bits == 0) {
                        bits = 0x80;
                        glyphData++;
                    }

                    if ((*glyphData & bits) != 0) {
                        *ptr = color & 0xFF;
                    }

                    bits >>= 1;
                    ptr++;
                }
                glyphData++;
                ptr += pitch - glyph->width;
            }

            ptr = end;
        }
    }

    if ((color & FONT_UNDERLINE) != 0) {
        // TODO: Probably additional -1 present, check.
        int length = ptr - buf;
        unsigned char* underlinePtr = buf + pitch * (gCurrentTextFontDescriptor->lineHeight - 1);
        for (int pix = 0; pix < length; pix++) {
            *underlinePtr++ = color & 0xFF;
        }
    }
}

// 0x4D5B54
static int textFontGetLineHeightImpl()
{
    return gCurrentTextFontDescriptor->lineHeight;
}

// 0x4D5B60
static int textFontGeStringWidthImpl(const char* string)
{
    int width = 0;

    const char* ch = string;
    while (*ch != '\0') {
        if (*ch < gCurrentTextFontDescriptor->glyphCount) {
            width += gCurrentTextFontDescriptor->letterSpacing + gCurrentTextFontDescriptor->glyphs[*ch & 0xFF].width;
        }
        ch++;
    }

    return width;
}

// 0x4D5BA4
static int textFontGetCharacterWidthImpl(int ch)
{
    return gCurrentTextFontDescriptor->glyphs[ch & 0xFF].width;
}

// 0x4D5BB8
static int textFontGetMonospacedStringWidthImpl(const char* string)
{
    return fontGetMonospacedCharacterWidth() * strlen(string);
}

// 0x4D5BD8
static int textFontGetLetterSpacingImpl()
{
    return gCurrentTextFontDescriptor->letterSpacing;
}

// 0x4D5BE4
static int textFontGetBufferSizeImpl(const char* string)
{
    return fontGetStringWidth(string) * fontGetLineHeight();
}

// 0x4D5BF8
static int textFontGetMonospacedCharacterWidthImpl()
{
    int width = 0;

    for (int index = 0; index < gCurrentTextFontDescriptor->glyphCount; index++) {
        TextFontGlyph* glyph = &(gCurrentTextFontDescriptor->glyphs[index]);
        if (width < glyph->width) {
            width = glyph->width;
        }
    }

    return width + gCurrentTextFontDescriptor->letterSpacing;
}

} // namespace fallout
