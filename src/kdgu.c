#include <stdint.h>
#include <limits.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <inttypes.h>

#include "kdgu.h"

#include "unicode_data.h"
#include "encoding.h"
#include "error.h"

#include "utf8.h"
#include "utf16.h"
#include "utf32.h"

/*
 * TODO: Case folding will need to specially consider U+0345:
 *
 * "The invocations of normalization before case folding in the
 * preceding definitions are to catch very infrequent edge
 * cases. Normalization is not required before case folding, except
 * for the character U+0345 n combining greek ypogegrammeni and any
 * characters that have it as part of their decomposition, such as
 * U+1FC3 o greek small letter eta with ypogegrammeni.  In practice,
 * optimized versions of implementations can catch these special
 * cases, thereby avoiding an extra normalization."
 *
 * http://www.unicode.org/versions/Unicode5.0.0/ch03.pdf
 */

/*
 * VOLATILE: This table must be kept up-to-date with the order of enum
 * fmt.
 */
static char *format[] = {
	"CP1252",
	"EBCDIC",
	"ASCII",
	"UTF-8",
	"UTF-16",
	"UTF-16-BE",
	"UTF-16-LE",
	"UTF-32",
	"UTF-32-LE",
	"UTF-32-BE"
};

#define IS_VALID_CP1252(X)	  \
	((X) == 0x81 \
	 || (X) == 0x8D \
	 || (X) == 0x8F \
	 || (X) == 0x90 \
	 || (X) == 0x9D)

static uint8_t *
cp1252validate(kdgu *k, const uint8_t *s, size_t *l)
{
	uint8_t *r = malloc(*l);
	if (!r) return NULL;
	memcpy(r, s, *l);

	/*
	 * Some bytes are never used!
	 * https://en.wikipedia.org/wiki/Windows-1252
	 */
	for (unsigned i = 0; i < k->len; i++)
		if (!IS_VALID_CP1252((uint8_t)r[i])) {
			pusherror(k, ERR(ERR_INVALID_CP1252, i));
			r[i] = KDGU_REPLACEMENT;
		}

	return r;
}

static uint8_t *
asciivalidate(kdgu *k, const uint8_t *s, size_t *l)
{
	uint8_t *r = malloc(*l);
	if (!r) return NULL;
	memcpy(r, s, *l);

	for (unsigned i = 0; i < k->len; i++)
		if ((uint8_t)s[i] > 128)
			pusherror(k, ERR(ERR_INVALID_ASCII, i));

	return r;
}

static uint8_t *
ebcdicvalidate(kdgu *k, const uint8_t *s, size_t *l)
{
	uint8_t *r = malloc(*l);
	if (!r) return NULL;
	memcpy(r, s, *l);

	for (unsigned i = 0; i < k->len; i++)
		if ((uint8_t)s[i] == 48
		    || (uint8_t)s[i] == 49)
			pusherror(k, ERR(ERR_INVALID_EBCDIC, i));

	return r;
}

void
kdgu_print_error(struct error err)
{
	if (err.kind == ERR_NO_CONVERSION) {
		printf(error_str[err.kind], err.codepoint, err.data);
	} else {
		printf("%s", error_str[err.kind]);
	}
}

static void
delete_point(kdgu *k)
{
	unsigned idx = k->idx;
	unsigned l = kdgu_inc(k);

	if (!l) l = k->len - k->idx;
	if (!l) return;

	k->idx = idx;
	memmove(k->s + k->idx,
	        k->s + k->idx + l,
	        k->len - k->idx - l);
	k->len -= l;
}

static int
overwritechr(kdgu *k, uint8_t *b, unsigned l1)
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

static bool
grapheme_break(enum boundclass l, enum boundclass r)
{
	if (l == BOUNDCLASS_START) {
		return true; /* GB1 */
	} else if (l == BOUNDCLASS_CR
	           && r == BOUNDCLASS_LF) {
		return false; /* GB3 */
	} else if (l >= BOUNDCLASS_CR
	           && l <= BOUNDCLASS_CONTROL) {
		return true; /* GB4 */
	} else if (r >= BOUNDCLASS_CR
	           && r <= BOUNDCLASS_CONTROL) {
		return true; /* GB5 */
	} else if (l == BOUNDCLASS_L &&
	           (r == BOUNDCLASS_L ||
	            r == BOUNDCLASS_V ||
	            r == BOUNDCLASS_LV ||
	            r == BOUNDCLASS_LVT)) {
		return false; /* GB6 */
	} else if ((l == BOUNDCLASS_LV ||
	            l == BOUNDCLASS_V) &&
	           (r == BOUNDCLASS_V ||
	            r == BOUNDCLASS_T)) {
		return false; /* GB7 */
	} else if ((l == BOUNDCLASS_LVT ||
	            l == BOUNDCLASS_T) &&
	           r == BOUNDCLASS_T) {
		return false; /* GB8 */
	} else if (r == BOUNDCLASS_EXTEND || /* GB9 */
	           r == BOUNDCLASS_ZWJ ||
	           r == BOUNDCLASS_SPACINGMARK || /* GB9a */
	           l == BOUNDCLASS_PREPEND) {
		return false; /* GB9b */
	} else if ((l == BOUNDCLASS_E_BASE ||
	            l == BOUNDCLASS_E_BASE_GAZ) &&
	           r == BOUNDCLASS_E_MODIFIER) {
		return false; /* GB10  */
	} else if ((l == BOUNDCLASS_ZWJ &&
	            (r == BOUNDCLASS_GLUE_AFTER_ZWJ ||
	             r == BOUNDCLASS_E_BASE_GAZ))) {
		return false; /* GB11 */
	} else if (l == BOUNDCLASS_REGIONAL_INDICATOR &&
	           r == BOUNDCLASS_REGIONAL_INDICATOR) {
		return false; /* GB12/13 */
	}

	return true; /* GB999 */
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
		                   codepoint(c2)->bound))
			break;
	} while (kdgu_inc(k));
	if (!kdgu_inc(k)) k->idx = now;

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
		                   codepoint(c2)->bound))
			break;
	} while (kdgu_dec(k));
	if (!kdgu_dec(k)) k->idx = now;

	return now - k->idx;
}

static int
insert_point(kdgu *k, uint32_t c)
{
	if (!k) return 0;

	uint8_t buf[4];
	unsigned len;

	struct error err =
		kdgu_encode(c, buf, &len, k->fmt, k->idx, k->endian);

	if (err.kind) {
		err.codepoint = c;
		err.data = format[k->fmt];
		pusherror(k, err);
		return 0;
	}

	kdgu_size(k, k->len + len);
	memmove(k->s + k->idx + len,
	        k->s + k->idx,
	        k->len - k->idx);
	memcpy(k->s + k->idx, buf, len);
	k->len += len;

	return len;
}

static int
insert_buffer(kdgu *k, uint8_t *buf, unsigned len)
{
	if (!k) return 0;

	kdgu_size(k, k->len + len);
	memmove(k->s + k->idx + len,
	        k->s + k->idx,
	        k->len - k->idx);
	memcpy(k->s + k->idx, buf, len);
	k->len += len;

	return len;
}

unsigned
kdgu_len(kdgu *k)
{
	if (!k || !k->len) return 0;

	k->idx = 0;
	size_t l = 0;
	while (kdgu_next(k)) l++;

	return l;
}

static unsigned
decompose_char(uint32_t c,
               uint32_t *buf,
               unsigned buflen)
{
	struct codepoint *cp = codepoint(c);
	int32_t hangul_s = c - HANGUL_SBASE;

	if (hangul_s >= 0 && hangul_s < HANGUL_SCOUNT) {
		uint32_t hangul_t = hangul_s % HANGUL_TCOUNT;

		if (!buf || !buflen)
			return hangul_t ? 3 : 2;

		buf[0] = HANGUL_LBASE +
			hangul_s / HANGUL_NCOUNT;

		buf[1] = HANGUL_VBASE +
			(hangul_s % HANGUL_NCOUNT)
			/ HANGUL_TCOUNT;

		if (!hangul_t) return 2;
		return buf[2] = HANGUL_TBASE + hangul_t, 3;
	}

	if (cp->decomp != UINT16_MAX)
		return write_sequence(buf, cp->decomp);

	return *buf = c, 1;
}

bool
kdgu_uc(kdgu *k)
{
	if (!k || !k->len) return false;
	k->idx = 0;

	do {
		uint32_t c = kdgu_decode(k);
		struct codepoint *cp = codepoint(c);
		uint32_t buf[20];
		unsigned len;

		if (cp->upper != UINT16_MAX) {
			len = write_sequence(buf, cp->upper);
		} else if (cp->special_uc != UINT16_MAX) {
			len = write_sequence(buf, cp->special_uc);
		} else continue;

		delete_point(k);
		for (unsigned i = 0; i < len; i++)
			insert_point(k, buf[i]);
	} while (kdgu_inc(k));

	return true;
}

bool
kdgu_lc(kdgu *k)
{
	if (!k || !k->len) return false;
	k->idx = 0;

	do {
		uint32_t c = kdgu_decode(k);
		struct codepoint *cp = codepoint(c);
		uint32_t buf[20];
		unsigned len;

		if (cp->lower != UINT16_MAX) {
			len = write_sequence(buf, cp->lower);
		} else if (cp->special_lc != UINT16_MAX) {
			len = write_sequence(buf, cp->special_lc);
		} else continue;

		delete_point(k);
		for (unsigned i = 0; i < len; i++)
			insert_point(k, buf[i]);
	} while (kdgu_inc(k));

	return true;
}

bool
kdgu_reverse(kdgu *k)
{
	if (!k || !k->len || k->len == 1) return false;

	unsigned end = kdgu_len(k) - 1;
	k->idx = 0; while (kdgu_next(k)); kdgu_prev(k);
	unsigned a = 0, b = k->idx;

	for (unsigned i = 0; i < end / 2 + 1; i++) {
		k->idx = a; uint8_t c1[kdgu_chrsize(k)];
		k->idx = b; uint8_t c2[kdgu_chrsize(k)];

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

/*
 * Only `safenize()' uses these two functions. They return the number
 * of leading non-starter code points and trailing non-starter code
 * points in `buf', respectively.
 */

static unsigned
leading_nonstarters(uint32_t *buf, unsigned len)
{
	unsigned n = 0;
	while (n < len && codepoint(buf[n])->ccc) n++;
	return n;
}

static unsigned
trailing_nonstarters(uint32_t *buf, unsigned len)
{
	int n = len - 1;
	while (n >= 0 && codepoint(buf[n])->ccc) n--;
	return len - n;
}

/*
 * This function ensures that `k' is in Stream-Safe Text Format by the
 * algorithm described in http://unicode.org/reports/tr15/#UAX15-D4
 */

static void
safenize(kdgu *k)
{
	unsigned non_starter_count = 0;

	for (k->idx = 0; k->idx < k->len; kdgu_inc(k)) {
		uint32_t c = kdgu_decode(k);
		uint32_t buf[100];
		unsigned len = decompose_char(c, buf, 100);

		if (leading_nonstarters(buf, len)
		    + non_starter_count > 5) {
			/* Insert the combining grapheme joiner. */
			insert_point(k, 0x34F);
			kdgu_inc(k);
			non_starter_count = 0;
		}

		unsigned trailing = trailing_nonstarters(buf, len);
		non_starter_count = trailing == len
			? trailing
			: non_starter_count + len;
	}
}

kdgu *
kdgu_new(enum fmt fmt, const uint8_t *s, size_t len)
{
	kdgu *k = malloc(sizeof *k);
	if (!k) return NULL;

	memset(k, 0, sizeof *k);
	k->fmt = fmt;

	int endian = ENDIAN_NONE;

	switch (fmt) {
	case FMT_CP1252: k->s = cp1252validate(k, s, &len); break;
	case FMT_ASCII: k->s = asciivalidate(k, s, &len); break;
	case FMT_EBCDIC: k->s = ebcdicvalidate(k, s, &len); break;
	case FMT_UTF8: k->s = utf8validate(k, s, &len); break;
	case FMT_UTF16LE: endian = ENDIAN_LITTLE;
	case FMT_UTF16BE: if (!endian) endian = ENDIAN_BIG;
	case FMT_UTF16:
		k->s = utf16validate(k, s, &len, endian);
		break;
	case FMT_UTF32LE: endian = ENDIAN_LITTLE;
	case FMT_UTF32BE: if (!endian) endian = ENDIAN_BIG;
	case FMT_UTF32:
		k->s = utf32validate(k, s, &len, endian);
		break;
	}

	if (!k->s) {
		free(k);
		return NULL;
	}

	k->len = len, k->alloc = k->len;
	kdgu_normalize(k, NORM_NFC), safenize(k);

	return k;
}

kdgu *
kdgu_news(const char *s)
{
	return kdgu_new(FMT_ASCII, (const uint8_t *)s, strlen(s));
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
	if (!k1 || !k2 || k1->fmt != k2->fmt)
		return false;

	k1->alloc += k2->len;
	uint8_t *p = realloc(k1->s, k1->alloc);
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

	switch (k->fmt) {
	case FMT_CP1252:
	case FMT_EBCDIC:
	case FMT_ASCII:
		idx++;
		break;

	case FMT_UTF8:
		do {
			idx++;
		} while (idx < k->len && UTF8CONT(k->s[idx]));
		break;

	case FMT_UTF16BE:
	case FMT_UTF16LE:
	case FMT_UTF16:
		idx += 2;
		if (idx >= k->len) break;
		if (UTF16HIGH_SURROGATE(READUTF16(k->endian,
		                                  k->s + idx)))
			idx += 2;
		break;

	case FMT_UTF32BE:
	case FMT_UTF32LE:
	case FMT_UTF32:
		idx += 4;
		break;
	}

	if (idx > k->len) return 0;

	unsigned r = idx - k->idx;
	k->idx = idx;

	return r;
}

unsigned
kdgu_dec(kdgu *k)
{
	if (!k || !k->idx) return 0;
	unsigned idx = k->idx;

	switch (k->fmt) {
	case FMT_CP1252:
	case FMT_EBCDIC:
	case FMT_ASCII:
		idx--;
		break;

	case FMT_UTF8:
		do {
			idx--;
		} while (idx && UTF8CONT(k->s[idx]));
		break;

	case FMT_UTF16BE:
	case FMT_UTF16LE:
	case FMT_UTF16:
		idx -= 2;
		if (UTF16LOW_SURROGATE(READUTF16(k->endian,
		                                 k->s + idx - 2)))
			idx -= 2;
		break;

	case FMT_UTF32BE:
	case FMT_UTF32LE:
	case FMT_UTF32:
		idx -= 4;
		break;
	}

	if (idx >= k->len) return 0;

	unsigned r = k->idx - idx;
	k->idx = idx;

	return r;
}

bool
kdgu_nth(kdgu *k, unsigned n)
{
	if (!k) return false;

	if (k->fmt == FMT_ASCII
	    || k->fmt == FMT_EBCDIC
	    || k->fmt == FMT_CP1252) {
		if (n >= k->len) return false;
		k->idx = n;
		return true;
	}

	if (k->fmt == FMT_UTF32) {
		if (n >= k->len / 4) return false;
		k->idx = n * 4;
		return true;
	}

	k->idx = 0;
	for (unsigned i = 0; i < n; i++)
		if (!kdgu_next(k))
			return false;

	return true;
}

kdgu *
kdgu_getnth(kdgu *k, unsigned n)
{
	if (!k || !k->len) return NULL;

	unsigned idx = k->idx;
	kdgu *chr = NULL;

	if (!kdgu_nth(k, n)) return NULL;
	chr = kdgu_new(k->fmt,
	               k->s + k->idx,
	               kdgu_chrsize(k));
	k->idx = idx;

	return chr;
}

bool
kdgu_convert(kdgu *k, enum fmt fmt)
{
	if (!k) return false;
	if (!k->len || k->fmt == fmt) {
		k->fmt = fmt;
		return true;
	}

	k->idx = 0;
	int endian = k->endian;
	if (!endian) endian = ENDIAN_BIG;

	while (k->idx < k->len) {
		unsigned len;
		uint32_t c = kdgu_decode(k);
		uint8_t buf[4];

		struct error err =
			kdgu_encode(c, buf, &len,
				    fmt, k->idx, endian);

		if (err.kind) {
			err.codepoint = c;
			err.data = format[fmt];
			pusherror(k, err);

			kdgu_encode(KDGU_REPLACEMENT, buf, &len,
				    fmt, k->idx, endian);
		}

		delete_point(k);
		insert_buffer(k, buf, len);
		k->idx += len;
	}

	k->fmt = fmt;
	k->endian = endian;

	return true;
}


/*
 * Uses bubble sort to organize the code points in `buf' according to
 * their Canonical_Combining_Class (ccc) properties.
 */

static void
sort_combining_marks(uint32_t *buf, unsigned len)
{
 sort:
	/* We use `len - 1' because we read pairs of numbers here. */
	for (unsigned i = 0; i < len - 1; i++) {
		struct codepoint *cp1 = codepoint(buf[i]);
		struct codepoint *cp2 = codepoint(buf[i + 1]);

		if (!cp2->ccc
		    || !cp1->ccc
		    || cp2->ccc >= cp1->ccc)
			continue;

		uint32_t temp = buf[i + 1];

		buf[i + 1] = buf[i];
		buf[i] = temp;

		goto sort;
	}
}

/*
 * This function does two things: it converts all decomposable code
 * points into their decomposed forms, then it sorts all sequences of
 * combining marks according to the Canonical Ordering Algorithm
 * described in Standard Annex #15.
 */

static void
decompose(kdgu *k, bool compat)
{
	if ((k->norm == NORM_NFD && !compat)
	    || (k->norm == NORM_NFKD && compat))
		return;

	/*
	 * KDGU strings are in Stream-Safe Text Format, so there will
	 * never be any sequences of starters and non-starters longer
	 * than 32 code points long:
	 * http://unicode.org/reports/tr15/#Stream_Safe_Text_Format
	 */
	uint32_t buf[32];
	unsigned len;

	/* This first loop decomposes all decomposable characters. */
	for (k->idx = 0; k->idx < k->len; kdgu_inc(k)) {
		uint32_t c = kdgu_decode(k);
		if (c == UINT32_MAX) break;
		struct codepoint *cp = codepoint(c);

		/*
		 * We don't need to continue if the character has no
		 * decomposition.
		 */
		if (cp->decomp == UINT16_MAX) continue;
		if (!compat && cp->decomp_type != DECOMP_TYPE_FONT)
			continue;

		len = decompose_char(c, buf,
				     sizeof buf / sizeof *buf);

		if (!len) continue;
		delete_point(k);

		for (unsigned i = 0; i < len; i++) {
			insert_point(k, buf[i]);
			if (i != len - 1) kdgu_inc(k);
		}
	}

	/* This loop sorts all sequences of combining marks. */
	for (k->idx = 0; k->idx < k->len; kdgu_inc(k)) {
		uint32_t c = kdgu_decode(k);
		struct codepoint *cp = codepoint(c);

		/* It's a starter. No sequence here! */
		if (!cp->ccc) continue;

		unsigned beginning = k->idx;
		len = 0;

		/* Search for the end of the sequence. */
		do {
			buf[len++] = c;
			kdgu_inc(k);

			c = kdgu_decode(k);
			cp = codepoint(c);
		} while (cp->ccc
		         && len < sizeof buf / sizeof *buf);

		sort_combining_marks(buf, len);

		/* Delete the sequence. */
		kdgu_delete(k, beginning, k->idx);
		k->idx = beginning;

		for (unsigned i = 0; i < len; i++) {
			insert_point(k, buf[i]);
			if (i != len - 1) kdgu_inc(k);
		}
	}

	k->norm = compat ? NORM_NFKD : NORM_NFD;
}

static bool
compose_char(kdgu *k)
{
	unsigned beginning = k->idx;

	/* Read the pair of code points. */
	uint32_t c1 = kdgu_decode(k);
	if (!kdgu_inc(k) || k->idx == k->len) return false;
	uint32_t c2 = kdgu_decode(k);

	k->idx = beginning;

	struct codepoint *cp1 = codepoint(c1);
	struct codepoint *cp2 = codepoint(c2);
	if (!cp1 || !cp2) return false;

	uint32_t composition = lookup_comp(c1, c2);
	/* No valid composition exists; do nothing. */
	if (composition == UINT32_MAX) return false;

	/*
	 * Only at this point are we sure we have a valid composition.
	 */

	kdgu_inc(k), kdgu_inc(k);
	kdgu_delete(k, beginning, k->idx);
	k->idx = beginning;
	insert_point(k, composition);

	return true;
}

/*
 * This function will first decompose all decomposable code points and
 * sort all combining marks with `decompose()'. Then it will read
 * pairs of code points and use the trie sequence indices to look up
 * possible compositions. If it successfully finds a composition for
 * any pair of characters it will delete that pair and insert the
 * composition at that point.
 *
 * It's worth noting that NFC and NFKC have identical compositions
 * (only their decompositions differ), so this function's composition
 * code has no additional logic for NFKC.
 */

/* TODO: Use faster checks to reduce unnecessary processing. */
static void
compose(kdgu *k, bool compat)
{
	if ((k->norm == NORM_NFC && !compat)
	    || (k->norm == NORM_NFKC && compat))
		return;

	decompose(k, compat);

	for (k->idx = 0; k->idx < k->len; kdgu_inc(k)) {
		/*
		 * We decrement here to offset the increment that will
		 * occur in the next iteration of the loop. This is
		 * done so that when a successful composition occurs
		 * the next pair will include the new composition code
		 * point; this correctly composes longer sequences of
		 * combining marks on a single starter code point.
		 */

		if (compose_char(k) && !kdgu_dec(k)) {
			compose(k, compat);
			break;
		}
	}

	k->norm = compat ? NORM_NFKC : NORM_NFC;
}

bool
kdgu_normalize(kdgu *k, enum normalization norm)
{
	if (!k || !k->len) return false;

	switch (norm) {
	case NORM_NONE:                      break;
	case NORM_NFD:  decompose(k, false); break;
	case NORM_NFC:  compose(k, false);   break;
	case NORM_NFKD: decompose(k, true);  break;
	case NORM_NFKC: compose(k, true);    break;
	}

	return true;
}

bool
kdgu_cmp(kdgu *k1, kdgu *k2)
{
	if (!k1 || !k2) return false;
	kdgu_normalize(k1, NORM_NFC), kdgu_normalize(k2, NORM_NFC);
	k1->idx = 0, k2->idx = 0;

	do {
		if (kdgu_decode(k1) != kdgu_decode(k2))
			return false;
	} while (kdgu_inc(k1) && kdgu_inc(k2));

	if (kdgu_inc(k1) || kdgu_inc(k2)) return false;
	return true;
}

void
kdgu_print(kdgu *k)
{
	if (!k) return;

	for (unsigned i = 0; i < k->len; i++)
		putchar((uint8_t)k->s[i]);
}

void
kdgu_debugprint1(kdgu *k)
{
	if (!k) return;

	printf("{%u} <", k->len);

	for (unsigned i = 0; i < k->len; i++) {
		printf("%02X", (uint8_t)k->s[i]);
		if (i != k->len - 1) putchar(' ');
	}

	putchar('>');
}

void
kdgu_debugprint2(kdgu *k)
{
	if (!k) return;

	printf("{%u} <", kdgu_len(k));
	k->idx = 0;

	do {
		uint32_t c1 = kdgu_decode(k);
		if (c1 == UINT32_MAX) break;

		kdgu_inc(k);
		uint32_t c2 = kdgu_decode(k);

		printf("U+%02"PRIX32, c1);
		if (c2 == UINT32_MAX) break;

		if (grapheme_break(codepoint(c1)->bound,
		                   codepoint(c2)->bound))
			printf(" | ");
		else putchar(' ');
	} while (true);

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

unsigned
kdgu_chomp(kdgu *k)
{
	k->idx = k->len;

	while (kdgu_dec(k) && kdgu_whitespace(k));
	kdgu_inc(k);
	kdgu_delete(k, k->idx, k->len);

	return k->len - k->idx;
}

void
kdgu_delete(kdgu *k, size_t a, size_t b)
{
	if (b > k->len || b <= a) return;
	memmove(k->s + a, k->s + b, k->len - b);
	k->len -= b - a;
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
	if (!k->len || k->idx >= k->len) return UINT32_MAX;
	uint32_t c = 0;

	switch (k->fmt) {
	case FMT_CP1252:
		return cp1252[(uint8_t)k->s[k->idx]];

	case FMT_EBCDIC:
		return ebcdic[(uint8_t)k->s[k->idx]];

	case FMT_ASCII:
		return (uint32_t)k->s[k->idx];

	case FMT_UTF8: {
		unsigned len = utf8chrlen(k->s + k->idx,
		                          k->len - k->idx);

		c = (k->s[k->idx] & ((1 << (8 - len)) - 1))
			<< (len - 1) * 6;
		for (unsigned i = 1; i < len; i++)
			c |= (k->s[k->idx + i] & 0x3F)
				<< (len - i - 1) * 6;
	} break;

	case FMT_UTF16BE:
	case FMT_UTF16LE:
	case FMT_UTF16: {
		uint16_t d = READUTF16(k->endian, k->s + k->idx);
		if (d <= 0xD7FF || d >= 0xE000) return d;

		/* It's a surrogate upper byte. */
		uint16_t e = READUTF16(k->endian, k->s + k->idx + 2);
		c = (d - 0xD800) * 0x400 + e - 0xDC00 + 0x10000;
	} break;

	case FMT_UTF32BE:
	case FMT_UTF32LE:
	case FMT_UTF32:
		return READUTF32(k->endian, k->s + k->idx);
	}

	return c;
}

/* TODO: Make sure NO_CONVERSION errors are consistent. */
struct error
kdgu_encode(uint32_t c, uint8_t *buf, unsigned *len,
	    enum fmt fmt, unsigned idx, int endian)
{
	struct error err = ERR(ERR_NO_ERROR, 0);

	switch (fmt) {
	case FMT_CP1252:
		if (c == KDGU_REPLACEMENT) c = '?';
		*len = 1, buf[0] = c & 0x7F;
		if (!IS_VALID_CP1252(c))
			ENCERR(kdgu_encode, buf, len,
			       FMT_CP1252, idx, endian);
		break;

	case FMT_EBCDIC: {
		if (c == KDGU_REPLACEMENT) c = '?';
		uint32_t o = -1;

		for (unsigned i = 0; i < 256 && o == UINT32_MAX; i++)
			o = (c == ebcdic[i]) ? ebcdic[i] : c;

		*len = 1, buf[0] = o;
		if (o != UINT32_MAX) break;
		ENCERR(kdgu_encode, buf, len,
		       FMT_EBCDIC, idx, endian);
	} break;

	case FMT_ASCII:
		if (c == KDGU_REPLACEMENT) c = '?';
		*len = 1, buf[0] = c & 0xFF;
		if (c > 127)
			ENCERR(kdgu_encode, buf, len,
			       FMT_ASCII, idx, endian);
		break;

	case FMT_UTF8: err = utf8encode(c, buf, len, idx); break;
	case FMT_UTF16LE:
	case FMT_UTF16BE:
	case FMT_UTF16:
		err = utf16encode(c, buf, len, idx, endian);
		break;

	case FMT_UTF32LE:
	case FMT_UTF32BE:
	case FMT_UTF32:
		*len = 4;

		if (endian == ENDIAN_LITTLE) {
			buf[0] = c & 0xFF;
			buf[1] = (c >> 8) & 0xFF;
			buf[2] = (c >> 16) & 0xFF;
			buf[3] = (c >> 24) & 0xFF;
		} else {
			buf[0] = (c >> 24) & 0xFF;
			buf[1] = (c >> 16) & 0xFF;
			buf[2] = (c >> 8) & 0xFF;
			buf[3] = c & 0xFF;
		}
		break;
	}

	return err;
}

unsigned
kdgu_chrsize(kdgu *k)
{
	unsigned i = k->idx, c = kdgu_next(k), ret = k->len - k->idx;
	return k->idx = i, c ? c : ret;
}

bool
kdgu_append(kdgu *k, const uint8_t *s, size_t l)
{
	kdgu_size(k, k->len + l);
	memcpy(k->s + k->len, s, l);
	k->len = k->len + l;

	return true;
}
