#ifndef MMX_H
#define MMX_H

bool mmxIsSupported();
void mmxBlit(unsigned char* dest, int destPitch, unsigned char* src, int srcPitch, int width, int height);
void mmxBlitTrans(unsigned char* dest, int destPitch, unsigned char* src, int srcPitch, int width, int height);

#endif /* MMX_H */
