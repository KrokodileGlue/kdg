#ifndef UTF16_H
#define UTF16_H

#include "kdgu.h"
#include "encoding.h"

#define READUTF16(X,Y)	  \
	((X == ENDIAN_LITTLE) \
	 ? (uint8_t)*(Y)   << 8 | (uint8_t)*(Y+1) \
	 : (uint8_t)*(Y+1) << 8 | (uint8_t)*(Y))

#define UTF16HIGH_SURROGATE(X)	  \
	((X) >= 0xd800 && (X) <= 0xdbff)

#define UTF16LOW_SURROGATE(X)	  \
	((X) >= 0xdc00 && (X) <= 0xdfff)

uint8_t *utf16validate(kdgu *k, const uint8_t *s,
                       size_t *l, int endian);

#endif
