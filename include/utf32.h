#ifndef UTF32_H
#define UTF32_H

#include "kdgu.h"
#include "encoding.h"

#define READUTF32(X,Y)	  \
	((X == KDGU_ENDIAN_LITTLE) \
	 ? ((uint32_t)*(Y+3) << 24) | ((uint32_t)*(Y+2) << 16) \
	 | ((uint32_t)*(Y+1) << 8)  | ((uint32_t)*(Y)) \
	 : ((uint32_t)*(Y)   << 24) | ((uint32_t)*(Y+1) << 16) \
	 | ((uint32_t)*(Y+2) << 8)  | ((uint32_t)*(Y+3)))

uint8_t *utf32validate(kdgu *k,
                       const uint8_t *s,
                       size_t *l,
                       int endian);

#endif
