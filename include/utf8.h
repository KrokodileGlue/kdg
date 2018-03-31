#ifndef UTF8_H
#define UTF8_H

#include "kdgu.h"
#include "encoding.h"

#define UTF8VALID(X) (X != 0xc0	\
                          && X != 0xc1	\
                          && X < 0xf5)

#define UTF8CONT(X) (((unsigned char)(X) & 0xc0) == 0x80)

char *utf8validate(kdgu *k, const char *s, size_t *l);
unsigned utf8chrlen(const char *s, unsigned l);

#endif
