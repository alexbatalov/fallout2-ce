#ifndef MMX_H
#define MMX_H

namespace fallout {

bool mmxIsSupported();
void mmxBlit(unsigned char* dest, int destPitch, unsigned char* src, int srcPitch, int width, int height);
void mmxBlitTrans(unsigned char* dest, int destPitch, unsigned char* src, int srcPitch, int width, int height);

} // namespace fallout

#endif /* MMX_H */
