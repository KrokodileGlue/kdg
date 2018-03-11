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

/*
 * TODO: Old versions of MSVC are missing lots of stuff. Maybe we
 * should set up some fallback typedefs.
 */

#ifndef KDGU_H
#define KDGU_H

#include <stdlib.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Manually disable warnings and errors about "unsafe" functions on
 * Windows.
 */
#ifdef _MSC_VER
#pragma warning(disable:4996)
#endif

#if defined(_MSC_VER) && defined(KDGU_DEBUG)
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

/* =============== Settings =============== */
/* #define KDGU_REPLACEMENT "\xef\xbf\xbd" */
#define KDGU_REPLACEMENT "?"

/* =============== API =============== */
enum kdgu_error {
	KDGU_ERR_INVALID
};

typedef struct kdgu {
	unsigned len;
	unsigned idx;
	char *s;
	unsigned char err;
} kdgu;

kdgu *kdgu_new(const char *s, size_t len);
bool kdgu_inc(kdgu *k);
bool kdgu_dec(kdgu *k);
void kdgu_print(const kdgu *k);
char *kdgu_nth(kdgu *k, int n);
void kdgu_pchr(const kdgu *k);

#ifdef __cplusplus
}
#endif

#ifdef KDGU_IMPLEMENTATION

size_t
utf8len(const char *s, size_t l)
{
	size_t j = 0;

	for (size_t i = 0; i < l; i++)
		if ((s[i] & 0xc0) != 0x80) j++;

	return j;
}

uint32_t
utf8inc(const char *s, int *_idx)
{
	int *idx = _idx;

	if (!_idx) {
		idx = malloc(sizeof *idx);
		*idx = 0;
	}

	int j = (*idx)++;
	while (s[*idx] && !((s[*idx] & 0xc0) != 0x80)) (*idx)++;
	uint32_t r = 0;
	for (int i = 0; i < *idx - j; i++)
		r |= (s[j + i] & 0xFF) << (i * 8);

	if (!_idx) free(idx);

	return r;
}

char *
utf8chrtostr(const char *s)
{
	if (!*s) return NULL;
	char *r = malloc(5);
	memcpy(r, (char *)&(uint32_t){ utf8inc(s, NULL) }, 4);
	r[4] = 0;
	return r;
}

#define ISVALIDBYTE(X) (!(X == 0xc0	\
                          || X == 0xc1	\
                          || (X >= 0xf5 && X <= 0xff)))

char *
utf8validate(const char *s, size_t *l)
{
	char *r = malloc(*l * strlen(KDGU_REPLACEMENT) + 1);
	*r = 0;
	unsigned idx = 0;

	for (unsigned i = 0; i < *l;) {
		/* Misplaced continuation byte. */
		if (((unsigned char)s[i] & 0xc0) == 0x80) {
			memcpy(r + idx, KDGU_REPLACEMENT,
			       strlen(KDGU_REPLACEMENT));
			idx += strlen(KDGU_REPLACEMENT);
			i++;
			continue;
		}

		if ((unsigned char)s[i] < 128
		    && ISVALIDBYTE((unsigned char)s[i])) {
			r[idx++] = s[i];
			i++;
			continue;
		}

		int len = -1;

		/*
		 * Count the number of 1 bits from the most
		 * significant bit to the first 0 bit in the current
		 * character.
		 */
		for (int j = 128; j; j /= 2) {
			if ((s[i] & j) == 0) break;
			if (len < 0) len = 0; else len++;
		}

		if (!ISVALIDBYTE((unsigned char)s[i])) goto err;
		if (len == -1 || len > 4) goto err;

		/*
		 * We're looking at the leading byte and we need to
		 * check the continuations.
		 */

		/* Make sure there are enough bytes. */
		if (i + len >= *l) goto err;

		/* Make sure they're all valid continuation bytes. */
		for (int j = 0; j < len; j++)
			if ((s[i + j + 1] & 0xc0) != 0x80)
				goto err;

		/* Now we need to check the ranges. */

#define RANGE(X,Y,Z) (((unsigned char)s[i + X] >= Y) && ((unsigned char)s[i + X] <= Z))
#define ASSERTRANGE(X,Y,Z) if (!RANGE(X,Y,Z)) { goto err; }
		if (RANGE(0, 0xc2, 0xdf))
			ASSERTRANGE(1, 0x80, 0xbf);
		if (RANGE(0, 0xe0, 0xe0)) {
			if (len != 2) goto err;
			ASSERTRANGE(1, 0xa0, 0xbf);
			ASSERTRANGE(2, 0x80, 0xbf);
		}
		if (RANGE(0, 0xe1, 0xec)) {
			if (len != 2) goto err;
			ASSERTRANGE(1, 0x80, 0xbf);
			ASSERTRANGE(2, 0x80, 0xbf);
		}
		if (RANGE(0, 0xed, 0xed)) {
			if (len != 2) goto err;
			ASSERTRANGE(1, 0x80, 0x9f);
			ASSERTRANGE(2, 0x80, 0xbf);
		}
		if (RANGE(0, 0xee, 0xef)) {
			if (len != 2) goto err;
			ASSERTRANGE(1, 0x80, 0xbf);
			ASSERTRANGE(2, 0x80, 0xbf);
		}
		if (RANGE(0, 0xf0, 0xf0)) {
			if (len != 3) goto err;
			ASSERTRANGE(1, 0x90, 0xbf);
			ASSERTRANGE(2, 0x80, 0xbf);
			ASSERTRANGE(3, 0x80, 0xbf);
		}
		if (RANGE(0, 0xf1, 0xf3)) {
			if (len != 3) goto err;
			ASSERTRANGE(1, 0x80, 0xbf);
			ASSERTRANGE(2, 0x80, 0xbf);
			ASSERTRANGE(3, 0x80, 0xbf);
		}
		if (RANGE(0, 0xf4, 0xf4)) {
			if (len != 3) goto err;
			ASSERTRANGE(1, 0x80, 0x8f);
			ASSERTRANGE(2, 0x80, 0xbf);
			ASSERTRANGE(3, 0x80, 0xbf);
		}

		/* Check for non-characters. */

		/* efb790 - efb7af */
		if (len == 2
		    && RANGE(0, 0xef, 0xef)
		    && RANGE(1, 0xb7, 0xb7)
		    && RANGE(2, 0x90, 0xaf))
			goto err;

		/* 0xefbfbe and 0xefbfbf */
		if (len == 2
		    && RANGE(0, 0xef, 0xef)
		    && RANGE(1, 0xbf, 0xbf)
		    && RANGE(2, 0xbe, 0xbf))
			goto err;

		if (len == 3) {
			uint32_t t = (unsigned char)s[i] << 24
				| (unsigned char)s[i + 1] << 16
				| (unsigned char)s[i + 2] << 8
				| (unsigned char)s[i + 3];

			/* f09fbfbe - f48fbfbe */
			if (0xf09fbfbe <= t && t <= 0xf48fbfbe)
				goto err;

			/* f09fbfbf - f48fbfbf */
			if (0xf09fbfbf <= t && t <= 0xf48fbfbf)
				goto err;
		}

		memcpy(r + idx, s + i, len + 1);
		idx += len + 1;
		i += len + 1;
		continue;

	err:
		i++;
		memcpy(r + idx, KDGU_REPLACEMENT, strlen(KDGU_REPLACEMENT));
		idx += strlen(KDGU_REPLACEMENT);
		while ((s[i] & 0xc0) == 0x80)
			if (i < *l) i++;
	}

	r[idx] = 0;
	*l = idx;
	return r;
}

kdgu *kdgu_new(const char *s, size_t len)
{
	kdgu *k = malloc(sizeof *k);
	memset(k, 0, sizeof *k);

	k->s = utf8validate(s, &len);
	k->len = len;

	return k;
}

void kdgu_free(kdgu *k)
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

char *
kdgu_nth(kdgu *k, int n)
{
	int i = 0;
	while (i < n && k->s[i])
		utf8inc(k->s, &i);
	return utf8chrtostr(k->s + i);
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

#endif /* ifdef KDGU_IMPLEMENTATION */
#endif /* ifndef KDGU_H */
