#ifndef UTF8_H
#define UTF8_H

#include "kdgu.h"
#include "encoding.h"

#define UTF8VALID(X) (X != 0xc0	\
                          && X != 0xc1	\
                          && X < 0xf5)

#define UTF8CONT(X) (((uint8_t)(X) & 0xc0) == 0x80)

uint8_t *utf8validate(kdgu *k, const uint8_t *s, size_t *l);
unsigned utf8chrlen(const uint8_t *s, unsigned l);

#endif
