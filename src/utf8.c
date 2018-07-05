#include <assert.h>
#include <string.h>

#include "error.h"
#include "kdgu.h"
#include "utf8.h"
#include "unicode.h"

uint32_t
utf8decode(const uint8_t *s, unsigned l)
{
	uint32_t c;
	unsigned len = utf8chrlen(s, l);

	c = (*s & ((1 << (8 - len)) - 1))
		<< (len - 1) * 6;
	for (unsigned i = 1; i < len; i++)
		c |= (s[i] & 0x3F) << (len - i - 1) * 6;

	return c;
}

struct error
utf8encode(uint32_t c, uint8_t *buf, unsigned *len, int idx)
{
	struct error err = ERR(ERR_NO_ERROR, idx);

	if (c <= 0x7F) {
		*len = 1;
		buf[0] = c & 0xFF;
	} else if (c >= 0x80 && c <= 0x7FF) {
		*len = 2;
		buf[0] = 0xC0 | ((c >> 6) & 0x1F);
		buf[1] = 0x80 | (c & 0x3F);
	} else if (c >= 0x800 && c <= 0xFFFF) {
		*len = 3;
		buf[0] = 0xE0 | ((c >> 12) & 0xF);
		buf[1] = 0x80 | ((c >> 6) & 0x3F);
		buf[2] = 0x80 | (c & 0x3F);
	} else if (c >= 0x10000 && c <= 0x10FFFF) {
		*len = 4;
		buf[0] = 0xF0 | ((c >> 18) & 0x7);
		buf[1] = 0x80 | ((c >> 12) & 0x3F);
		buf[2] = 0x80 | ((c >> 6) & 0x3F);
		buf[3] = 0x80 | (c & 0x3F);
	} else ENCERR(utf8encode, buf, len, idx);

	return err;
}

unsigned
utf8chrlen(const uint8_t *s, unsigned l)
{
	unsigned i = 0;
	while (s++ && ++i < l && UTF8CONT(*s));
	return i;
}

#define RANGE(X,Y,Z)	  \
	(((uint8_t)s[X] >= Y) && ((uint8_t)s[X] <= Z))

#define ASSERTRANGE(X,Y,Z)	  \
	do { \
		if (RANGE(X,Y,Z)) break; \
		err = ERR(ERR_UTF8_INVALID_CONTRANGE, *i); \
		goto error; \
	} while (false)

#define CHECKLEN(X)	  \
	if (len != X) { \
		err = ERR(ERR_UTF8_RANGE_LENGTH_MISMATCH, *i); \
		goto error; \
	}

#define FAIL(X)	  \
	do { \
		err = ERR(X, *i); \
		goto error; \
	} while (false)

/*
 * TODO: Investigate whether non shortest-form sequences are being
 * caught.
 */

static struct error
utf8validatechar(const uint8_t *s,
		 uint8_t *r,
		 unsigned *i,
                 unsigned *idx,
		 size_t *l)
{
	struct error err = ERR(ERR_NO_ERROR, *i);
	/* Only for encoding the replacement character */
	unsigned buflen;

	/*
	 * Misplaced continuation byte. It's not a mistake that a
	 * separate error substitution is made for each one.
	 */

	if (UTF8CONT(s[*i])) {
		utf8encode(KDGU_REPLACEMENT, r + *idx, &buflen, *idx);
		(*idx) += buflen, (*i)++;
		return ERR(ERR_UTF8_STRAY_CONTINUATION_BYTE, *i);
	}

	/* This is just a regular ASCII character. */
	if ((uint8_t)s[*i] < 128 && UTF8VALID((uint8_t)s[*i])) {
		r[(*idx)++] = s[*i];
		(*i)++;
		return ERR(ERR_NO_ERROR, *i);
	}

	int len = -1;

	/*
	 * Count the number of 1 bits from the most significant bit to
	 * the first 0 bit in the current character.
	 */
	for (int j = 128; j; j /= 2) {
		if (!(s[*i] & j)) break;
		if (len < 0) len = 0; else len++;
	}

	if (!UTF8VALID((uint8_t)s[*i])) FAIL(ERR_UTF8_INVALID_BYTE);
	if (len < 0 || len > 4) FAIL(ERR_UTF8_INVALID_LENGTH);

	/*
	 * We're looking at the leading byte and we need to
	 * check the continuations.
	 */

	/*
	 * Make sure there are enough bytes left in the buffer for a
	 * sequence of the expected length.
	 */
	if (*i + len >= *l) FAIL(ERR_UTF8_INCORRECT_LENGTH);

	/* Make sure they're all valid continuation bytes. */
	for (int j = 0; j < len; j++) {
		if (!UTF8VALID((uint8_t)s[*i + j + 1]))
			FAIL(ERR_UTF8_INVALID_BYTE);

		if (!UTF8CONT(s[*i + j + 1]))
			FAIL(ERR_UTF8_MISSING_CONTINUATION);
	}

	/* Quickly check for noncharacters. */
	if (is_noncharacter(utf8decode(s + *i, *l - *i)))
		FAIL(ERR_NONCHARACTER);

	/* Now we need to check the ranges. */

	/*
	 * There is no rhyme or reason to these magic numbers. For
	 * information see page 126 (table 3-7) of
	 * https://goo.gl/kQZpon
	 */

	if (RANGE(*i, 0xc2, 0xdf)) {
		ASSERTRANGE(*i + 1, 0x80, 0xbf);
	} else if (RANGE(*i, 0xe0, 0xe0)) {
		CHECKLEN(2);
		ASSERTRANGE(*i + 1, 0xa0, 0xbf);
		ASSERTRANGE(*i + 2, 0x80, 0xbf);
	} else if (RANGE(*i, 0xe1, 0xec)) {
		CHECKLEN(2);
		ASSERTRANGE(*i + 1, 0x80, 0xbf);
		ASSERTRANGE(*i + 2, 0x80, 0xbf);
	} else if (RANGE(*i, 0xed, 0xed)) {
		CHECKLEN(2);
		ASSERTRANGE(*i + 1, 0x80, 0x9f);
		ASSERTRANGE(*i + 2, 0x80, 0xbf);
	} else if (RANGE(*i, 0xee, 0xef)) {
		CHECKLEN(2);
		ASSERTRANGE(*i + 1, 0x80, 0xbf);
		ASSERTRANGE(*i + 2, 0x80, 0xbf);
	} else if (RANGE(*i, 0xf0, 0xf0)) {
		CHECKLEN(3);
		ASSERTRANGE(*i + 1, 0x90, 0xbf);
		ASSERTRANGE(*i + 2, 0x80, 0xbf);
		ASSERTRANGE(*i + 3, 0x80, 0xbf);
	} else if (RANGE(*i, 0xf1, 0xf3)) {
		CHECKLEN(3);
		ASSERTRANGE(*i + 1, 0x80, 0xbf);
		ASSERTRANGE(*i + 2, 0x80, 0xbf);
		ASSERTRANGE(*i + 3, 0x80, 0xbf);
	} else if (RANGE(*i, 0xf4, 0xf4)) {
		CHECKLEN(3);
		ASSERTRANGE(*i + 1, 0x80, 0x8f);
		ASSERTRANGE(*i + 2, 0x80, 0xbf);
		ASSERTRANGE(*i + 3, 0x80, 0xbf);
	} else {
		err = ERR(ERR_UTF8_INVALID_RANGE, *i);
		goto error;
	}

	memcpy(r + *idx, s + *i, len + 1);
	*idx += len + 1;
	*i += len + 1;
	return ERR(ERR_NO_ERROR, *i);

 error:
	utf8encode(KDGU_REPLACEMENT, r + *idx, &buflen, *idx);
	(*idx) += buflen, (*i)++;
	/* Just skip over any continuation bytes. */
	while (*i < *l && UTF8CONT(s[*i])) (*i)++;

	return err;
}

uint8_t *
utf8validate(kdgu *k, const uint8_t *s, size_t *l)
{
	uint8_t *r = malloc(*l);
	if (!r) return NULL;

	unsigned idx = 0;

	/* Check for the UTF-8 BOM from 2.13 (Unicode Signature). */
	if (*l >= 3
	    && s[0] == (uint8_t)0xEF
	    && s[1] == (uint8_t)0xBB
	    && s[2] == (uint8_t)0xBF)
		s += 3;

	for (unsigned i = 0; i < *l;) {
		struct error err = utf8validatechar(s,
						    r,
						    &i,
						    &idx,
						    l);
		if (!err.kind) continue;
		if (!pusherror(k, err)) {
			free(r), free(r);
			return NULL;
		}
	}

	*l = idx;
	return r;
}
