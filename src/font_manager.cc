#include "font_manager.h"

#include <stdio.h>
#include <string.h>

#include "color.h"
#include "db.h"
#include "memory_manager.h"
#include "word_wrap.h"

// The maximum number of interface fonts.
#define INTERFACE_FONT_MAX (16)

namespace fallout {

typedef struct InterfaceFontGlyph {
    short width;
    short height;
    int offset;
} InterfaceFontGlyph;

typedef struct InterfaceFontDescriptor {
    short maxHeight;
    short letterSpacing;
    short wordSpacing;
    short lineSpacing;
    short field_8;
    short field_A;
    InterfaceFontGlyph glyphs[256];
    unsigned char* data;
} InterfaceFontDescriptor;

static int interfaceFontLoad(int font);
static void interfaceFontSetCurrentImpl(int font);
static int interfaceFontGetLineHeightImpl();
static int interfaceFontGetStringWidthImpl(const char* string);
static int interfaceFontGetCharacterWidthImpl(int ch);
static int interfaceFontGetMonospacedStringWidthImpl(const char* string);
static int interfaceFontGetLetterSpacingImpl();
static int interfaceFontGetBufferSizeImpl(const char* string);
static int interfaceFontGetMonospacedCharacterWidthImpl();
static void interfaceFontDrawImpl(unsigned char* buf, const char* string, int length, int pitch, int color);
static void interfaceFontByteSwapUInt32(unsigned int* value);
static void interfaceFontByteSwapInt32(int* value);
static void interfaceFontByteSwapUInt16(unsigned short* value);
static void interfaceFontByteSwapInt16(short* value);

// 0x518680
static bool gInterfaceFontsInitialized = false;

// 0x518684
static int gInterfaceFontsLength = 0;

// 0x518688
FontManager gModernFontManager = {
    100,
    110,
    interfaceFontSetCurrentImpl,
    interfaceFontDrawImpl,
    interfaceFontGetLineHeightImpl,
    interfaceFontGetStringWidthImpl,
    interfaceFontGetCharacterWidthImpl,
    interfaceFontGetMonospacedStringWidthImpl,
    interfaceFontGetLetterSpacingImpl,
    interfaceFontGetBufferSizeImpl,
    interfaceFontGetMonospacedCharacterWidthImpl,
    leagcyWordWrap,
};

// 0x586838
static InterfaceFontDescriptor gInterfaceFontDescriptors[INTERFACE_FONT_MAX];

// 0x58E938
static int gCurrentInterfaceFont;

// 0x58E93C
static InterfaceFontDescriptor* gCurrentInterfaceFontDescriptor;

// 0x441C80
int interfaceFontsInit()
{
    int currentFont = -1;

    for (int font = 0; font < INTERFACE_FONT_MAX; font++) {
        if (interfaceFontLoad(font) == -1) {
            gInterfaceFontDescriptors[font].maxHeight = 0;
            gInterfaceFontDescriptors[font].data = NULL;
        } else {
            ++gInterfaceFontsLength;

            if (currentFont == -1) {
                currentFont = font;
            }
        }
    }

    if (currentFont == -1) {
        return -1;
    }

    gInterfaceFontsInitialized = true;

    interfaceFontSetCurrentImpl(currentFont + 100);

    return 0;
}

// 0x441CEC
void interfaceFontsExit()
{
    for (int font = 0; font < INTERFACE_FONT_MAX; font++) {
        if (gInterfaceFontDescriptors[font].data != NULL) {
            internal_free_safe(gInterfaceFontDescriptors[font].data, __FILE__, __LINE__); // FONTMGR.C, 124
        }
    }
}

// 0x441D20
static int interfaceFontLoad(int font_index)
{
    InterfaceFontDescriptor* fontDescriptor = &(gInterfaceFontDescriptors[font_index]);

    char path[56];
    snprintf(path, sizeof(path), "font%d.aaf", font_index);

    File* stream = fileOpen(path, "rb");
    if (stream == NULL) {
        return -1;
    }

    int fileSize = fileGetSize(stream);

    int sig;
    if (fileRead(&sig, 4, 1, stream) != 1) {
        fileClose(stream);
        return -1;
    }

    interfaceFontByteSwapInt32(&sig);
    if (sig != 0x41414646) {
        fileClose(stream);
        return -1;
    }

    if (fileRead(&(fontDescriptor->maxHeight), 2, 1, stream) != 1) {
        fileClose(stream);
        return -1;
    }
    interfaceFontByteSwapInt16(&(fontDescriptor->maxHeight));

    if (fileRead(&(fontDescriptor->letterSpacing), 2, 1, stream) != 1) {
        fileClose(stream);
        return -1;
    }
    interfaceFontByteSwapInt16(&(fontDescriptor->letterSpacing));

    if (fileRead(&(fontDescriptor->wordSpacing), 2, 1, stream) != 1) {
        fileClose(stream);
        return -1;
    }
    interfaceFontByteSwapInt16(&(fontDescriptor->wordSpacing));

    if (fileRead(&(fontDescriptor->lineSpacing), 2, 1, stream) != 1) {
        fileClose(stream);
        return -1;
    }
    interfaceFontByteSwapInt16(&(fontDescriptor->lineSpacing));

    for (int index = 0; index < 256; index++) {
        InterfaceFontGlyph* glyph = &(fontDescriptor->glyphs[index]);

        if (fileRead(&(glyph->width), 2, 1, stream) != 1) {
            fileClose(stream);
            return -1;
        }
        interfaceFontByteSwapInt16(&(glyph->width));

        if (fileRead(&(glyph->height), 2, 1, stream) != 1) {
            fileClose(stream);
            return -1;
        }
        interfaceFontByteSwapInt16(&(glyph->height));

        if (fileRead(&(glyph->offset), 4, 1, stream) != 1) {
            fileClose(stream);
            return -1;
        }
        interfaceFontByteSwapInt32(&(glyph->offset));
    }

    int glyphDataSize = fileSize - 2060;

    fontDescriptor->data = (unsigned char*)internal_malloc_safe(glyphDataSize, __FILE__, __LINE__); // FONTMGR.C, 259
    if (fontDescriptor->data == NULL) {
        fileClose(stream);
        return -1;
    }

    if (fileRead(fontDescriptor->data, glyphDataSize, 1, stream) != 1) {
        internal_free_safe(fontDescriptor->data, __FILE__, __LINE__); // FONTMGR.C, 268
        fileClose(stream);
        return -1;
    }

    fileClose(stream);
    return 0;
}

// 0x442120
static void interfaceFontSetCurrentImpl(int font)
{
    if (!gInterfaceFontsInitialized) {
        return;
    }

    font -= 100;

    if (gInterfaceFontDescriptors[font].data != NULL) {
        gCurrentInterfaceFont = font;
        gCurrentInterfaceFontDescriptor = &(gInterfaceFontDescriptors[font]);
    }
}

// 0x442168
static int interfaceFontGetLineHeightImpl()
{
    if (!gInterfaceFontsInitialized) {
        return 0;
    }

    return gCurrentInterfaceFontDescriptor->lineSpacing + gCurrentInterfaceFontDescriptor->maxHeight;
}

// 0x442188
static int interfaceFontGetStringWidthImpl(const char* string)
{
    if (!gInterfaceFontsInitialized) {
        return 0;
    }

    int stringWidth = 0;

    while (*string != '\0') {
        unsigned char ch = static_cast<unsigned char>(*string++);

        int characterWidth;
        if (ch == ' ') {
            characterWidth = gCurrentInterfaceFontDescriptor->wordSpacing;
        } else {
            characterWidth = gCurrentInterfaceFontDescriptor->glyphs[ch].width;
        }

        stringWidth += characterWidth + gCurrentInterfaceFontDescriptor->letterSpacing;
    }

    return stringWidth;
}

// 0x4421DC
static int interfaceFontGetCharacterWidthImpl(int ch)
{
    int width;

    if (!gInterfaceFontsInitialized) {
        return 0;
    }

    if (ch == ' ') {
        width = gCurrentInterfaceFontDescriptor->wordSpacing;
    } else {
        width = gCurrentInterfaceFontDescriptor->glyphs[ch].width;
    }

    return width;
}

// 0x442210
static int interfaceFontGetMonospacedStringWidthImpl(const char* str)
{
    if (!gInterfaceFontsInitialized) {
        return 0;
    }

    return interfaceFontGetMonospacedCharacterWidthImpl() * strlen(str);
}

// 0x442240
static int interfaceFontGetLetterSpacingImpl()
{
    if (!gInterfaceFontsInitialized) {
        return 0;
    }

    return gCurrentInterfaceFontDescriptor->letterSpacing;
}

// 0x442258
static int interfaceFontGetBufferSizeImpl(const char* str)
{
    if (!gInterfaceFontsInitialized) {
        return 0;
    }

    return interfaceFontGetStringWidthImpl(str) * interfaceFontGetLineHeightImpl();
}

// 0x442278
static int interfaceFontGetMonospacedCharacterWidthImpl()
{
    if (!gInterfaceFontsInitialized) {
        return 0;
    }

    int v1;
    if (gCurrentInterfaceFontDescriptor->wordSpacing <= gCurrentInterfaceFontDescriptor->field_8) {
        v1 = gCurrentInterfaceFontDescriptor->lineSpacing;
    } else {
        v1 = gCurrentInterfaceFontDescriptor->letterSpacing;
    }

    return v1 + gCurrentInterfaceFontDescriptor->maxHeight;
}

// 0x4422B4
static void interfaceFontDrawImpl(unsigned char* buf, const char* string, int length, int pitch, int color)
{
    if (!gInterfaceFontsInitialized) {
        return;
    }

    if ((color & FONT_SHADOW) != 0) {
        color &= ~FONT_SHADOW;
        // NOTE: Other font options preserved. This is different from text font
        // shadows.
        interfaceFontDrawImpl(buf + pitch + 1, string, length, pitch, (color & ~0xFF) | _colorTable[0]);
    }

    unsigned char* palette = _getColorBlendTable(color & 0xFF);

    int monospacedCharacterWidth;
    if ((color & FONT_MONO) != 0) {
        // NOTE: Uninline.
        monospacedCharacterWidth = interfaceFontGetMonospacedCharacterWidthImpl();
    }

    unsigned char* ptr = buf;
    while (*string != '\0') {
        unsigned char ch = static_cast<unsigned char>(*string++);

        int characterWidth;
        if (ch == ' ') {
            characterWidth = gCurrentInterfaceFontDescriptor->wordSpacing;
        } else {
            characterWidth = gCurrentInterfaceFontDescriptor->glyphs[ch].width;
        }

        unsigned char* end;
        if ((color & FONT_MONO) != 0) {
            end = ptr + monospacedCharacterWidth;
            ptr += (monospacedCharacterWidth - characterWidth - gCurrentInterfaceFontDescriptor->letterSpacing) / 2;
        } else {
            end = ptr + characterWidth + gCurrentInterfaceFontDescriptor->letterSpacing;
        }

        if (end - buf > length) {
            break;
        }

        InterfaceFontGlyph* glyph = &(gCurrentInterfaceFontDescriptor->glyphs[ch]);
        unsigned char* glyphDataPtr = gCurrentInterfaceFontDescriptor->data + glyph->offset;

        // Skip blank pixels (difference between font's line height and glyph height).
        ptr += (gCurrentInterfaceFontDescriptor->maxHeight - glyph->height) * pitch;

        for (int y = 0; y < glyph->height; y++) {
            for (int x = 0; x < glyph->width; x++) {
                unsigned char byte = *glyphDataPtr++;

                *ptr++ = palette[(byte << 8) + *ptr];
            }

            ptr += pitch - glyph->width;
        }

        ptr = end;
    }

    if ((color & FONT_UNDERLINE) != 0) {
        int length = ptr - buf;
        unsigned char* underlinePtr = buf + pitch * (gCurrentInterfaceFontDescriptor->maxHeight - 1);
        for (int index = 0; index < length; index++) {
            *underlinePtr++ = color & 0xFF;
        }
    }

    _freeColorBlendTable(color & 0xFF);
}

// NOTE: Inlined.
//
// 0x442520
static void interfaceFontByteSwapUInt32(unsigned int* value)
{
    unsigned int swapped = *value;
    unsigned short high = swapped >> 16;
    // NOTE: Uninline.
    interfaceFontByteSwapUInt16(&high);
    unsigned short low = swapped & 0xFFFF;
    // NOTE: Uninline.
    interfaceFontByteSwapUInt16(&low);
    *value = (low << 16) | high;
}

// NOTE: 0x442520 with different signature.
static void interfaceFontByteSwapInt32(int* value)
{
    interfaceFontByteSwapUInt32((unsigned int*)value);
}

// 0x442568
static void interfaceFontByteSwapUInt16(unsigned short* value)
{
    unsigned short swapped = *value;
    swapped = (swapped >> 8) | (swapped << 8);
    *value = swapped;
}

// NOTE: 0x442568 with different signature.
static void interfaceFontByteSwapInt16(short* value)
{
    interfaceFontByteSwapUInt16((unsigned short*)value);
}

} // namespace fallout
