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
#define KDGU_REPLACEMENT "~"

/* =============== API =============== */
typedef struct kdgu_error {
	enum kdgu_errorcode {
		KDGU_ERR_NO_ERROR,
		KDGU_ERR_UTF8_STRAY_CONTINUATION_BYTE,
		KDGU_ERR_UTF8_INVALID_BYTE,
		KDGU_ERR_UTF8_MISSING_CONTINUATION,
		KDGU_ERR_UTF8_NONCHARACTER,
		KDGU_ERR_UTF8_INVALID_RANGE,
		KDGU_ERR_UTF8_INVALID_CONTRANGE,
		KDGU_ERR_UTF8_RANGE_LENGTH_MISMATCH,
		KDGU_ERR_UTF8_INVALID_LENGTH,
		KDGU_ERR_UTF8_INCORRECT_LENGTH,
		KDGU_ERR_INVALID_ASCII,
		KDGU_ERR_OUT_OF_MEMORY
	} kind;

	unsigned loc;
	char *msg;
} kdgu_error;

typedef struct kdgu {
	unsigned len, idx;
	char *s;

	struct kdgu_errorlist {
		struct kdgu_error *err;
		unsigned num;
	} *errlist;

	enum kdgu_fmt {
		KDGU_FMT_RAW,
		KDGU_FMT_ASCII,
		KDGU_FMT_UTF8
	} fmt;
} kdgu;

kdgu *kdgu_new(enum kdgu_fmt fmt, const char *s, size_t len);

bool kdgu_inc(kdgu *k);
bool kdgu_dec(kdgu *k);
bool kdgu_nth(kdgu *k, unsigned n);

kdgu *kdgu_getnth(kdgu *k, unsigned n);

bool kdgu_whitespace(const kdgu *k);

void kdgu_replace_substr(kdgu *k1, size_t a, size_t b, kdgu *k2);
void kdgu_print(const kdgu *k);
void kdgu_pchr(const kdgu *k);

size_t kdgu_len(const kdgu *k);
size_t kdgu_chomp(kdgu *k);

#ifdef KDGU_IMPLEMENTATION

/*
 * VOLATILE: This table must be kept up-to-date with the order of enum
 * kdgu_errorcode.
 */

static char *error[] = {
	"no error",

	"a continuation byte cannot be the start of a well-formed"
	" sequence",

	"impossible byte value cannot appear anywhere in well-formed"
	" UTF-8",

	"sequence initializer is followed by an unexpected number of"
	" continuation bytes",

	"sequence represents a noncharacter; noncharacters are"
	" permanently reserved for internal usage",

	"initializer byte is not in any of the possible ranges",

	"sequence contains continuation bytes that are outside of the"
	" valid range",

	"initializer byte indicates an impossible sequence length",

	"initializer byte indicates a length that is not valid for"
	" its range",

	"sequence actual length does not match the length declared"
	" by the initializer byte",

	"ASCII character is out-of-range",

	"kdgu has run out of memory; you should never see this"
};

#include <stdio.h>
#include <stdint.h>
#include <limits.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

_Static_assert(CHAR_BIT == 8,
               "kdgu only supports systems on which CHAR_BIT == 8 is true.");


#define IS_VALID_BYTE(X) (X != 0xc0	\
                          && X != 0xc1	\
                          && X < 0xf5)

#define IS_CONT_BYTE(X) (((unsigned char)(X) & 0xc0) == 0x80)

/*
 * These utf8 functions are internal, for use only on fully processed
 * kdgu strings. They do no validation of their input.
 */

static size_t
utf8len(const char *s, size_t l)
{
	size_t j = 0;

	for (size_t i = 0; i < l; i++)
		if (!IS_CONT_BYTE(s[i]))
			j++;

	return j;
}

static bool
utf8inc(const char *s, size_t l, unsigned *i)
{
	if (*i >= l) return false;
	do (*i)++; while (*i < l && IS_CONT_BYTE(s[*i]));
	return true;
}

static char *
utf8chrtostr(const char *s, size_t l)
{
	/* The maximum length for a UTF-8 character is four bytes. */
	if (!s) return NULL;

	char *r = malloc(5);
	if (!r) return NULL;

	unsigned i = 0;

	if (!utf8inc(s, l, &i)) return free(r), NULL;
	memcpy(r, s, i);
	r[i] = 0;

	return r;
}

static uint32_t
utf8chr(const char *s, size_t l)
{
	assert(l > 0 && l <= 4);
	uint32_t r = (unsigned char)s[0] << 24;

	for (unsigned i = 1; i < l && IS_CONT_BYTE(s[i]); i++)
		r |= (unsigned char)s[i] << (24 - i * 8);

	return r;
}

#define ERR(X,Y)	  \
	(struct kdgu_error){.kind = (X), .loc = (Y), .msg = error[X]}

#define RANGE(X,Y,Z)	  \
	(((unsigned char)s[X] >= Y) && ((unsigned char)s[X] <= Z))

#define ASSERTRANGE(X,Y,Z)	  \
	do { \
		if (RANGE(X,Y,Z)) break; \
		err = ERR(KDGU_ERR_UTF8_INVALID_CONTRANGE, *i); \
		goto error; \
	} while (false)

#define CHECKLEN(X)	  \
	if (len != X) { \
		err = ERR(KDGU_ERR_UTF8_RANGE_LENGTH_MISMATCH, *i); \
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
		err = ERR(KDGU_ERR_UTF8_INVALID_BYTE, *i); \
		goto error; \
	} while (false)

static struct kdgu_error
utf8validatechar(const char *s, char *r, unsigned *i,
                 unsigned *idx, size_t *l)
{
	struct kdgu_error err = ERR(KDGU_ERR_NO_ERROR, *i);

	/*
	 * Misplaced continuation byte. It's not a mistake that a
	 * separate error substitution is made for each one.
	 */

	if (IS_CONT_BYTE(s[*i])) {
		memcpy(r + *idx,
		       KDGU_REPLACEMENT,
		       strlen(KDGU_REPLACEMENT));

		*idx += strlen(KDGU_REPLACEMENT);
		(*i)++;
		return ERR(KDGU_ERR_UTF8_STRAY_CONTINUATION_BYTE, *i);
	}

	/* This is just a regular ASCII character. */
	if ((unsigned char)s[*i] < 128
	    && IS_VALID_BYTE((unsigned char)s[*i])) {
		r[(*idx)++] = s[*i];
		(*i)++;
		return ERR(KDGU_ERR_NO_ERROR, *i);
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

	if (!IS_VALID_BYTE((unsigned char)s[*i]))
		FAIL(KDGU_ERR_UTF8_INVALID_BYTE);

	if (len < 0 || len > 4)
		FAIL(KDGU_ERR_UTF8_INVALID_LENGTH);

	/*
	 * We're looking at the leading byte and we need to
	 * check the continuations.
	 */

	/*
	 * Make sure there are enough bytes left in the buffer for a
	 * sequence of the expected length.
	 */
	if (*i + len >= *l)
		FAIL(KDGU_ERR_UTF8_INCORRECT_LENGTH);

	/* Make sure they're all valid continuation bytes. */
	for (int j = 0; j < len; j++)
		if (!IS_CONT_BYTE(s[*i + j + 1]))
			FAIL(KDGU_ERR_UTF8_MISSING_CONTINUATION);

	/* Quickly check for noncharacters. */
	if (is_noncharacter(s + *i, len))
		FAIL(KDGU_ERR_UTF8_NONCHARACTER);

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
		err = ERR(KDGU_ERR_UTF8_INVALID_RANGE, *i);
		goto error;
	}

	memcpy(r + *idx, s + *i, len + 1);
	*idx += len + 1;
	*i += len + 1;
	return ERR(KDGU_ERR_NO_ERROR, *i);

 error:
	(*i)++;
	memcpy(r + *idx, KDGU_REPLACEMENT, strlen(KDGU_REPLACEMENT));
	*idx += strlen(KDGU_REPLACEMENT);

	/* Just skip over any continuation bytes. */
	while (*i < *l && IS_CONT_BYTE(s[*i]))
		(*i)++;

	return err;
}

static bool
pusherror(kdgu *k, struct kdgu_error err)
{
	if (!k->errlist) {
		k->errlist = malloc(sizeof *k->errlist);
		if (!k->errlist) return false;
		memset(k->errlist, 0, sizeof *k->errlist);
	}

	void *p = realloc(k->errlist->err,
	                  (k->errlist->num + 1)
	                  * sizeof *k->errlist->err);

	if (!p) {
		free(k->errlist->err);
		return false;
	}

	k->errlist->err = p;
	k->errlist->err[k->errlist->num++] = err;

	return true;
}

static char *
utf8validate(kdgu *k, const char *s, size_t *l)
{
	char *r = malloc(*l * strlen(KDGU_REPLACEMENT) + 1);
	if (!r) return NULL;

	*r = 0;
	unsigned idx = 0;

	/* Check for the UTF-8 BOM from 2.13 (Unicode Signature) */
	if (*l >= 3
	    && s[0] == (char)0xEF
	    && s[1] == (char)0xBB
	    && s[2] == (char)0xBF)
		s += 3;

	for (unsigned i = 0; i < *l;) {
		struct kdgu_error err =
			utf8validatechar(s, r, &i, &idx, l);

		if (!err.kind) continue;

		if (!pusherror(k, err)) {
			free(r), free(r);
			return NULL;
		}
	}

	r[idx] = 0;
	*l = idx;
	return r;
}

kdgu *
kdgu_new(enum kdgu_fmt fmt, const char *s, size_t len)
{
	kdgu *k = malloc(sizeof *k);
	if (!k) return NULL;

	memset(k, 0, sizeof *k);
	k->fmt = fmt;

	switch (fmt) {
	case KDGU_FMT_RAW:
		k->s = malloc(len);
		if (!k->s) return free(k), NULL;
		memcpy(k->s, s, len);
		k->len = len;
		break;

	case KDGU_FMT_ASCII:
		k->s = malloc(len);
		if (!k->s) return free(k), NULL;
		memcpy(k->s, s, len);
		k->len = len;

		for (unsigned i = 0; i < k->len; i++)
			if ((unsigned char)k->s[i] > 128)
				pusherror(k, ERR(KDGU_ERR_INVALID_ASCII, i));
		break;

	case KDGU_FMT_UTF8:
		k->s = utf8validate(k, s, &len);
		k->len = len;
		break;
	}

	if (!k->s) {
		free(k);
		return NULL;
	}

	return k;
}

void
kdgu_free(kdgu *k)
{
	if (!k) return;

	if (k->errlist) free(k->errlist->err);
	free(k->errlist);
	free(k->s);
	free(k);
}

bool
kdgu_inc(kdgu *k)
{
	if (k->idx >= k->len) return false;
	unsigned idx = k->idx;
	do {
		idx++;
	} while (idx < k->len && (k->s[idx] & 0xc0) == 0x80);
	if (idx == k->len) return false;
	k->idx = idx;
	return true;
}

bool
kdgu_dec(kdgu *k)
{
	if (!k) return false;

	if (k->idx == 0) return false;
	do {
		k->idx--;
	} while (k->idx && (k->s[k->idx] & 0xc0) == 0x80);
	return true;
}

bool
kdgu_nth(kdgu *k, unsigned n)
{
	if (!k) return false;

	k->idx = 0;
	for (unsigned i = 0; i < n; i++)
		if (!kdgu_inc(k)) return false;
	return true;
}

kdgu *
kdgu_getnth(kdgu *k, unsigned n)
{
	if (n >= k->len) return NULL;
	unsigned i = 0;
	kdgu *chr = NULL;

	switch (k->fmt) {
	case KDGU_FMT_RAW:
	case KDGU_FMT_ASCII: {
		char t[2] = { k->s[n], 0 };
		chr = kdgu_new(k->fmt, t, strlen(t));
	} break;

	case KDGU_FMT_UTF8:
		for (unsigned j = 0; j < n; j++)
			if (!utf8inc(k->s, k->len, &i))
				return NULL;

		char *t = utf8chrtostr(k->s + i, k->len - i);
		if (!t) return NULL;

		chr = kdgu_new(k->fmt, t, strlen(t));
		free(t);
		break;
	}

	return chr;
}

void
kdgu_print(const kdgu *k)
{
	if (!k) return;

	for (unsigned i = 0; i < k->len; i++)
		putchar(k->s[i]);
}

/* TODO: take FILE * and print to that. */
void
kdgu_pchr(const kdgu *k)
{
	if (!k) return;
	if (k->idx >= k->len) return;
	unsigned i = k->idx;

	switch (k->fmt) {
	case KDGU_FMT_RAW:
	case KDGU_FMT_ASCII:
		putchar(k->s[i]);
		break;

	case KDGU_FMT_UTF8:
		do putchar(k->s[i++]);
		while (i < k->len && IS_CONT_BYTE(k->s[i]));
		break;
	}
}

size_t
kdgu_len(const kdgu *k)
{
	if (!k) return -1;
	size_t l = -1;

	switch (k->fmt) {
	case KDGU_FMT_RAW:
	case KDGU_FMT_ASCII: l = k->len; break;
	case KDGU_FMT_UTF8: l = utf8len(k->s, k->len); break;
	}

	return l;
}

static char *asciiwhitespace[] = {
	"\x9", /* CHARACTER TABULATION */
	"\xa", /* LINE FEED            */
	"\xb", /* LINE TABULATION      */
	"\xc", /* FORM FEED            */
	"\xd", /* CARRIAGE RETURN      */
	"\x20" /* SPACE                */
};

static char *utf8whitespace[] = {
	"\x9",          /* CHARACTER TABULATION          */
	"\xa",          /* LINE FEED                     */
	"\xb",          /* LINE TABULATION               */
	"\xc",          /* FORM FEED                     */
	"\xd",          /* CARRIAGE RETURN               */
	"\x20",         /* SPACE                         */
	"\xc2\x85",     /* NEXT LINE                     */
	"\xc2\xa0",     /* NO-BREAK SPACE                */
	"\xe1\x9a\x80", /* OGHAM SPACE MARK              */
	"\xe1\xa0\x8e", /* MONGOLIAN VOWEL SEPARATOR     */
	"\xe2\x80\x80", /* EN QUAD                       */
	"\xe2\x80\x81", /* EM QUAD                       */
	"\xe2\x80\x82", /* EN SPACE                      */
	"\xe2\x80\x83", /* EM SPACE                      */
	"\xe2\x80\x84", /* THREE-PER-EM SPACE            */
	"\xe2\x80\x85", /* FOUR-PER-EM SPACE             */
	"\xe2\x80\x86", /* SIX-PER-EM SPACE              */
	"\xe2\x80\x87", /* FIGURE SPACE                  */
	"\xe2\x80\x88", /* PUNCTUATION SPACE             */
	"\xe2\x80\x89", /* THIN SPACE                    */
	"\xe2\x80\x8A", /* HAIR SPACE                    */
	"\xe2\x80\x8B", /* ZERO WIDTH SPACE              */
	"\xe2\x80\x8C", /* ZERO WIDTH NON-JOINER         */
	"\xe2\x80\x8D", /* ZERO WIDTH JOINER             */
	"\xe2\x80\xa8", /* LINE SEPARATOR                */
	"\xe2\x80\xa9", /* PARAGRAPH SEPARATOR           */
	"\xe2\x80\xaf", /* NARROW NO-BREAK SPACE         */
	"\xe2\x81\x9f", /* MEDIUM MATHEMATICAL SPACE     */
	"\xe2\x81\xa0", /* WORD JOINER                   */
	"\xe3\x80\x80", /* IDEOGRAPHIC SPACE             */
	"\xef\xbb\xbf"  /* ZERO WIDTH NON-BREAKING SPACE */
};

bool
kdgu_whitespace(const kdgu *k)
{
	char **ws = NULL;
	size_t len = -1;

	switch (k->fmt) {
	case KDGU_FMT_RAW:
	case KDGU_FMT_ASCII:
		ws = asciiwhitespace;
		len = sizeof asciiwhitespace
			/ sizeof *asciiwhitespace;
		break;

	case KDGU_FMT_UTF8:
		ws = utf8whitespace;
		len = sizeof utf8whitespace / sizeof *utf8whitespace;
		break;
	}

	for (unsigned i = 0; i < len; i++)
		if (strlen(ws[i]) <= k->len - k->idx
		    && !strncmp(k->s + k->idx, ws[i], strlen(ws[i])))
			return true;

	return false;
}

size_t
kdgu_chomp(kdgu *k)
{
	size_t r = 0;

	switch (k->fmt) {
	case KDGU_FMT_RAW:
	case KDGU_FMT_ASCII: {
		unsigned i = k->len - 1;
		while (i && isspace(k->s[i])) i--, r++;
		k->len = i;
	} break;

	case KDGU_FMT_UTF8: {
		/* We just need to preserve the idx. */
		unsigned idx = k->idx;

		while (kdgu_inc(k));
		while (kdgu_whitespace(k) && kdgu_dec(k));
		kdgu_inc(k);
		r = idx - k->idx;

		kdgu *tmp = kdgu_new(k->fmt, "", 0);
		assert(!tmp->errlist);
		kdgu_replace_substr(k, k->idx, k->len, tmp);
		kdgu_free(tmp);

		k->idx = idx;
	} break;
	}

	return r;
}

void
kdgu_replace_substr(kdgu *k1, size_t a, size_t b, kdgu *k2)
{
	assert(b >= a);

	/* TODO: make realloc/malloc/free customizable */
	void *p = realloc(k1->s, k1->len + k2->len);
	if (!p) return; /* TODO: report something somehow. */

	k1->s = p;

	memcpy(k1->s + a, k2->s, k2->len < b - a ? k2->len : b - a);
	k1->len = k1->len + k2->len - (b - a);
}

#endif /* ifdef KDGU_IMPLEMENTATION */
#endif /* ifndef KDGU_H */
