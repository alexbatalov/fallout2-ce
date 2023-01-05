#ifndef TEXT_OBJECT_H
#define TEXT_OBJECT_H

#include "geometry.h"
#include "obj_types.h"

namespace fallout {

int textObjectsInit(unsigned char* windowBuffer, int width, int height);
int textObjectsReset();
void textObjectsFree();
void textObjectsDisable();
void textObjectsEnable();
void textObjectsSetBaseDelay(double value);
void textObjectsSetLineDelay(double value);
int textObjectAdd(Object* object, char* string, int font, int color, int outlineColor, Rect* rect);
void textObjectsRenderInRect(Rect* rect);
int textObjectsGetCount();
void textObjectsRemoveByOwner(Object* object);

} // namespace fallout

#endif /* TEXT_OBJECT_H */
