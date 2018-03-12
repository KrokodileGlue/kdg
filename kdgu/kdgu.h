/*
 * MIT License
 *
 * Copyright (c) 2017 "KrokodileGlue" (https://github.com/KrokodileGlue)
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef KDGU_H
#define KDGU_H

#include <stdlib.h>
#include <stdbool.h>

/*
 * Manually disable errors about "unsafe" functions on Windows.
 */
#ifdef _MSC_VER
#pragma warning(disable:4996)
#endif

/* =============== Settings =============== */
#define KDGU_REPLACEMENT "?"

/* =============== API =============== */
enum kdgu_error {
	KDGU_ERR_NO_ERROR                =      0,
	KDGU_ERR_STRAY_CONTINUATION_BYTE = 1 << 0,
	KDGU_ERR_INVALID_BYTE            = 1 << 1,
	KDGU_ERR_OUT_OF_MEMORY           = 1 << 2
};

typedef struct kdgu {
	unsigned len;
	unsigned idx;
	unsigned errloc;
	char *s;
	unsigned char err;
} kdgu;

kdgu *kdgu_new(const char *s, size_t len, bool recover);

bool kdgu_inc(kdgu *k);
bool kdgu_dec(kdgu *k);
bool kdgu_nth(kdgu *k, unsigned n);

bool kdgu_whitespace(const kdgu *k);

void kdgu_setsubstr(kdgu *k1, size_t a, size_t b, kdgu *k2);
void kdgu_print(const kdgu *k);
void kdgu_pchr(const kdgu *k);

size_t kdgu_len(const kdgu *k);
size_t kdgu_chomp(kdgu *k);

#ifdef KDGU_IMPLEMENTATION

#include <stdio.h>
#include <stdint.h>
#include <limits.h>
#include <string.h>
#include <assert.h>

_Static_assert(CHAR_BIT == 8,
               "kdgu only supports systems on which CHAR_BIT == 8 is true.");

static size_t
utf8len(const char *s, size_t l)
{
	size_t j = 0;

	for (size_t i = 0; i < l; i++)
		if ((s[i] & 0xc0) != 0x80) j++;

	return j;
}

static bool
utf8inc(const char *s, size_t l, unsigned *i)
{
	if (l >= *i) return false;
	while (*i < l && !((s[*i] & 0xc0) != 0x80)) (*i)++;
	return true;
}

static char *
utf8chrtostr(const char *s, size_t l)
{
	/* The maximum length for a UTF-8 character is four bytes. */
	char *r = malloc(5);
	if (!r) return NULL;
	unsigned i = 0;

	utf8inc(s, l, &i);
	memcpy(r, s, i);
	r[i] = 0;

	return r;
}

static uint32_t
utf8chr(const char *s, size_t l)
{
	assert(l > 0 && l <= 4);
	uint32_t r = (unsigned char)s[0] << 24;

	for (unsigned i = 1; i < l && (s[i] & 0xc0) == 0x80; i++)
		r |= (unsigned char)s[i] << (24 - i * 8);

	return r;
}

#define IS_VALID_BYTE(X) (!(X == 0xc0	\
                          || X == 0xc1	\
                          || (X >= 0xf5 && X <= 0xff)))

#define IS_CONT_BYTE(X) (((X) & 0xc0) == 0x80)

#define RANGE(X,Y,Z)	  \
	(((unsigned char)s[X] >= Y) && ((unsigned char)s[X] <= Z))

#define ASSERTRANGE(X,Y,Z)	  \
	do { \
		if (!RANGE(X,Y,Z)) goto err; \
	} while (false)

static bool
is_noncharacter(const char *s, unsigned len)
{
	/*
	 * Refer to Section 2.4 (Code Points and Characters) and
	 * Section 23.7 (Noncharacters) for details on this stuff.
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

static enum kdgu_error
utf8validatechar(const char *s, char *r, unsigned *i, unsigned *idx, size_t *l)
{
	enum kdgu_error err = KDGU_ERR_NO_ERROR;

	/* Misplaced continuation byte. */
	if (IS_CONT_BYTE((unsigned char)s[*i])) {
		memcpy(r + *idx,
		       KDGU_REPLACEMENT,
		       strlen(KDGU_REPLACEMENT));

		*idx += strlen(KDGU_REPLACEMENT);
		(*i)++;
		return KDGU_ERR_STRAY_CONTINUATION_BYTE;
	}

	/* This is just a regular ASCII character. */
	if ((unsigned char)s[*i] < 128
	    && IS_VALID_BYTE((unsigned char)s[*i])) {
		r[(*idx)++] = s[*i];
		(*i)++;
		return KDGU_ERR_NO_ERROR;
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

	if (!IS_VALID_BYTE((unsigned char)s[*i])) goto err;
	if (len < 0 || len > 4) goto err;

	/*
	 * We're looking at the leading byte and we need to
	 * check the continuations.
	 */

	/*
	 * Make sure there are enough bytes left in the buffer for a
	 * sequence of the expected length.
	 */
	if (*i + len >= *l) goto err;

	/* Make sure they're all valid continuation bytes. */
	for (int j = 0; j < len; j++)
		if (!IS_CONT_BYTE((unsigned char)s[*i + j + 1]))
			goto err;

	/* Now we need to check the ranges. */

	/*
	 * There is no rhyme or reason to these magic numbers. See
	 * page 126 (table 3-7) of
	 * http://www.unicode.org/versions/Unicode10.0.0/UnicodeStandard-10.0.pdf
	 * for information.
	 */

	if (RANGE(*i, 0xc2, 0xdf))
		ASSERTRANGE(*i + 1, 0x80, 0xbf);

	if (RANGE(*i, 0xe0, 0xe0)) {
		if (len != 2) goto err;
		ASSERTRANGE(*i + 1, 0xa0, 0xbf);
		ASSERTRANGE(*i + 2, 0x80, 0xbf);
	}

	if (RANGE(*i, 0xe1, 0xec)) {
		if (len != 2) goto err;
		ASSERTRANGE(*i + 1, 0x80, 0xbf);
		ASSERTRANGE(*i + 2, 0x80, 0xbf);
	}

	if (RANGE(*i, 0xed, 0xed)) {
		if (len != 2) goto err;
		ASSERTRANGE(*i + 1, 0x80, 0x9f);
		ASSERTRANGE(*i + 2, 0x80, 0xbf);
	}

	if (RANGE(*i, 0xee, 0xef)) {
		if (len != 2) goto err;
		ASSERTRANGE(*i + 1, 0x80, 0xbf);
		ASSERTRANGE(*i + 2, 0x80, 0xbf);
	}

	if (RANGE(*i, 0xf0, 0xf0)) {
		if (len != 3) goto err;
		ASSERTRANGE(*i + 1, 0x90, 0xbf);
		ASSERTRANGE(*i + 2, 0x80, 0xbf);
		ASSERTRANGE(*i + 3, 0x80, 0xbf);
	}

	if (RANGE(*i, 0xf1, 0xf3)) {
		if (len != 3) goto err;
		ASSERTRANGE(*i + 1, 0x80, 0xbf);
		ASSERTRANGE(*i + 2, 0x80, 0xbf);
		ASSERTRANGE(*i + 3, 0x80, 0xbf);
	}

	if (RANGE(*i, 0xf4, 0xf4)) {
		if (len != 3) goto err;
		ASSERTRANGE(*i + 1, 0x80, 0x8f);
		ASSERTRANGE(*i + 2, 0x80, 0xbf);
		ASSERTRANGE(*i + 3, 0x80, 0xbf);
	}

	if (is_noncharacter(s + *i, len)) goto err;

	memcpy(r + *idx, s + *i, len + 1);
	*idx += len + 1;
	*i += len + 1;
	return KDGU_ERR_NO_ERROR;

 err:
	(*i)++;
	memcpy(r + *idx, KDGU_REPLACEMENT, strlen(KDGU_REPLACEMENT));
	*idx += strlen(KDGU_REPLACEMENT);

	/* Just skip over any continuation bytes. */
	while (*i < *l && IS_CONT_BYTE((unsigned char)s[*i])) (*i)++;
	return err;
}

static char *
utf8validate(kdgu *k, const char *s, size_t *l, bool recover)
{
	char *r = malloc(*l * strlen(KDGU_REPLACEMENT) + 1);
	*r = 0;
	unsigned idx = 0;

	/* Check for the UTF-8 BOM from 2.13 (Unicode Signature) */
	if (*l >= 3
	    && s[0] == (char)0xEF
	    && s[1] == (char)0xBB
	    && s[2] == (char)0xBF)
		s += 3;

	for (unsigned i = 0; i < *l;) {
		enum kdgu_error err = utf8validatechar(s, r, &i, &idx, l);

		if (!recover && !k->err) {
			k->err = err;
			k->errloc = i;
		}

		if (recover) k->err |= err;
	}

	r[idx] = 0;
	*l = idx;
	return r;
}

kdgu *kdgu_new(const char *s, size_t len, bool recover)
{
	kdgu *k = malloc(sizeof *k);
	memset(k, 0, sizeof *k);

	k->s = utf8validate(k, s, &len, recover);
	k->len = len;

	return k;
}

void
kdgu_free(kdgu *k)
{
	free(k->s);
	free(k);
}

bool
kdgu_inc(kdgu *k)
{
	if (k->idx >= k->len) return false;
	do {
		k->idx++;
	} while (k->idx < k->len && (k->s[k->idx] & 0xc0) == 0x80);
	return true;
}

bool
kdgu_dec(kdgu *k)
{
	if (k->idx == 0) return false;
	do {
		k->idx--;
	} while (k->idx && (k->s[k->idx] & 0xc0) == 0x80);
	return true;
}

bool
kdgu_nth(kdgu *k, unsigned n)
{
	k->idx = 0;
	for (unsigned i = 0; i < n; i++)
		if (!kdgu_inc(k)) return false;
	return true;
}

void
kdgu_print(const kdgu *k)
{
	for (unsigned i = 0; i < k->len; i++)
		putchar(k->s[i]);
}

void
kdgu_pchr(const kdgu *k)
{
	unsigned i = k->idx;
	if (i >= k->len) return;
	do {
		putchar(k->s[i++]);
	} while (i < k->len && (k->s[i] & 0xc0) == 0x80);
}

size_t
kdgu_len(const kdgu *k)
{
	return utf8len(k->s, k->len);
}

bool kdgu_whitespace(const kdgu *k)
{
	return false;
}

size_t
kdgu_chomp(kdgu *k)
{
	/* We just need to preserve the idx. */
	unsigned idx = k->idx;

	while (kdgu_inc(k));
	while (kdgu_whitespace(k)) kdgu_dec(k);
	unsigned r = idx - k->idx;

	kdgu *tmp = kdgu_new("", 0, false);
	assert(!tmp->err);
	kdgu_setsubstr(k, k->idx, k->len, tmp);
	kdgu_free(tmp);

	k->idx = idx;
	return r;
}

void
kdgu_setsubstr(kdgu *k1, size_t a, size_t b, kdgu *k2)
{
	assert(b >= a);

	/* TODO: make realloc/malloc/free customizable */
	void *p = realloc(k1->s, k1->len - (b - a) + k2->len);

	if (!p) {
		k1->err |= KDGU_ERR_OUT_OF_MEMORY;
		return;
	}

	k1->s = p;

	memmove(k1->s + b, k1->s + a, b - a);
	memcpy(k1->s + a, k2->s, b - a);
}

#endif /* ifdef KDGU_IMPLEMENTATION */
#endif /* ifndef KDGU_H */
