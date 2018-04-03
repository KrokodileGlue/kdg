#include <assert.h>
#include <string.h>

#include "error.h"
#include "kdgu.h"
#include "utf8.h"

/* Not a decoder! Does not calculate code points! */
static uint32_t
utf8chr(const char *s, size_t l)
{
	assert(l > 0 && l <= 4);
	uint32_t r = (unsigned char)s[0] << 24;

	for (unsigned i = 1; i < l && UTF8CONT(s[i]); i++)
		r |= (unsigned char)s[i] << (24 - i * 8);

	return r;
}

unsigned
utf8chrlen(const char *s, unsigned l)
{
	unsigned i = 0;
	while (s++ && ++i < l && UTF8CONT(*s));
	return i;
}

#define RANGE(X,Y,Z)	  \
	(((unsigned char)s[X] >= Y) && ((unsigned char)s[X] <= Z))

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

static bool
is_noncharacter(const char *s, unsigned len)
{
	/*
	 * Refer to Section 2.4 (Code Points and Characters) and
	 * Section 23.7 (Noncharacters) for this stuff.
	 */

	/* efb790 to efb7af */
	if (len == 2
	    && RANGE(0, 0xef, 0xef)
	    && RANGE(1, 0xb7, 0xb7)
	    && RANGE(2, 0x90, 0xaf))
		return true;

	/* 0xefbfbe to 0xefbfbf */
	if (len == 2
	    && RANGE(0, 0xef, 0xef)
	    && RANGE(1, 0xbf, 0xbf)
	    && RANGE(2, 0xbe, 0xbf))
		return true;

	if (len == 3) {
		uint32_t t = utf8chr(s, 4);

		/* f09fbfbe to f48fbfbe */
		if (0xf09fbfbe <= t && t <= 0xf48fbfbe)
			return true;

		/* f09fbfbf to f48fbfbf */
		if (0xf09fbfbf <= t && t <= 0xf48fbfbf)
			return true;
	}

	return false;
}

#define FAIL(X)	  \
	do { \
		err = ERR(X, *i); \
		goto error; \
	} while (false)

static struct error
utf8validatechar(const char *s, char *r, unsigned *i,
                 unsigned *idx, size_t *l)
{
	struct error err = ERR(ERR_NO_ERROR, *i);

	/*
	 * Misplaced continuation byte. It's not a mistake that a
	 * separate error substitution is made for each one.
	 */

	if (UTF8CONT(s[*i])) {
		r[(*idx)++] = KDGU_REPLACEMENT;
		(*i)++;
		return ERR(ERR_UTF8_STRAY_CONTINUATION_BYTE, *i);
	}

	/* This is just a regular ASCII character. */
	if ((unsigned char)s[*i] < 128
	    && UTF8VALID((unsigned char)s[*i])) {
		r[(*idx)++] = s[*i];
		(*i)++;
		return ERR(ERR_NO_ERROR, *i);
	}

	int len = -1;

	/*
	 * Count the number of 1 bits from the most
	 * significant bit to the first 0 bit in the current
	 * character.
	 */
	for (int j = 128; j; j /= 2) {
		if (!(s[*i] & j)) break;
		if (len < 0) len = 0; else len++;
	}

	if (!UTF8VALID((unsigned char)s[*i]))
		FAIL(ERR_UTF8_INVALID_BYTE);

	if (len < 0 || len > 4)
		FAIL(ERR_UTF8_INVALID_LENGTH);

	/*
	 * We're looking at the leading byte and we need to
	 * check the continuations.
	 */

	/*
	 * Make sure there are enough bytes left in the buffer for a
	 * sequence of the expected length.
	 */
	if (*i + len >= *l)
		FAIL(ERR_UTF8_INCORRECT_LENGTH);

	/* Make sure they're all valid continuation bytes. */
	for (int j = 0; j < len; j++) {
		if (!UTF8VALID((unsigned char)s[*i + j + 1]))
			FAIL(ERR_UTF8_INVALID_BYTE);

		if (!UTF8CONT(s[*i + j + 1]))
			FAIL(ERR_UTF8_MISSING_CONTINUATION);
	}

	/* Quickly check for noncharacters. */
	if (is_noncharacter(s + *i, len))
		FAIL(ERR_UTF8_NONCHARACTER);

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
	(*i)++;
	r[(*idx)++] = KDGU_REPLACEMENT;

	/* Just skip over any continuation bytes. */
	while (*i < *l && UTF8CONT(s[*i]))
		(*i)++;

	return err;
}

char *
utf8validate(kdgu *k, const char *s, size_t *l)
{
	char *r = malloc(*l);
	if (!r) return NULL;

	unsigned idx = 0;

	/* Check for the UTF-8 BOM from 2.13 (Unicode Signature). */
	if (*l >= 3
	    && s[0] == (char)0xEF
	    && s[1] == (char)0xBB
	    && s[2] == (char)0xBF)
		s += 3;

	for (unsigned i = 0; i < *l;) {
		struct error err =
			utf8validatechar(s, r, &i, &idx, l);

		if (!err.kind) continue;

		if (!pusherror(k, err)) {
			free(r), free(r);
			return NULL;
		}
	}

	*l = idx;
	return r;
}
