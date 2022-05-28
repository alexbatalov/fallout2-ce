#include "text_object.h"

#include "core.h"
#include "debug.h"
#include "draw.h"
#include "game_config.h"
#include "memory.h"
#include "object.h"
#include "text_font.h"
#include "tile.h"
#include "word_wrap.h"

// 0x51D944
int gTextObjectsCount = 0;

// 0x51D948
unsigned int gTextObjectsBaseDelay = 3500;

// 0x51D94C
unsigned int gTextObjectsLineDelay = 1399;

// 0x6681C0
TextObject* gTextObjects[TEXT_OBJECTS_MAX_COUNT];

// 0x668210
int gTextObjectsWindowWidth;

// 0x668214
int gTextObjectsWindowHeight;

// 0x668218
unsigned char* gTextObjectsWindowBuffer;

// 0x66821C
bool gTextObjectsEnabled;

// 0x668220
bool gTextObjectsInitialized;

// 0x4B0130
int textObjectsInit(unsigned char* windowBuffer, int width, int height)
{
    if (gTextObjectsInitialized) {
        return -1;
    }

    gTextObjectsWindowBuffer = windowBuffer;
    gTextObjectsWindowWidth = width;
    gTextObjectsWindowHeight = height;
    gTextObjectsCount = 0;

    tickersAdd(textObjectsTicker);

    double textBaseDelay;
    if (!configGetDouble(&gGameConfig, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_TEXT_BASE_DELAY_KEY, &textBaseDelay)) {
        textBaseDelay = 3.5;
    }

    double textLineDelay;
    if (!configGetDouble(&gGameConfig, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_TEXT_LINE_DELAY_KEY, &textLineDelay)) {
        textLineDelay = 1.399993896484375;
    }

    gTextObjectsBaseDelay = (unsigned int)(textBaseDelay * 1000.0);
    gTextObjectsLineDelay = (unsigned int)(textLineDelay * 1000.0);

    gTextObjectsEnabled = true;
    gTextObjectsInitialized = true;

    return 0;
}

// 0x4B021C
int textObjectsReset()
{
    if (!gTextObjectsInitialized) {
        return -1;
    }

    for (int index = 0; index < gTextObjectsCount; index++) {
        internal_free(gTextObjects[index]->data);
        internal_free(gTextObjects[index]);
    }

    gTextObjectsCount = 0;
    tickersAdd(textObjectsTicker);

    return 0;
}

// 0x4B0280
void textObjectsFree()
{
    if (gTextObjectsInitialized) {
        textObjectsReset();
        tickersRemove(textObjectsTicker);
        gTextObjectsInitialized = false;
    }
}

// 0x4B02A4
void textObjectsDisable()
{
    gTextObjectsEnabled = false;
}

// 0x4B02B0
void textObjectsEnable()
{
    gTextObjectsEnabled = true;
}

// 0x4B02C4
void textObjectsSetBaseDelay(double value)
{
    if (value < 1.0) {
        value = 1.0;
    }

    gTextObjectsBaseDelay = (int)(value * 1000.0);
}

// 0x4B031C
void textObjectsSetLineDelay(double value)
{
    if (value < 0.0) {
        value = 0.0;
    }

    gTextObjectsLineDelay = (int)(value * 1000.0);
}

// text_object_create
// 0x4B036C
int textObjectAdd(Object* object, char* string, int font, int color, int a5, Rect* rect)
{
    if (!gTextObjectsInitialized) {
        return -1;
    }

    // SFALL: Fix incorrect value of the limit number of floating messages.
    if (gTextObjectsCount >= TEXT_OBJECTS_MAX_COUNT) {
        return -1;
    }

    if (string == NULL) {
        return -1;
    }

    if (*string == '\0') {
        return -1;
    }

    TextObject* textObject = (TextObject*)internal_malloc(sizeof(*textObject));
    if (textObject == NULL) {
        return -1;
    }

    memset(textObject, 0, sizeof(*textObject));

    int oldFont = fontGetCurrent();
    fontSetCurrent(font);

    short beginnings[WORD_WRAP_MAX_COUNT];
    short count;
    if (wordWrap(string, 200, beginnings, &count) != 0) {
        fontSetCurrent(oldFont);
        return -1;
    }

    textObject->linesCount = count - 1;
    if (textObject->linesCount < 1) {
        debugPrint("**Error in text_object_create()\n");
    }

    textObject->width = 0;

    for (int index = 0; index < textObject->linesCount; index++) {
        char* ending = string + beginnings[index + 1];
        char* beginning = string + beginnings[index];
        if (ending[-1] == ' ') {
            --ending;
        }

        char c = *ending;
        *ending = '\0';

        // NOTE: Calls [fontGetStringWidth] twice, probably result of using min/max macro
        int width = fontGetStringWidth(beginning);
        if (width >= textObject->width) {
            textObject->width = width;
        }

        *ending = c;
    }

    textObject->height = (fontGetLineHeight() + 1) * textObject->linesCount;

    if (a5 != -1) {
        textObject->width += 2;
        textObject->height += 2;
    }

    int size = textObject->width * textObject->height;
    textObject->data = (unsigned char*)internal_malloc(size);
    if (textObject->data == NULL) {
        fontSetCurrent(oldFont);
        return -1;
    }

    memset(textObject->data, 0, size);

    unsigned char* dest = textObject->data;
    int skip = textObject->width * (fontGetLineHeight() + 1);

    if (a5 != -1) {
        dest += textObject->width;
    }

    for (int index = 0; index < textObject->linesCount; index++) {
        char* beginning = string + beginnings[index];
        char* ending = string + beginnings[index + 1];
        if (ending[-1] == ' ') {
            --ending;
        }

        char c = *ending;
        *ending = '\0';

        int width = fontGetStringWidth(beginning);
        fontDrawText(dest + (textObject->width - width) / 2, beginning, textObject->width, textObject->width, color);

        *ending = c;

        dest += skip;
    }

    if (a5 != -1) {
        bufferOutline(textObject->data, textObject->width, textObject->height, textObject->width, a5);
    }

    if (object != NULL) {
        textObject->tile = object->tile;
    } else {
        textObject->flags |= TEXT_OBJECT_UNBOUNDED;
        textObject->tile = gCenterTile;
    }

    textObjectFindPlacement(textObject);

    if (rect != NULL) {
        rect->left = textObject->x;
        rect->top = textObject->y;
        rect->right = textObject->x + textObject->width - 1;
        rect->bottom = textObject->y + textObject->height - 1;
    }

    textObjectsRemoveByOwner(object);

    textObject->owner = object;
    textObject->time = _get_bk_time();

    gTextObjects[gTextObjectsCount] = textObject;
    gTextObjectsCount++;

    fontSetCurrent(oldFont);

    return 0;
}

// 0x4B06E8
void textObjectsRenderInRect(Rect* rect)
{
    if (!gTextObjectsInitialized) {
        return;
    }

    for (int index = 0; index < gTextObjectsCount; index++) {
        TextObject* textObject = gTextObjects[index];
        tileToScreenXY(textObject->tile, &(textObject->x), &(textObject->y), gElevation);
        textObject->x += textObject->sx;
        textObject->y += textObject->sy;

        Rect textObjectRect;
        textObjectRect.left = textObject->x;
        textObjectRect.top = textObject->y;
        textObjectRect.right = textObject->width + textObject->x - 1;
        textObjectRect.bottom = textObject->height + textObject->y - 1;
        if (rectIntersection(&textObjectRect, rect, &textObjectRect) == 0) {
            blitBufferToBufferTrans(textObject->data + textObject->width * (textObjectRect.top - textObject->y) + (textObjectRect.left - textObject->x),
                textObjectRect.right - textObjectRect.left + 1,
                textObjectRect.bottom - textObjectRect.top + 1,
                textObject->width,
                gTextObjectsWindowBuffer + gTextObjectsWindowWidth * textObjectRect.top + textObjectRect.left,
                gTextObjectsWindowWidth);
        }
    }
}

// 0x4B07F0
int textObjectsGetCount()
{
    return gTextObjectsCount;
}

// 0x4B07F8
void textObjectsTicker()
{
    if (!gTextObjectsEnabled) {
        return;
    }

    bool textObjectsRemoved = false;
    Rect dirtyRect;

    for (int index = 0; index < gTextObjectsCount; index++) {
        TextObject* textObject = gTextObjects[index];

        unsigned int delay = gTextObjectsLineDelay * textObject->linesCount + gTextObjectsBaseDelay;
        if ((textObject->flags & TEXT_OBJECT_MARKED_FOR_REMOVAL) != 0 || (getTicksBetween(_get_bk_time(), textObject->time) > delay)) {
            tileToScreenXY(textObject->tile, &(textObject->x), &(textObject->y), gElevation);
            textObject->x += textObject->sx;
            textObject->y += textObject->sy;

            Rect textObjectRect;
            textObjectRect.left = textObject->x;
            textObjectRect.top = textObject->y;
            textObjectRect.right = textObject->width + textObject->x - 1;
            textObjectRect.bottom = textObject->height + textObject->y - 1;

            if (textObjectsRemoved) {
                rectUnion(&dirtyRect, &textObjectRect, &dirtyRect);
            } else {
                rectCopy(&dirtyRect, &textObjectRect);
                textObjectsRemoved = true;
            }

            internal_free(textObject->data);
            internal_free(textObject);

            memmove(&(gTextObjects[index]), &(gTextObjects[index + 1]), sizeof(*gTextObjects) * (gTextObjectsCount - index - 1));

            gTextObjectsCount--;
            index--;
        }
    }

    if (textObjectsRemoved) {
        tileWindowRefreshRect(&dirtyRect, gElevation);
    }
}

// Finds best position for placing text object.
//
// 0x4B0954
void textObjectFindPlacement(TextObject* textObject)
{
    int tileScreenX;
    int tileScreenY;
    tileToScreenXY(textObject->tile, &tileScreenX, &tileScreenY, gElevation);
    textObject->x = tileScreenX + 16 - textObject->width / 2;
    textObject->y = tileScreenY;

    if ((textObject->flags & TEXT_OBJECT_UNBOUNDED) == 0) {
        textObject->y -= textObject->height + 60;
    }

    if ((textObject->x >= 0 && textObject->x + textObject->width - 1 < gTextObjectsWindowWidth)
        && (textObject->y >= 0 && textObject->y + textObject->height - 1 < gTextObjectsWindowHeight)) {
        textObject->sx = textObject->x - tileScreenX;
        textObject->sy = textObject->y - tileScreenY;
        return;
    }

    textObject->x -= textObject->width / 2;
    if ((textObject->x >= 0 && textObject->x + textObject->width - 1 < gTextObjectsWindowWidth)
        && (textObject->y >= 0 && textObject->y + textObject->height - 1 < gTextObjectsWindowHeight)) {
        textObject->sx = textObject->x - tileScreenX;
        textObject->sy = textObject->y - tileScreenY;
        return;
    }

    textObject->x += textObject->width;
    if ((textObject->x >= 0 && textObject->x + textObject->width - 1 < gTextObjectsWindowWidth)
        && (textObject->y >= 0 && textObject->y + textObject->height - 1 < gTextObjectsWindowHeight)) {
        textObject->sx = textObject->x - tileScreenX;
        textObject->sy = textObject->y - tileScreenY;
        return;
    }

    textObject->x = tileScreenX - 16 - textObject->width;
    textObject->y = tileScreenY - 16 - textObject->height;
    if ((textObject->x >= 0 && textObject->x + textObject->width - 1 < gTextObjectsWindowWidth)
        && (textObject->y >= 0 && textObject->y + textObject->height - 1 < gTextObjectsWindowHeight)) {
        textObject->sx = textObject->x - tileScreenX;
        textObject->sy = textObject->y - tileScreenY;
        return;
    }

    textObject->x += textObject->width + 64;
    if ((textObject->x >= 0 && textObject->x + textObject->width - 1 < gTextObjectsWindowWidth)
        && (textObject->y >= 0 && textObject->y + textObject->height - 1 < gTextObjectsWindowHeight)) {
        textObject->sx = textObject->x - tileScreenX;
        textObject->sy = textObject->y - tileScreenY;
        return;
    }

    textObject->x = tileScreenX + 16 - textObject->width / 2;
    textObject->y = tileScreenY;
    if ((textObject->x >= 0 && textObject->x + textObject->width - 1 < gTextObjectsWindowWidth)
        && (textObject->y >= 0 && textObject->y + textObject->height - 1 < gTextObjectsWindowHeight)) {
        textObject->sx = textObject->x - tileScreenX;
        textObject->sy = textObject->y - tileScreenY;
        return;
    }

    textObject->x -= textObject->width / 2;
    if ((textObject->x >= 0 && textObject->x + textObject->width - 1 < gTextObjectsWindowWidth)
        && (textObject->y >= 0 && textObject->y + textObject->height - 1 < gTextObjectsWindowHeight)) {
        textObject->sx = textObject->x - tileScreenX;
        textObject->sy = textObject->y - tileScreenY;
        return;
    }

    textObject->x += textObject->width;
    if ((textObject->x >= 0 && textObject->x + textObject->width - 1 < gTextObjectsWindowWidth)
        && (textObject->y >= 0 && textObject->y + textObject->height - 1 < gTextObjectsWindowHeight)) {
        textObject->sx = textObject->x - tileScreenX;
        textObject->sy = textObject->y - tileScreenY;
        return;
    }

    textObject->x = tileScreenX + 16 - textObject->width / 2;
    textObject->y = tileScreenY - (textObject->height + 60);
    textObject->sx = textObject->x - tileScreenX;
    textObject->sy = textObject->y - tileScreenY;
}

// Marks text objects attached to [object] for removal.
//
// 0x4B0C00
void textObjectsRemoveByOwner(Object* object)
{
    for (int index = 0; index < gTextObjectsCount; index++) {
        if (gTextObjects[index]->owner == object) {
            gTextObjects[index]->flags |= TEXT_OBJECT_MARKED_FOR_REMOVAL;
        }
    }
}
