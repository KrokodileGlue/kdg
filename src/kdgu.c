#include "kdgu.h"
#include "unicode.h"

#include <stdint.h>
#include <limits.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <inttypes.h>

#define KDGU_REPLACEMENT '?'

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

	"buffer contains a trailing byte",

	"sequences contains an invalid low surrogate byte",

	"invalid byte in CP1252",

	"ASCII character is out-of-range",

	"no representation for character U+%02"PRIX32
	" in encoding '%s'",

	"kdgu has run out of memory; you should never see this"
};

/*
 * VOLATILE: This table must be kept up-to-date with the order of enum
 * kdgu_fmt.
 */
static char *format[] = {
	"CP1252",
	"EBCDIC",
	"ASCII",
	"UTF-8",
	"UTF-16",
	"UTF-16BE",
	"UTF-16LE"
};

_Static_assert(CHAR_BIT == 8,
               "kdgu only supports systems on"
               " which CHAR_BIT == 8 is true.");

#define ERR(X,Y)	  \
	(struct kdgu_error){.kind = (X), .loc = (Y), .msg = error[X]}

#define UTF8VALID(X) (X != 0xc0	\
                          && X != 0xc1	\
                          && X < 0xf5)

#define UTF8CONT(X) (((unsigned char)(X) & 0xc0) == 0x80)

#define READUTF16(X)	  \
	((endian == ENDIAN_LITTLE) \
	 ? (uint8_t)*(X)   << 8 | (uint8_t)*(X+1) \
	 : (uint8_t)*(X+1) << 8 | (uint8_t)*(X))

#define UTF16HIGH_SURROGATE(X)	  \
	((X) >= 0xd800 && (X) <= 0xdbff)

#define UTF16LOW_SURROGATE(X)	  \
	((X) >= 0xdc00 && (X) <= 0xdfff)

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

static unsigned
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
		err = ERR(X, *i); \
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

	if (UTF8CONT(s[*i])) {
		r[(*idx)++] = KDGU_REPLACEMENT;
		(*i)++;
		return ERR(KDGU_ERR_UTF8_STRAY_CONTINUATION_BYTE, *i);
	}

	/* This is just a regular ASCII character. */
	if ((unsigned char)s[*i] < 128
	    && UTF8VALID((unsigned char)s[*i])) {
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

	if (!UTF8VALID((unsigned char)s[*i]))
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
	for (int j = 0; j < len; j++) {
		if (!UTF8VALID((unsigned char)s[*i + j + 1]))
			FAIL(KDGU_ERR_UTF8_INVALID_BYTE);

		if (!UTF8CONT(s[*i + j + 1]))
			FAIL(KDGU_ERR_UTF8_MISSING_CONTINUATION);
	}

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
	r[(*idx)++] = KDGU_REPLACEMENT;

	/* Just skip over any continuation bytes. */
	while (*i < *l && UTF8CONT(s[*i]))
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
		struct kdgu_error err =
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

static struct kdgu_error
utf16validatechar(const char *s, char *r, unsigned *i,
                  unsigned *idx, size_t buflen, int endian)
{
	struct kdgu_error err = ERR(KDGU_ERR_NO_ERROR, *i);

	if (buflen - *i < 2) {
		if (buflen - *i == 1) (*i)++;
		return ERR(KDGU_ERR_UTF16_EOS, *i);
	}

	uint16_t c = READUTF16(s + *i);

	/* It's not in the surrogate pair range. */
	if (!UTF16HIGH_SURROGATE(c)) {
		memcpy(r + *idx, &c, sizeof c);
		*idx += 2;
		*i += 2;
		return err;
	}

	if (buflen - *i < 4)
		return (*i) += 2, ERR(KDGU_ERR_UTF16_EOS, *i);

	/* It's in the surrogate pair range. */
	uint16_t c2 = READUTF16(s + *i + 2);
	*i += 4;

	if (!UTF16LOW_SURROGATE(c2)) {
		r[(*idx)++] = KDGU_REPLACEMENT;
		return ERR(KDGU_ERR_UTF16_MISSING_SURROGATE, *i);
	}

	memcpy(r + *idx, &c, sizeof c);
	memcpy(r + *idx + 2, &c2, sizeof c2);

	*idx += 4;

	return err;
}

static char *
utf16validate(kdgu *k, const char *s, size_t *l, int endian)
{
	char *r = malloc(*l);
	if (!r) return NULL;

	unsigned idx = 0;
	size_t buflen = *l;

	/*
	 * Page 41 table 2-4 indicates that the BOM should not appear
	 * in in the UTF-16BE or UTF-16LE encodings. Simply ignoring
	 * it will cause it (if it is present) to be interpreted as a
	 * zero width space in utf16validatechar.
	 */

	if (!endian && buflen >= 2) {
		/* Check the BOM. */
		uint16_t c = (uint8_t)s[1] << 8
			| (uint8_t)s[0];

		if (c == (uint16_t)0xFFFE) {
			endian = ENDIAN_LITTLE;
			s += 2;
			buflen -= 2;
		} else if (c == (uint16_t)0xFEFF) {
			endian = ENDIAN_BIG;
			s += 2;
			buflen -= 2;
		}
	}

	for (unsigned i = 0; i < buflen;) {
		struct kdgu_error err =
			utf16validatechar(s, r, &i,
			                  &idx, buflen, endian);

		if (!err.kind) continue;

		if (!pusherror(k, err)) {
			free(r), free(r);
			return NULL;
		}
	}

	*l = idx;
	return r;
}

#define IS_VALID_CP1252(X)	  \
	((X) == 0x81 \
	 || (X) == 0x8D \
	 || (X) == 0x8F \
	 || (X) == 0x90 \
	 || (X) == 0x9D)

static char *
cp1252validate(kdgu *k, const char *s, size_t *l)
{
	char *r = malloc(*l);
	if (!r) return NULL;
	memcpy(r, s, *l);

	/*
	 * Some bytes are never used!
	 * https://en.wikipedia.org/wiki/Windows-1252
	 */
	for (unsigned i = 0; i < k->len; i++)
		if (!IS_VALID_CP1252((unsigned char)r[i])) {
			pusherror(k, ERR(KDGU_ERR_INVALID_CP1252, i));
			r[i] = KDGU_REPLACEMENT;
		}

	return r;
}

static char *
asciivalidate(kdgu *k, const char *s, size_t *l)
{
	char *r = malloc(*l);
	if (!r) return NULL;
	memcpy(r, s, *l);

	for (unsigned i = 0; i < k->len; i++)
		if ((unsigned char)s[i] > 128)
			pusherror(k, ERR(KDGU_ERR_INVALID_ASCII, i));

	return r;
}

static char *
ebcdicvalidate(kdgu *k, const char *s, size_t *l)
{
	char *r = malloc(*l);
	if (!r) return NULL;
	memcpy(r, s, *l);

	for (unsigned i = 0; i < k->len; i++)
		if ((unsigned char)s[i] == 48
		    || (unsigned char)s[i] == 49)
			pusherror(k, ERR(KDGU_ERR_INVALID_EBCDIC, i));

	return r;
}

kdgu *
kdgu_new(enum kdgu_fmt fmt, const char *s, size_t len)
{
	kdgu *k = malloc(sizeof *k);
	if (!k) return NULL;

	memset(k, 0, sizeof *k);
	k->fmt = fmt;

	int endian = ENDIAN_NONE;

	switch (fmt) {
	case KDGU_FMT_CP1252:
		k->s = cp1252validate(k, s, &len);
		break;

	case KDGU_FMT_ASCII:
		k->s = asciivalidate(k, s, &len);
		break;

	case KDGU_FMT_EBCDIC:
		k->s = ebcdicvalidate(k, s, &len);
		break;

	case KDGU_FMT_UTF8:
		k->s = utf8validate(k, s, &len);
		break;

	case KDGU_FMT_UTF16BE: if (!endian) endian = ENDIAN_BIG;
	case KDGU_FMT_UTF16LE: if (!endian) endian = ENDIAN_LITTLE;
	case KDGU_FMT_UTF16:
		if (!endian) endian = ENDIAN_BIG;
		k->s = utf16validate(k, s, &len, endian);
		k->endian = endian;
		break;
	}

	if (!k->s) {
		free(k);
		return NULL;
	}

	k->len = len;
	k->alloc = k->len;

	return k;
}

kdgu *
kdgu_news(const char *s)
{
	return kdgu_new(KDGU_FMT_ASCII, s, strlen(s));
}

kdgu *
kdgu_copy(kdgu *k)
{
	kdgu *r = malloc(sizeof *r);
	if (!r) return NULL;
	memset(r, 0, sizeof *r);

	r->endian = k->endian;
	r->fmt = k->fmt;
	r->len = k->len;
	r->idx = k->idx;
	r->alloc = k->len;
	r->s = malloc(k->len);

	if (!r->s) return free(r), NULL;
	memcpy(r->s, k->s, k->len);

	return r;
}

bool
kdgu_cat(kdgu *k1, kdgu *k2)
{
	if (!k1 || !k2 || k1->fmt != k2->fmt) return NULL;

	k1->alloc += k2->len;
	char *p = realloc(k1->s, k1->alloc);
	if (!p) return false;

	k1->s = p;
	memcpy(k1->s + k1->len, k2->s, k2->len);
	k1->len += k2->len;

	return true;
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

unsigned
kdgu_inc(kdgu *k)
{
	if (!k) return 0;
	if (k->idx >= k->len) return 0;

	unsigned idx = k->idx;
	int endian = ENDIAN_NONE;

	switch (k->fmt) {
	case KDGU_FMT_CP1252:
	case KDGU_FMT_EBCDIC:
	case KDGU_FMT_ASCII:
		idx++;
		break;

	case KDGU_FMT_UTF8:
		do {
			idx++;
		} while (idx < k->len && UTF8CONT(k->s[idx]));
		break;

	case KDGU_FMT_UTF16BE: endian = ENDIAN_BIG;
	case KDGU_FMT_UTF16LE: if (!endian) endian = ENDIAN_LITTLE;
	case KDGU_FMT_UTF16: {
		if (UTF16HIGH_SURROGATE(READUTF16(k->s + idx)))
			idx += 4;
		else idx += 2;
	} break;
	}

	if (idx >= k->len) return 0;

	unsigned r = idx - k->idx;
	k->idx = idx;

	return r;
}

unsigned
kdgu_dec(kdgu *k)
{
	if (!k || !k->idx) return 0;

	unsigned idx = k->idx;
	int endian = ENDIAN_NONE;

	switch (k->fmt) {
	case KDGU_FMT_CP1252:
	case KDGU_FMT_EBCDIC:
	case KDGU_FMT_ASCII:
		idx--;
		break;

	case KDGU_FMT_UTF8:
		do {
			idx--;
		} while (idx && UTF8CONT(k->s[idx]));
		break;

	case KDGU_FMT_UTF16BE: if (!endian) endian = ENDIAN_BIG;
	case KDGU_FMT_UTF16LE: if (!endian) endian = ENDIAN_LITTLE;
	case KDGU_FMT_UTF16: {
		if (UTF16LOW_SURROGATE(READUTF16(k->s + idx - 2)))
			idx -= 4;
		else idx -= 2;
	} break;
	}

	unsigned r = k->idx - idx;
	k->idx = idx;

	return r;
}

bool
kdgu_nth(kdgu *k, unsigned n)
{
	if (!k) return false;

	/* TODO: UTF-32 */
	if (k->fmt == KDGU_FMT_ASCII
	    || k->fmt == KDGU_FMT_EBCDIC
	    || k->fmt == KDGU_FMT_CP1252) {
		if (n >= k->len) return false;
		k->idx = n;
		return true;
	}

	k->idx = 0;
	for (unsigned i = 0; i < n; i++)
		if (!kdgu_inc(k))
			return false;

	return true;
}

static int
overwritechr(kdgu *k, char *b, unsigned l1)
{
	unsigned l2 = kdgu_chrsize(k);

	if (l1 == l2) {
		memcpy(k->s + k->idx, b, l1);
		return 0;
	} else if (l1 > l2) {
		kdgu_size(k, k->len + l1 - l2);
		memmove(k->s + k->idx + l1,
		        k->s + k->idx + l2,
		        k->len - k->idx - l2);
		memcpy(k->s + k->idx, b, l1);
		k->len += l1 - l2;
		return l1 - l2;
	} else if (l1 < l2) {
		kdgu_size(k, k->len + l2 - l1);
		memmove(k->s + k->idx + l1,
		        k->s + k->idx + l2,
		        k->len - k->idx - l2);
		memcpy(k->s + k->idx, b, l1);
		k->len -= l2 - l1;
		return l1 - l2;
	}

	/* This should never happen. */
	assert(false);
}

kdgu *
kdgu_getnth(kdgu *k, unsigned n)
{
	if (!kdgu_nth(k, n)) return NULL;

	kdgu *chr = kdgu_new(k->fmt,
	                     k->s + k->idx,
	                     kdgu_chrsize(k));

	return chr;
}

bool
kdgu_convert(kdgu *k, enum kdgu_fmt fmt)
{
	if (!k) return false;
	if (!k->len || k->fmt == fmt) {
		k->fmt = fmt;
		return true;
	}

	unsigned l;
	k->idx = 0;

	while (true) {
		uint32_t c = kdgu_decode(k);
		char buf[4];

		struct kdgu_error err =
			kdgu_encode(fmt, c, buf, &l, k->idx);

		if (err.kind) {
			err.codepoint = c;
			err.data = format[fmt];
			pusherror(k, err);
		}

		overwritechr(k, buf, l);
		k->idx += l;

		if (k->idx >= k->len) break;
	}

	k->fmt = fmt;
	return true;
}

bool
kdgu_cmp(kdgu *k1, kdgu *k2)
{
	if (!k1 || !k2 || kdgu_len(k1) != kdgu_len(k2))
		return false;

	unsigned idx1 = k1->idx, idx2 = k2->idx;
	k1->idx = 0, k2->idx = 0;

	do {
		if (kdgu_decode(k1) != kdgu_decode(k2))
			return false;
	} while (kdgu_inc(k1) && kdgu_inc(k2));

	k1->idx = idx1, k2->idx = idx2;
	return true;
}

void
kdgu_print(kdgu *k)
{
	if (!k) return;

	for (unsigned i = 0; i < k->len; i++)
		putchar(k->s[i]);
}

void
kdgu_debugprint(kdgu *k)
{
	if (!k) return;

	printf("<{%u}", k->len);
	for (unsigned i = 0; i < k->len; i++) {
		printf("%02X", (unsigned char)k->s[i]);
		if (i != k->len - 1) putchar(' ');
	}
	putchar('>');
}

void
kdgu_pchr(kdgu *k, FILE *f)
{
	if (!k || k->idx >= k->len) return;
	unsigned len = kdgu_chrsize(k);
	for (unsigned i = 0; i < len; i++)
		fputc(k->s[k->idx + i], f);
}

static bool
grapheme_break(enum boundclass l, enum boundclass r)
{
	if (l == KDGU_BOUNDCLASS_START) {
		return true; /* GB1 */
	} else if (l == KDGU_BOUNDCLASS_CR
	           && r == KDGU_BOUNDCLASS_LF) {
		return false; /* GB3 */
	} else if (l >= KDGU_BOUNDCLASS_CR
	           && l <= KDGU_BOUNDCLASS_CONTROL) {
		return true; /* GB4 */
	} else if (r >= KDGU_BOUNDCLASS_CR
	           && r <= KDGU_BOUNDCLASS_CONTROL) {
		return true; /* GB5 */
	} else if (l == KDGU_BOUNDCLASS_L &&
	           (r == KDGU_BOUNDCLASS_L ||
	            r == KDGU_BOUNDCLASS_V ||
	            r == KDGU_BOUNDCLASS_LV ||
	            r == KDGU_BOUNDCLASS_LVT)) {
		return false; /* GB6 */
	} else if ((l == KDGU_BOUNDCLASS_LV ||
	            l == KDGU_BOUNDCLASS_V) &&
	           (r == KDGU_BOUNDCLASS_V ||
	            r == KDGU_BOUNDCLASS_T)) {
		return false; /* GB7 */
	} else if ((l == KDGU_BOUNDCLASS_LVT ||
	            l == KDGU_BOUNDCLASS_T) &&
	           r == KDGU_BOUNDCLASS_T) {
		return false; /* GB8 */
	} else if (r == KDGU_BOUNDCLASS_EXTEND || /* GB9 */
	           r == KDGU_BOUNDCLASS_ZWJ ||
	           r == KDGU_BOUNDCLASS_SPACINGMARK || /* GB9a */
	           l == KDGU_BOUNDCLASS_PREPEND) {
		return false; /* GB9b */
	} else if ((l == KDGU_BOUNDCLASS_E_BASE ||
	            l == KDGU_BOUNDCLASS_E_BASE_GAZ) &&
	           r == KDGU_BOUNDCLASS_E_MODIFIER) {
		return false; /* GB10  */
	} else if ((l == KDGU_BOUNDCLASS_ZWJ &&
	            (r == KDGU_BOUNDCLASS_GLUE_AFTER_ZWJ ||
	             r == KDGU_BOUNDCLASS_E_BASE_GAZ))) {
		return false; /* GB11 */
	} else if (l == KDGU_BOUNDCLASS_REGIONAL_INDICATOR &&
	           r == KDGU_BOUNDCLASS_REGIONAL_INDICATOR) {
		return false; /* GB12/13 */
	}

	return true; /* GB999 */
}

size_t
kdgu_chomp(kdgu *k)
{
	k->idx = 0;

	while (kdgu_inc(k));
	while (kdgu_whitespace(k) && kdgu_dec(k));
	kdgu_inc(k);

	kdgu_delete(k, k->idx, k->len);

	return k->len - k->idx;
}

/* TODO: Should operate on characters, not bytes. */
void
kdgu_delete(kdgu *k, size_t a, size_t b)
{
	if (b > k->len || b <= a) return;
	memmove(k->s + a, k->s + b, k->len - b);
	k->len -= b - a;
}

void
kdgu_replace_substr(kdgu *k1, size_t a, size_t b, kdgu *k2)
{
	assert(b >= a);
	kdgu_size(k1, k1->len + k2->len);
	memcpy(k1->s + a, k2->s, k2->len < b - a ? k2->len : b - a);
	k1->len = k1->len + k2->len - (b - a);
}

static uint32_t cp1252[] = {
	0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7,
	0x8, 0x9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF,
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
	0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
	0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
	0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
	0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
	0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
	0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
	0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,
	0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
	0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
	0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F,
	0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
	0x78, 0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F,
	0x20AC, 0x201A, 0x192, 0x201E, 0x2026, 0x2020, 0x2021, 0x2C6,
	0x2030, 0x160, 0x2039, 0x152, 0x17D, 0x2018, 0x2019, 0x201C,
	0x201D, 0x2022, 0x2013, 0x2014, 0x2DC, 0x2122, 0x161, 0x203A,
	0x153, 0x17E, 0x178, 0xA0, 0xA1, 0xA2, 0xA3, 0xA4,
	0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xAB, 0xAC,
	0xAD, 0xAE, 0xAF, 0xB0, 0xB1, 0xB2, 0xB3, 0xB4,
	0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC,
	0xBD, 0xBE, 0xBF, 0xC0, 0xC1, 0xC2, 0xC3, 0xC4,
	0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC,
	0xCD, 0xCE, 0xCF, 0xD0, 0xD1, 0xD2, 0xD3, 0xD4,
	0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC,
	0xDD, 0xDE, 0xDF, 0xE0, 0xE1, 0xE2, 0xE3, 0xE4,
	0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC,
	0xED, 0xEE, 0xEF, 0xF0, 0xF1, 0xF2, 0xF3, 0xF4,
	0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC,
	0xFD, 0xFE, 0xFF
};

static uint32_t ebcdic[] = {
	0x00, 0x01, 0x02, 0x03, 0x9C, 0x09, 0x86, 0x7F,
	0x97, 0x8D, 0x8E, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
	0x10, 0x11, 0x12, 0x13, 0x9D, 0x85, 0x08, 0x87,
	0x18, 0x19, 0x92, 0x8F, 0x1C, 0x1D, 0x1E, 0x1F,
	0x80, 0x81, 0x82, 0x83, 0x84, 0x0A, 0x17, 0x1B,
	0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x05, 0x06, 0x07,
	0x90, 0x91, 0x16, 0x93, 0x94, 0x95, 0x96, 0x04,
	0x98, 0x99, 0x9A, 0x9B, 0x14, 0x15, 0x9E, 0x1A,
	0x20, 0xA0, 0xE2, 0xE4, 0xE0, 0xE1, 0xE3, 0xE5,
	0xE7, 0xF1, 0xA2, 0x2E, 0x3C, 0x28, 0x2B, 0x7C,
	0x26, 0xE9, 0xEA, 0xEB, 0xE8, 0xED, 0xEE, 0xEF,
	0xEC, 0xDF, 0x21, 0x24, 0x2A, 0x29, 0x3B, 0xAC,
	0x2D, 0x2F, 0xC2, 0xC4, 0xC0, 0xC1, 0xC3, 0xC5,
	0xC7, 0xD1, 0xA6, 0x2C, 0x25, 0x5F, 0x3E, 0x3F,
	0xF8, 0xC9, 0xCA, 0xCB, 0xC8, 0xCD, 0xCE, 0xCF,
	0xCC, 0x60, 0x3A, 0x23, 0x40, 0x27, 0x3D, 0x22,
	0xD8, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
	0x68, 0x69, 0xAB, 0xBB, 0xF0, 0xFD, 0xFE, 0xB1,
	0xB0, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F, 0x70,
	0x71, 0x72, 0xAA, 0xBA, 0xE6, 0xB8, 0xC6, 0xA4,
	0xB5, 0x7E, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
	0x79, 0x7A, 0xA1, 0xBF, 0xD0, 0xDD, 0xDE, 0xAE,
	0x5E, 0xA3, 0xA5, 0xB7, 0xA9, 0xA7, 0xB6, 0xBC,
	0xBD, 0xBE, 0x5B, 0x5D, 0xAF, 0xA8, 0xB4, 0xD7,
	0x7B, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
	0x48, 0x49, 0xAD, 0xF4, 0xF6, 0xF2, 0xF3, 0xF5,
	0x7D, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 0x50,
	0x51, 0x52, 0xB9, 0xFB, 0xFC, 0xF9, 0xFA, 0xFF,
	0x5C, 0xF7, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
	0x59, 0x5A, 0xB2, 0xD4, 0xD6, 0xD2, 0xD3, 0xD5,
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
	0x38, 0x39, 0xB3, 0xDB, 0xDC, 0xD9, 0xDA, 0x9F
};

uint32_t
kdgu_decode(kdgu *k)
{
	uint32_t c = 0;
	int endian = k->endian;

	switch (k->fmt) {
	case KDGU_FMT_CP1252:
		return cp1252[(unsigned char)k->s[k->idx]];

	case KDGU_FMT_EBCDIC:
		return ebcdic[(unsigned char)k->s[k->idx]];

	case KDGU_FMT_ASCII:
		return (uint32_t)k->s[k->idx];

	case KDGU_FMT_UTF8: {
		unsigned len = utf8chrlen(k->s + k->idx,
		                          k->len - k->idx);

		c = (k->s[k->idx] & ((1 << (8 - len)) - 1))
			<< (len - 1) * 6;
		for (unsigned i = 1; i < len; i++)
			c |= (k->s[k->idx + i] & 0x3F)
				<< (len - i - 1) * 6;
	} break;

	case KDGU_FMT_UTF16BE: if (!endian) endian = ENDIAN_BIG;
	case KDGU_FMT_UTF16LE: if (!endian) endian = ENDIAN_LITTLE;
	case KDGU_FMT_UTF16: {
		uint16_t d = READUTF16(k->s + k->idx);
		if (d <= 0xD7FF || d >= 0xE000) return d;

		/* It's a surrogate upper byte. */
		uint16_t e = READUTF16(k->s + k->idx + 2);
		c = (d - 0xD800) * 0x400 + e - 0xDC00 + 0x10000;
	} break;
	}

	return c;
}

struct kdgu_error
kdgu_encode(enum kdgu_fmt fmt, uint32_t c, char *buf,
            unsigned *len, unsigned idx)
{
	struct kdgu_error err = ERR(KDGU_ERR_NO_ERROR, 0);
	int endian = ENDIAN_NONE;

	switch (fmt) {
	case KDGU_FMT_CP1252:
		*len = 1;
		buf[0] = c & 0x7F;

		if (!IS_VALID_CP1252(c)) {
			err = ERR(KDGU_ERR_NO_CONVERSION, idx);
			buf[0] = KDGU_REPLACEMENT;
		}
		break;

	case KDGU_FMT_EBCDIC:
		*len = 1;

		for (unsigned i = 0; i < 256; i++) {
			if (ebcdic[i] == c) {
				buf[0] = i;
				break;
			}

			if (i == 255) {
				err = ERR(KDGU_ERR_NO_CONVERSION,
				          idx);
				buf[0] = KDGU_REPLACEMENT;
			}
		}
		break;

	case KDGU_FMT_ASCII:
		if (c > 127) {
			err = ERR(KDGU_ERR_NO_CONVERSION, idx);
			buf[0] = KDGU_REPLACEMENT;
			break;
		}

		*len = 1;
		buf[0] = c & 0xFF;
		break;

	case KDGU_FMT_UTF8:
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
		}
		break;

	case KDGU_FMT_UTF16BE: if (!endian) endian = ENDIAN_BIG;
	case KDGU_FMT_UTF16LE: if (!endian) endian = ENDIAN_LITTLE;
	case KDGU_FMT_UTF16:
		if (!endian) endian = ENDIAN_BIG;

		if (c <= 0xD7FF || (c >= 0xE000 && c <= 0xFFFF)) {
			*len = 2;
			if (endian == ENDIAN_BIG) {
				buf[1] = (c >> 8) & 0xFF;
				buf[0] = c & 0xFF;
			} else {
				buf[0] = (c >> 8) & 0xFF;
				buf[1] = c & 0xFF;
			}
			break;
		}

		c -= 0x10000;
		uint16_t high = (c >> 10) + 0xD800;
		uint16_t low = (c & 0x3FF) + 0xDC00;

		if (UTF16HIGH_SURROGATE(high)) *len = 4;
		else *len = 2;

		if (endian == ENDIAN_BIG) {
			buf[0] = (high >> 8) & 0xFF;
			buf[1] = high & 0xFF;
			buf[2] = (low >> 8) & 0xFF;
			buf[3] = low & 0xFF;
		} else {
			buf[1] = (high >> 8) & 0xFF;
			buf[0] = high & 0xFF;
			buf[3] = (low >> 8) & 0xFF;
			buf[2] = low & 0xFF;
		}
		break;
	}

	return err;
}

size_t
kdgu_chrsize(kdgu *k)
{
	unsigned idx = k->idx;
	unsigned c = kdgu_next(k);
	if (!c) return k->len - k->idx;
	k->idx = idx;
	return c;
}

bool
kdgu_append(kdgu *k, const char *s, size_t l)
{
	kdgu_size(k, k->len + l);
	memcpy(k->s + k->len, s, l);
	k->len = k->len + l;

	return true;
}

void
kdgu_print_error(struct kdgu_error err)
{
	if (err.kind == KDGU_ERR_NO_CONVERSION) {
		printf(err.msg, err.codepoint, err.data);
	} else {
		printf("%s", err.msg);
	}
}

static uint32_t
seqindex_decode_entry(uint16_t **entry)
{
	uint32_t cp = **entry;

	if ((cp & 0xF800) != 0xD800)
		return cp;

	(*entry)++;
	cp = ((cp & 0x03FF) << 10) | (**entry & 0x03FF);
	cp += 0x10000;

	return cp;
}

extern uint16_t sequences[];
extern uint16_t stage1table[];
extern uint16_t stage2table[];
extern struct kdgu_codepoint codepoints[];

extern size_t num_special_case;
extern struct kdgu_case special_case[];

static uint32_t
seqindex_decode_index(uint32_t idx)
{
	return seqindex_decode_entry(&(uint16_t *){sequences + idx});
}

static struct kdgu_codepoint *
codepoint(uint32_t u) {
	if (u == UINT32_MAX || u >= 0x110000) {
		return codepoints;
	} else {
		return codepoints + stage2table[stage1table[u >> 8] + (u & 0xFF)];
	}
}

static uint32_t
upperize(uint32_t c)
{
	uint16_t u = codepoint(c)->uc;
	return u != UINT16_MAX ? seqindex_decode_index(u) : c;
}

static uint32_t
lowerize(uint32_t c)
{
	uint16_t u = codepoint(c)->lc;
	return u != UINT16_MAX ? seqindex_decode_index(u) : c;
}

#define CHRMANIP(X)	  \
	if (!k || !k->len) return false; \
	bool ret = true; \
	unsigned idx = k->idx; \
	k->idx = 0; \
	unsigned l = kdgu_inc(k); \
	kdgu_dec(k); \
	do { \
		uint32_t c = kdgu_decode(k); \
		char buf[4]; \
		unsigned len; \
		c = X(c); \
		struct kdgu_error err = \
			kdgu_encode(k->fmt, c, buf, &len, k->idx); \
		if (err.kind) { \
			ret = false; \
			err.codepoint = c; \
			err.data = format[k->fmt]; \
			pusherror(k, err); \
		} \
		overwritechr(k, buf, len); \
		if (!l) break; \
	} while ((l = kdgu_inc(k))); \
	k->idx = idx; \
	return ret

bool
kdgu_delchr(kdgu *k)
{
	return !!overwritechr(k, &(char){ 0 }, 0);
}

unsigned
kdgu_next(kdgu *k)
{
	unsigned now = k->idx;

	do {
		unsigned idx = k->idx;
		uint32_t c1 = kdgu_decode(k);
		kdgu_inc(k);
		uint32_t c2 = kdgu_decode(k);
		k->idx = idx;
		if (grapheme_break(codepoint(c1)->bound,
		                   codepoint(c2)->bound)) {
			kdgu_inc(k);
			break;
		}
	} while (kdgu_inc(k));

	return k->idx - now;
}

unsigned
kdgu_prev(kdgu *k)
{
	unsigned now = k->idx;

	do {
		unsigned idx = k->idx;
		uint32_t c1 = kdgu_decode(k);
		kdgu_dec(k);
		uint32_t c2 = kdgu_decode(k);
		k->idx = idx;
		if (grapheme_break(codepoint(c1)->bound,
		                   codepoint(c2)->bound)) {
			kdgu_dec(k);
			break;
		}
	} while (kdgu_dec(k));

	return now - k->idx;
}

int
kdgu_inschr(kdgu *k, uint32_t c)
{
	if (!k) return 0;

	char buf[4];
	unsigned l;

	struct kdgu_error err =
		kdgu_encode(k->fmt, c, buf, &l, k->idx);

	if (err.kind) {
		err.codepoint = c;
		err.data = format[k->fmt];
		pusherror(k, err);
	}

	kdgu_size(k, k->len + l);
	memmove(k->s + k->idx + l,
	        k->s + k->idx,
	        k->len - k->idx);
	memcpy(k->s + k->idx, buf, l);
	k->len += l;

	return l;
}

size_t
kdgu_len(kdgu *k)
{
	if (!k || !k->len) return 0;

	k->idx = 0;
	size_t l = 0;
	do l++; while (kdgu_next(k));

	return l;
}

bool
kdgu_uc(kdgu *k)
{
	if (!k || !k->len) return false;

	bool ret = true;
	k->idx = 0;
	unsigned l = kdgu_inc(k);
	kdgu_dec(k);

	do {
		uint32_t c = kdgu_decode(k);
		uint32_t *u = NULL;
		unsigned len = 0;
		uint32_t cu = upperize(c);

		if (cu != c) {
			kdgu_delchr(k);
			kdgu_inschr(k, cu);
			continue;
		}

		for (unsigned i = 0;
		     i < num_special_case;
		     i++) {
			if (special_case[i].c == c) {
				u = special_case[i].upper;
				len = special_case[i].upper_len;
				break;
			}
		}

		if (!u || len == 0) continue;
		kdgu_delchr(k);

		for (unsigned i = 0; i < len; i++) {
			kdgu_inschr(k, u[i]);
			kdgu_inc(k);
		}

		kdgu_dec(k);
		if (!l) break;
	} while ((l = kdgu_inc(k)));

	return ret;
}

bool
kdgu_lc(kdgu *k)
{
	CHRMANIP(lowerize);
}

/* TODO: Think about the idx at return for all of these functions. */
bool
kdgu_reverse(kdgu *k)
{
	if (!k || !k->len) return false;

	unsigned end = kdgu_len(k) - 1;
	k->idx = k->len, kdgu_prev(k);
	unsigned a = 0, b = k->idx;

	for (unsigned i = 0; i < end / 2 + 1; i++) {
		k->idx = a; char c1[kdgu_chrsize(k)];
		k->idx = b; char c2[kdgu_chrsize(k)];

		memcpy(c1, k->s + a, sizeof c1);
		memcpy(c2, k->s + b, sizeof c2);

		k->idx = a, b += overwritechr(k, c2, sizeof c2);
		k->idx = b, overwritechr(k, c1, sizeof c1);

		k->idx = a, kdgu_next(k), a = k->idx;
		k->idx = b, kdgu_prev(k), b = k->idx;
	}

	return true;
}

void
kdgu_size(kdgu *k, size_t n)
{
	if (n <= k->alloc) return;

	if (!k->alloc) k->alloc = n / 2 + 1;
	else if (n >= k->alloc * 2) k->alloc = n;
	else k->alloc *= 2;

	void *p = realloc(k->s, k->alloc);
	if (!p) return;

	k->s = p;
}

static uint32_t whitespace[] = {
	0x9,    /* CHARACTER TABULATION          */
	0xA,    /* LINE FEED                     */
	0xB,    /* LINE TABULATION               */
	0xC,    /* FORM FEED                     */
	0xD,    /* CARRIAGE RETURN               */
	0x20,   /* SPACE                         */
	0x85,   /* NEXT LINE                     */
	0xA0,   /* NO-BREAK SPACE                */
	0x1680, /* OGHAM SPACE MARK              */
	0x180E, /* MONGOLIAN VOWEL SEPARATOR     */
	0x2000, /* EN QUAD                       */
	0x2001, /* EM QUAD                       */
	0x2002, /* EN SPACE                      */
	0x2003, /* EM SPACE                      */
	0x2004, /* THREE-PER-EM SPACE            */
	0x2005, /* FOUR-PER-EM SPACE             */
	0x2006, /* SIX-PER-EM SPACE              */
	0x2007, /* FIGURE SPACE                  */
	0x2008, /* PUNCTUATION SPACE             */
	0x2009, /* THIN SPACE                    */
	0x200A, /* HAIR SPACE                    */
	0x200B, /* ZERO WIDTH SPACE              */
	0x200C, /* ZERO WIDTH NON-JOINER         */
	0x200D, /* ZERO WIDTH JOINER             */
	0x2028, /* LINE SEPARATOR                */
	0x2029, /* PARAGRAPH SEPARATOR           */
	0x202F, /* NARROW NO-BREAK SPACE         */
	0x205F, /* MEDIUM MATHEMATICAL SPACE     */
	0x2060, /* WORD JOINER                   */
	0x3000, /* IDEOGRAPHIC SPACE             */
	0xFEFF  /* ZERO WIDTH NON-BREAKING SPACE */
};

bool
kdgu_whitespace(kdgu *k)
{
	if (!k) return false;
	uint32_t c = kdgu_decode(k);

	for (unsigned i = 0;
	     i < sizeof whitespace / sizeof *whitespace;
	     i++)
		if (c == whitespace[i])
			return true;

	return false;
}
