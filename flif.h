#ifndef _FLIF_H_
#define _FLIF_H_ 1

bool encode(const char* filename, const Image &image, int encoding);
bool decode(const char* filename, Image &image);



#endif

