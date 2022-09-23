#ifndef GRAYSCALE_H
#define GRAYSCALE_H

namespace fallout {

void grayscalePaletteUpdate(int a1, int a2);
void grayscalePaletteApply(unsigned char* surface, int width, int height, int pitch);

} // namespace fallout

#endif /* GRAYSCALE_H */
