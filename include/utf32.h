#ifndef UTF32_H
#define UTF32_H

#include "kdgu.h"
#include "encoding.h"

#define READUTF32(X,Y)	  \
	((X == ENDIAN_BIG) \
	 ? (uint8_t)*(Y)   << 24 | (uint8_t)*(Y+1) << 16 \
	 | (uint8_t)*(Y+2) << 8  | (uint8_t)*(Y+3) \
	 : (uint8_t)*(Y+3) << 24 | (uint8_t)*(Y+2) << 16 | \
	 (uint8_t)*(Y+1)   << 8  | (uint8_t)*(Y))

char *utf32validate(kdgu *k, const char *s, size_t *l, int endian);

#endif
