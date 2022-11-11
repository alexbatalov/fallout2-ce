#include "font_manager.h"

#include <stdio.h>
#include <string.h>

#include "color.h"
#include "db.h"
#include "memory_manager.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_BITMAP_H

#include "config.h"

#include "word_wrap.h"

#include <map> 
#include "iconv.h"
#include "settings.h"

// The maximum number of interface fonts.
#define FT_FONT_MAX (16)

namespace fallout {

typedef struct FtFontGlyph {
    short width;
    short rows;
    short left;
    short top;
    unsigned char* buffer;
} FtFontGlyph;

typedef struct FtFontDescriptor {
    FT_Library library;
    FT_Face face;

    unsigned char* filebuffer;

    int maxHeight;
    int maxWidth;
    int letterSpacing;
    int wordSpacing;
    int lineSpacing;
    int heightOffset;
    int warpMode;
    char encoding[64];

    std::map<uint32_t, FtFontGlyph> map;
} FtFontDescriptor;

static int FtFontLoad(int font);
static void FtFontSetCurrentImpl(int font);
static int FtFontGetLineHeightImpl();
static int FtFontGetStringWidthImpl(const char* string);
static int FtFontGetCharacterWidthImpl(int ch);
static int FtFontGetMonospacedStringWidthImpl(const char* string);
static int FtFontGetLetterSpacingImpl();
static int FtFontGetBufferSizeImpl(const char* string);
static int FtFontGetMonospacedCharacterWidthImpl();
static void FtFontDrawImpl(unsigned char* buf, const char* string, int length, int pitch, int color);

static int FtFonteWordWrapImpl(const char* string, int width, short* breakpoints, short* breakpointsLengthPtr);
// 0x518680
static bool gFtFontsInitialized = false;

// 0x518684
static int gFtFontsLength = 0;

static int knobWidth = 5;
static int knobHeight = 7;

static unsigned char knobDump[35] = {
    0x34, 0x82, 0xb6, 0x82,
    0x34, 0x82, 0xb6, 0xb6,
    0xb6, 0x82, 0xb6, 0xb6,
    0xb6, 0xb6, 0xb6, 0x82,
    0xb6, 0xb6, 0xb6, 0x82,
    0x34, 0x82, 0xb6, 0x82,
    0x34, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0
};

// 0x518688
FontManager gFtFontManager = {
    100,
    110,
    FtFontSetCurrentImpl,
    FtFontDrawImpl,
    FtFontGetLineHeightImpl,
    FtFontGetStringWidthImpl,
    FtFontGetCharacterWidthImpl,
    FtFontGetMonospacedStringWidthImpl,
    FtFontGetLetterSpacingImpl,
    FtFontGetBufferSizeImpl,
    FtFontGetMonospacedCharacterWidthImpl,
    FtFonteWordWrapImpl,
};

// 0x586838
static FtFontDescriptor gFtFontDescriptors[FT_FONT_MAX];

// 0x58E938
static int gCurrentFtFont;

// 0x58E93C
static FtFontDescriptor* current;

static uint32_t output[1024] = {
    0x0,
};

static int LtoU(const char* input, size_t charInPutLen)
{
    if (input[0] == '\x95') {
        size_t output_size = 1024;
        iconv_t cd = iconv_open("UCS-4-INTERNAL", current->encoding);
        char* tmp = (char*)(output + 1);
        const char* tmp2 = (const char *)(input + 1);
        charInPutLen -= 1;
        iconv(cd, &tmp2, &charInPutLen, &tmp, &output_size);
        iconv_close(cd);

        output[0] = '\x95';
        return (1024 - output_size) / 4 + 1;
    } else {
        size_t output_size = 1024;
        iconv_t cd = iconv_open("UCS-4-INTERNAL", current->encoding);
        char* tmp = (char*)output;
        iconv(cd, &input, &charInPutLen, &tmp, &output_size);
        iconv_close(cd);

        return (1024 - output_size) / 4;
    }
}

static int UtoL(const char* input, size_t charInPutLen)
{
    size_t output_size = 1024;
    iconv_t cd = iconv_open(current->encoding, "UCS-4-INTERNAL");
    char* tmp = (char*)output;
    iconv(cd, &input, &charInPutLen, &tmp, &output_size);
    iconv_close(cd);

    return (1024 - output_size);
}

static FtFontGlyph GetFtFontGlyph(uint32_t unicode)
{
    if (current->map.count(unicode) > 0) {
        return current->map[unicode];
    } else {
        if (unicode == '\x95') {
            current->map[unicode].left = 0;
            current->map[unicode].top = current->maxHeight / 2;
            current->map[unicode].width = knobWidth;
            current->map[unicode].rows = knobHeight;
            current->map[unicode].buffer = knobDump;
        } else {
            FT_Load_Glyph(current->face, FT_Get_Char_Index(current->face, unicode), FT_LOAD_DEFAULT | FT_LOAD_NO_BITMAP);
            FT_Render_Glyph(current->face->glyph, FT_RENDER_MODE_NORMAL);

            current->map[unicode].left = current->face->glyph->bitmap_left;
            current->map[unicode].top = current->face->glyph->bitmap_top;
            current->map[unicode].width = current->face->glyph->bitmap.width;
            current->map[unicode].rows = current->face->glyph->bitmap.rows;

            int count = current->face->glyph->bitmap.width * current->face->glyph->bitmap.rows;

            if (count > 0) {
                current->map[unicode].buffer = (unsigned char*)internal_malloc_safe(count, __FILE__, __LINE__); // FONTMGR.C, 259
                memcpy(current->map[unicode].buffer, current->face->glyph->bitmap.buffer, count);
            } else {
                current->map[unicode].buffer = NULL;
            }
        }

        return current->map[unicode];
    }
}

// 0x441C80
int FtFontsInit()
{
    int currentFont = -1;

    for (int font = 0; font < FT_FONT_MAX; font++) {
        if (FtFontLoad(font) == -1) {
            gFtFontDescriptors[font].maxHeight = 0;
            gFtFontDescriptors[font].filebuffer = NULL;
        } else {
            ++gFtFontsLength;

            if (currentFont == -1) {
                currentFont = font;
            }

            gFtFontManager.maxFont = gFtFontsLength + 100;
        }
    }

    if (currentFont == -1) {
        return -1;
    }

    gFtFontsInitialized = true;

    FtFontSetCurrentImpl(currentFont + 100);

    return 0;
}

// 0x441CEC
void FtFontsExit()
{
    for (int font = 0; font < FT_FONT_MAX; font++) {
        if (gFtFontDescriptors[font].filebuffer != NULL) {
            internal_free_safe(gFtFontDescriptors[font].filebuffer, __FILE__, __LINE__); // FONTMGR.C, 124
        }
    }
    //TODO: clean up
}

// 0x441D20
static int FtFontLoad(int font_index)
{
    char string[56];
    FtFontDescriptor* desc = &(gFtFontDescriptors[font_index]);

    Config config;
    if (!configInit(&config)) {
        return -1;
    }

    sprintf(string, "fonts/%s/font.ini", settings.system.language.c_str());
    if (!configRead(&config, string, false)) {
        return -1;
    }

    sprintf(string, "font%d", font_index);

    if (!configGetInt(&config, string, "maxHeight", &desc->maxHeight)) {
        return -1;
    }
    if (!configGetInt(&config, string, "maxWidth", &desc->maxWidth)) {
        desc->maxWidth = desc->maxHeight;
    }
    if (!configGetInt(&config, string, "lineSpacing", &desc->lineSpacing)) {
        return -1;
    }
    if (!configGetInt(&config, string, "wordSpacing", &desc->wordSpacing)) {
        return -1;
    }
    if (!configGetInt(&config, string, "letterSpacing", &desc->letterSpacing)) {
        return -1;
    }
    if (!configGetInt(&config, string, "heightOffset", &desc->heightOffset)) {
        return -1;
    }
    if (!configGetInt(&config, string, "warpMode", &desc->warpMode)) {
        return -1;
    }
    char* encoding = NULL;
    if (!configGetString(&config, string, "encoding", &encoding)) {
        return -1;
    }
    strcpy(desc->encoding, encoding);
    
    char *fontFileName = NULL;
    if (!configGetString(&config, string, "fileName", &fontFileName)) {
        return -1;
    }

    sprintf(string, "fonts/%s/%s", settings.system.language.c_str(), fontFileName);

    File* stream = fileOpen(string, "rb");
    if (stream == NULL) {
        return -1;
    }

    int fileSize = fileGetSize(stream); //19647736

    desc->filebuffer = (unsigned char*)internal_malloc_safe(fileSize, __FILE__, __LINE__); // FONTMGR.C, 259

    int readleft = fileSize;
    unsigned char* ptr = desc->filebuffer;

    while (readleft > 10000) {
        int readsize = fileRead(ptr, 1, 10000, stream);
        if (readsize != 10000) {
            return -1;
        }
        readleft -= 10000;
        ptr += 10000;
    }

    if (fileRead(ptr, 1, readleft, stream) != readleft) {
        return -1;
    }

    FT_Init_FreeType(&(desc->library));
    FT_New_Memory_Face(desc->library, desc->filebuffer, fileSize, 0, &desc->face);

    FT_Select_Charmap(desc->face, FT_ENCODING_UNICODE);
    FT_Set_Pixel_Sizes(desc->face, desc->maxWidth, desc->maxHeight);

    fileClose(stream);
    configFree(&config);
    return 0;
}

// 0x442120
static void FtFontSetCurrentImpl(int font)
{
    if (!gFtFontsInitialized) {
        return;
    }

    font -= 100;

    if (gFtFontDescriptors[font].filebuffer != NULL) {
        gCurrentFtFont = font;
        current = &(gFtFontDescriptors[font]);
    }
}

// 0x442168
static int FtFontGetLineHeightImpl()
{
    if (!gFtFontsInitialized) {
        return 0;
    }

    return current->lineSpacing + current->maxHeight + current->heightOffset;
}

// 0x442188
static int FtFontGetStringWidthImpl(const char* string)
{
    if (!gFtFontsInitialized) {
        return 0;
    }

    if (strlen(string) == 1 && string[0] < 0) {
        return current->wordSpacing;
    } else {
        int count;
        int width = 0;

        count = LtoU((char*)string, strlen(string));

        for (int i = 0; i < count; i++) {
            uint32_t ch = output[i];

            if (ch == L'\n' || ch == L'\r') {
                continue;
            }

            if (ch == '\x95') {
                FtFontGlyph g = GetFtFontGlyph(ch);
                width += g.width + current->letterSpacing + 2;
            } else if (ch == L' ' || (ch < 256 && ch > 128)) {
                width += current->wordSpacing + current->letterSpacing;
            } else {
                FtFontGlyph g = GetFtFontGlyph(ch);
                width += g.width + current->letterSpacing;
            }
        }

        return width;
    }
}

// 0x4421DC
static int FtFontGetCharacterWidthImpl(int ch)
{
    if (!gFtFontsInitialized) {
        return 0;
    }

    return current->wordSpacing;
}

// 0x442210
static int FtFontGetMonospacedStringWidthImpl(const char* str)
{
    return FtFontGetStringWidthImpl(str);
}

// 0x442240
static int FtFontGetLetterSpacingImpl()
{
    if (!gFtFontsInitialized) {
        return 0;
    }

    return current->letterSpacing;
}

// 0x442258
static int FtFontGetBufferSizeImpl(const char* str)
{
    if (!gFtFontsInitialized) {
        return 0;
    }

    return FtFontGetStringWidthImpl(str) * (current->lineSpacing + current->maxHeight + current->heightOffset);
}

// 0x442278
static int FtFontGetMonospacedCharacterWidthImpl()
{
    if (!gFtFontsInitialized) {
        return 0;
    }

    return current->lineSpacing + current->maxHeight;
}
                                                                                                                                                
// 0x4422B4
static void FtFontDrawImpl(unsigned char* buf, const char* string, int length, int pitch, int color)
{
    if (!gFtFontsInitialized) {
        return;
    }

    if ((color & FONT_SHADOW) != 0) {
        color &= ~FONT_SHADOW;
        // NOTE: Other font options preserved. This is different from text font
        // shadows.
        FtFontDrawImpl(buf + pitch + 1, string, length, pitch, (color & ~0xFF) | _colorTable[0]);
    }

    unsigned char* palette = _getColorBlendTable(color & 0xFF);
    int count = LtoU((char*)string, strlen(string));
    int maxTop = -1;

    for (int i = 0; i < count; i++) {
        uint32_t ch = output[i];

        FtFontGlyph g = GetFtFontGlyph(ch);
        if (maxTop < g.top)
            maxTop = g.top;
    }

    maxTop = current->maxHeight - maxTop;

    unsigned char* ptr = buf;
    for (int i = 0; i < count; i++) {
        uint32_t ch = output[i];

        FtFontGlyph g = GetFtFontGlyph(ch);

        int characterWidth;
        if (ch == L'\n' || ch == L'\r') {
            continue;
        } else if (ch == L' ') {
            characterWidth = current->wordSpacing;
        } else if (ch == '\x95') {
            characterWidth = g.width + 2;
        } else {
            characterWidth = g.width;
        }

        unsigned char* end = ptr + characterWidth + current->letterSpacing;
        if (end - buf > length) {
            break;
        }

        ptr += (current->maxHeight - g.top - maxTop) * pitch;

        unsigned char* glyphDataPtr = g.buffer;

        for (int y = 0; y < g.rows && y < current->maxHeight; y++) {
            for (int x = 0; x < g.width; x++) {
                unsigned char byte = *glyphDataPtr++;
                byte /= 26;

                *ptr++ = palette[(byte << 8) + *ptr];
            }

            ptr += pitch - g.width;
        }

        ptr = end;
    }

    if ((color & FONT_UNDERLINE) != 0) {
        int length = ptr - buf;
        unsigned char* underlinePtr = buf + pitch * (current->maxHeight - 1);
        for (int index = 0; index < length; index++) {
            *underlinePtr++ = color & 0xFF;
        }
    }

    _freeColorBlendTable(color & 0xFF);
}

static int FtFonteWordWrapImpl(const char* string, int width, short* breakpoints, short* breakpointsLengthPtr)
{
    breakpoints[0] = 0;
    *breakpointsLengthPtr = 1;

    for (int index = 1; index < WORD_WRAP_MAX_COUNT; index++) {
        breakpoints[index] = -1;
    }

    if (fontGetStringWidth(string) < width) {
        breakpoints[*breakpointsLengthPtr] = (short)strlen(string);
        *breakpointsLengthPtr += 1;
        return 0;
    }

    int accum = 0;
    int prevSpaceOrHyphen = -1;

    int count = LtoU((char*)string, strlen(string));

    int PreCharIndex;

    int uint32Index;
    int CharIndex = 0;

    for (int i = 0; i < count;) 
    {
        const uint32_t ch = output[i];

        if (ch == L'\n' || ch == L'\r') {
            continue;
        }

        FtFontGlyph g = GetFtFontGlyph(ch);

        PreCharIndex = CharIndex;

        if (ch == L' ' || (ch > 128 && ch < 256)) {
            accum += current->letterSpacing + current->wordSpacing;
            CharIndex += 1;
        } else {
            if (ch == '\x95')
                accum += current->letterSpacing + g.width + 2;
            else
                accum += current->letterSpacing + g.width;

            if ((ch > 0 && ch < 128) || ch == '\x95')
                CharIndex += 1;
            else
                CharIndex += UtoL((char*)(&(output[i])), sizeof(uint32_t));
        }

        if (accum <= width) {
            // NOTE: quests.txt #807 uses extended ascii.
            if (ch == L' ' || ch == L'-') {
                prevSpaceOrHyphen = CharIndex;
                uint32Index = i;
            } else if (current->warpMode == 1) {
                prevSpaceOrHyphen = -1;
            }
            i++;
        } else {
            if (*breakpointsLengthPtr == WORD_WRAP_MAX_COUNT) {
                return -1;
            }

            if (prevSpaceOrHyphen != -1) {
                // Word wrap.
                breakpoints[*breakpointsLengthPtr] = prevSpaceOrHyphen;

                i = uint32Index + 1;
                CharIndex = prevSpaceOrHyphen + 1;
            } else {
                // Character wrap.
                breakpoints[*breakpointsLengthPtr] = PreCharIndex;
                CharIndex = PreCharIndex;
            }

            *breakpointsLengthPtr += 1;
            prevSpaceOrHyphen = -1;
            accum = 0;
        }
    }

    if (*breakpointsLengthPtr == WORD_WRAP_MAX_COUNT) {
        return -1;
    }

    breakpoints[*breakpointsLengthPtr] = CharIndex;
    *breakpointsLengthPtr += 1;

    return 0;
}

} // namespace fallout
