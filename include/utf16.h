#ifndef UTF16_H
#define UTF16_H

#include "kdgu.h"
#include "encoding.h"

#define READUTF16(X)	  \
	((endian == ENDIAN_LITTLE) \
	 ? (uint8_t)*(X)   << 8 | (uint8_t)*(X+1) \
	 : (uint8_t)*(X+1) << 8 | (uint8_t)*(X))

#define UTF16HIGH_SURROGATE(X)	  \
	((X) >= 0xd800 && (X) <= 0xdbff)

#define UTF16LOW_SURROGATE(X)	  \
	((X) >= 0xdc00 && (X) <= 0xdfff)

char *utf16validate(kdgu *k, const char *s,
                    size_t *l, int endian);

#endif
