#ifndef UTF32_H
#define UTF32_H

#include "kdgu.h"
#include "encoding.h"

#define READUTF32(X)	  \
	((endian == ENDIAN_LITTLE) \
	 ? (uint8_t)*(X) << 24 | (uint8_t)*(X+1) << 16 \
	 | (uint8_t)*(X+2) << 8 | (uint8_t)*(X+3) \
	 : (uint8_t)*(X+3) << 24 | (uint8_t)*(X+2) << 16 | \
	 (uint8_t)*(X+1) << 8 | (uint8_t)*(X))

char *utf32validate(kdgu *k, const char *s, size_t *l, int endian);

#endif
