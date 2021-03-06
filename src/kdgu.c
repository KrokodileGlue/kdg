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
	!((X) == 0x81 \
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

static bool
delete_point(kdgu *k, unsigned idx)
{
	unsigned l = kdgu_inc(k, &(unsigned){idx});
	if (!l) return false;

	memmove(k->s + idx,
	        k->s + idx + l,
	        k->len - idx - l);
	k->len -= l;

	return true;
}

static int
overwritechr(kdgu *k, unsigned idx, uint8_t *b, unsigned l1)
{
	unsigned l2 = kdgu_chrsize(k, idx);

	if (l1 == l2) {
		memcpy(k->s + idx, b, l1);
		return 0;
	} else if (l1 > l2) {
		kdgu_size(k, k->len + l1 - l2);
		memmove(k->s + idx + l1,
		        k->s + idx + l2,
		        k->len - idx - l2);
		memcpy(k->s + idx, b, l1);
		k->len += l1 - l2;
		return l1 - l2;
	} else if (l1 < l2) {
		kdgu_size(k, k->len + l2 - l1);
		memmove(k->s + idx + l1,
		        k->s + idx + l2,
		        k->len - idx - l2);
		memcpy(k->s + idx, b, l1);
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
kdgu_next(const kdgu *k, unsigned *idx)
{
	if (!k) return 0;
	unsigned now = *idx;

	do {
		unsigned tmp = *idx;
		uint32_t c1 = kdgu_decode(k, *idx);
		kdgu_inc(k, idx);
		uint32_t c2 = kdgu_decode(k, *idx);
		*idx = tmp;
		if (grapheme_break(codepoint(c1)->bound,
		                   codepoint(c2)->bound))
			break;
	} while (kdgu_inc(k, idx));
	if (!kdgu_inc(k, idx)) *idx = now;

	return *idx - now;
}

unsigned
kdgu_prev(const kdgu *k, unsigned *idx)
{
	if (!k) return 0;
	unsigned now = *idx;

	do {
		unsigned tmp = *idx;
		uint32_t c1 = kdgu_decode(k, *idx);
		kdgu_dec(k, idx);
		uint32_t c2 = kdgu_decode(k, *idx);
		*idx = tmp;
		if (grapheme_break(codepoint(c1)->bound,
		                   codepoint(c2)->bound))
			break;
	} while (kdgu_dec(k, idx));
	if (!kdgu_dec(k, idx)) *idx = now;

	return *idx - now;
}

static unsigned
insert_point(kdgu *k, unsigned idx, uint32_t c)
{
	if (!k) return 0;

	uint8_t buf[4];
	unsigned len;
	struct error err = kdgu_encode(c, buf, &len, k->fmt, idx, GETENDIAN(k->fmt));

	if (err.kind) {
		err.codepoint = c;
		err.data = format[k->fmt];
		pusherror(k, err);
		return 0;
	}

	kdgu_size(k, k->len + len);
	if (k->len)
		memmove(k->s + idx + len,
		        k->s + idx,
		        k->len - idx);
	memcpy(k->s + idx, buf, len);
	k->len += len;

	return len;
}

static unsigned
insert_buffer(kdgu *k, unsigned idx, uint8_t *buf, unsigned len)
{
	if (!k) return 0;

	kdgu_size(k, k->len + len);
	memmove(k->s + idx + len,
	        k->s + idx,
	        k->len - idx);
	memcpy(k->s + idx, buf, len);
	k->len += len;

	return len;
}

unsigned
kdgu_len(const kdgu *k)
{
	if (!k || !k->len) return 0;

	unsigned l = 0, idx = 0;
	while (kdgu_next(k, &idx)) l++;

	return l;
}

static unsigned
decompose_char(uint32_t c,
               uint32_t *buf,
               unsigned buflen)
{
	const struct codepoint *cp = codepoint(c);
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
kdgu_setchr(kdgu *k, unsigned idx, uint32_t c)
{
	return delete_point(k, idx), insert_point(k, idx, c);
}

bool
kdgu_uc(kdgu *k)
{
	if (!k || !k->len) return false;
	unsigned idx = 0;

	do {
		uint32_t c = kdgu_decode(k, idx);
		const struct codepoint *cp = codepoint(c);
		uint32_t buf[20];
		unsigned len;

		if (cp->upper != UINT16_MAX) {
			len = write_sequence(buf, cp->upper);
		} else if (cp->special_uc != UINT16_MAX) {
			len = write_sequence(buf, cp->special_uc);
		} else continue;

		delete_point(k, idx);
		for (unsigned i = 0; i < len; i++)
			insert_point(k, idx, buf[i]);
	} while (kdgu_inc(k, &idx));

	return true;
}

bool
kdgu_lc(kdgu *k)
{
	if (!k || !k->len) return false;
	unsigned idx = 0;

	do {
		uint32_t c = kdgu_decode(k, idx);
		const struct codepoint *cp = codepoint(c);
		uint32_t buf[20];
		unsigned len;

		if (cp->lower != UINT16_MAX) {
			len = write_sequence(buf, cp->lower);
		} else if (cp->special_lc != UINT16_MAX) {
			len = write_sequence(buf, cp->special_lc);
		} else continue;

		delete_point(k, idx);
		for (unsigned i = 0; i < len; i++)
			insert_point(k, idx, buf[i]);
	} while (kdgu_inc(k, &idx));

	return true;
}

bool
kdgu_reverse(kdgu *k)
{
	if (!k || !k->len || k->len == 1) return false;

	unsigned end = kdgu_len(k) - 1;
	while (kdgu_next(k, &end));
	kdgu_prev(k, &end);
	unsigned a = 0, b = end;

	for (unsigned i = 0; i < end / 2 + 1; i++) {
		uint8_t c1[kdgu_chrsize(k, a)];
		uint8_t c2[kdgu_chrsize(k, b)];

		memcpy(c1, k->s + a, sizeof c1);
		memcpy(c2, k->s + b, sizeof c2);

		b += overwritechr(k, a, c2, sizeof c2);
		overwritechr(k, b, c1, sizeof c1);

		kdgu_next(k, &a);
		kdgu_prev(k, &b);
	}

	return true;
}

void
kdgu_size(kdgu *k, size_t n)
{
	if (n <= k->alloc) return;

	if (!k->alloc || n >= k->alloc * 2) k->alloc = n;
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
kdgu_whitespace(const kdgu *k, unsigned idx)
{
	if (!k) return false;
	uint32_t c = kdgu_decode(k, idx);

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

	for (unsigned i = 0; i < k->len; kdgu_inc(k, &i)) {
		uint32_t c = kdgu_decode(k, i);
		uint32_t buf[100];
		unsigned len = decompose_char(c, buf, 100);

		if (leading_nonstarters(buf, len)
		    + non_starter_count > 30) {
			/* Insert the combining grapheme joiner. */
			insert_point(k, i, 0x34F);
			kdgu_inc(k, &i);
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

	memset(k, 0, sizeof *k), k->fmt = fmt;
	if (!len) return k;

	int endian = KDGU_ENDIAN_NONE;

	switch (fmt) {
	case KDGU_FMT_CP1252: k->s = cp1252validate(k, s, &len); break;
	case KDGU_FMT_ASCII:  k->s = asciivalidate(k, s, &len); break;
	case KDGU_FMT_EBCDIC: k->s = ebcdicvalidate(k, s, &len); break;
	case KDGU_FMT_UTF8:   k->s = utf8validate(k, s, &len); break;
	case KDGU_FMT_UTF16LE:
	case KDGU_FMT_UTF16BE:
	case KDGU_FMT_UTF16:
		k->s = utf16validate(k, s, &len, endian);
		break;
	case KDGU_FMT_UTF32LE:
	case KDGU_FMT_UTF32BE:
	case KDGU_FMT_UTF32:
		k->s = utf32validate(k, s, &len, endian);
		break;
	}

	if (!k->s) {
		free(k);
		return NULL;
	}

	k->len = len, k->alloc = k->len;
	kdgu_normalize(k, KDGU_NORM_NFC), safenize(k);

	return k;
}

kdgu *
kdgu_news(const char *s)
{
	return kdgu_new(KDGU_FMT_UTF8, (const uint8_t *)s, strlen(s));
}

kdgu *
kdgu_copy(const kdgu *k)
{
	kdgu *r = malloc(sizeof *r);
	if (!r) return NULL;
	memset(r, 0, sizeof *r);

	r->fmt = k->fmt;
	r->len = k->len;
	r->alloc = k->len;
	r->s = malloc(k->len);

	if (!r->s) return free(r), NULL;
	memcpy(r->s, k->s, k->len);

	return r;
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
kdgu_inc(const kdgu *k, unsigned *idx)
{
	if (!k || *idx >= k->len) return 0;
	unsigned now = *idx;

	switch (k->fmt) {
	case KDGU_FMT_CP1252:
	case KDGU_FMT_EBCDIC:
	case KDGU_FMT_ASCII:
		now++;
		break;

	case KDGU_FMT_UTF8:
		do {
			now++;
		} while (now < k->len && UTF8CONT(k->s[now]));
		break;

	case KDGU_FMT_UTF16BE:
	case KDGU_FMT_UTF16LE:
	case KDGU_FMT_UTF16:
		now += 2;
		if (now >= k->len) break;
		if (UTF16HIGH_SURROGATE(READUTF16(GETENDIAN(k->fmt),
		                                  k->s + now)))
			now += 2;
		break;

	case KDGU_FMT_UTF32BE:
	case KDGU_FMT_UTF32LE:
	case KDGU_FMT_UTF32:
		now += 4;
		break;
	}

	if (now > k->len) return 0;
	unsigned r = now - *idx;
	*idx = now;

	return r;
}

unsigned
kdgu_dec(const kdgu *k, unsigned *idx)
{
	if (!k || !idx) return 0;
	unsigned now = *idx;
	if (!now || now >= k->len) return 0;

	switch (k->fmt) {
	case KDGU_FMT_CP1252:
	case KDGU_FMT_EBCDIC:
	case KDGU_FMT_ASCII:
		now--;
		break;
	case KDGU_FMT_UTF8:
		do {
			now--;
		} while (now && UTF8CONT(k->s[now]));
		break;
	case KDGU_FMT_UTF16BE:
	case KDGU_FMT_UTF16LE:
	case KDGU_FMT_UTF16:
		now -= 2;
		if (!now) break;
		if (UTF16LOW_SURROGATE(READUTF16(GETENDIAN(k->fmt),
		                                 k->s + now - 2)))
			now -= 2;
		break;
	case KDGU_FMT_UTF32BE:
	case KDGU_FMT_UTF32LE:
	case KDGU_FMT_UTF32:
		now -= 4;
		break;
	}

	if (now >= k->len) return 0;

	unsigned r = *idx - now;
	*idx = now;

	return r;
}

bool
kdgu_nth(const kdgu *k, unsigned *idx, unsigned n)
{
	if (!k) return false;

	if (k->fmt == KDGU_FMT_ASCII
	    || k->fmt == KDGU_FMT_EBCDIC
	    || k->fmt == KDGU_FMT_CP1252) {
		if (n >= k->len) return false;
		*idx = n;
		return true;
	}

	if (k->fmt == KDGU_FMT_UTF32) {
		if (n >= k->len / 4) return false;
		*idx = n * 4;
		return true;
	}

	unsigned i;
	for (i = 0; i < n; i++)
		if (!kdgu_next(k, &i))
			return false;

	return *idx = i, true;
}

bool
kdgu_convert(kdgu *k, enum fmt fmt)
{
	if (!k) return false;
	if (!k->len || k->fmt == fmt) {
		k->fmt = fmt;
		return true;
	}

	unsigned idx = 0;
	int endian = GETENDIAN(k->fmt);
	if (!endian) endian = KDGU_ENDIAN_BIG;
	else {
		if (fmt == KDGU_FMT_UTF16LE
		    || fmt == KDGU_FMT_UTF32LE)
			endian = KDGU_ENDIAN_LITTLE;
	}

	while (idx < k->len) {
		unsigned len;
		uint32_t c = kdgu_decode(k, idx);
		uint8_t buf[4];

		struct error err =
			kdgu_encode(c, buf, &len,
				    fmt, idx, endian);

		if (err.kind) {
			err.codepoint = c;
			err.data = format[fmt];
			pusherror(k, err);

			kdgu_encode(KDGU_REPLACEMENT, buf, &len,
				    fmt, idx, endian);
		}

		delete_point(k, idx);
		insert_buffer(k, idx, buf, len);
		idx += len;
	}

	k->fmt = fmt;

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
		const struct codepoint *cp1 = codepoint(buf[i]);
		const struct codepoint *cp2 = codepoint(buf[i + 1]);

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
	if ((k->norm == KDGU_NORM_NFD && !compat)
	    || (k->norm == KDGU_NORM_NFKD && compat))
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
	for (unsigned i = 0; i < k->len; kdgu_inc(k, &i)) {
		uint32_t c = kdgu_decode(k, i);
		if (c == UINT32_MAX) break;
		const struct codepoint *cp = codepoint(c);

		/* printf("the thing is: %u\n", i); */

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
		delete_point(k, i);

		for (unsigned j = 0; j < len; j++) {
			insert_point(k, i, buf[j]);
			if (j != len - 1) kdgu_inc(k, &i);
		}
	}

	/* This loop sorts all sequences of combining marks. */
	for (unsigned i = 0; i < k->len; kdgu_inc(k, &i)) {
		uint32_t c = kdgu_decode(k, i);
		const struct codepoint *cp = codepoint(c);

		/* It's a starter. No sequence here! */
		if (!cp->ccc) continue;

		unsigned now = i;
		len = 0;

		/* Search for the end of the sequence. */
		do {
			buf[len++] = c;
			kdgu_inc(k, &i);

			c = kdgu_decode(k, i);
			cp = codepoint(c);
		} while (cp->ccc
		         && len < sizeof buf / sizeof *buf);

		sort_combining_marks(buf, len);

		/* Delete the sequence. */
		kdgu_delete(k, now, i);
		i = now;

		for (unsigned j = 0; j < len; j++) {
			insert_point(k, i, buf[j]);
			if (j != len - 1) kdgu_inc(k, &i);
		}
	}

	k->norm = compat ? KDGU_NORM_NFKD : KDGU_NORM_NFD;
}

static bool
compose_char(kdgu *k, unsigned idx)
{
	unsigned now = idx;

	/* Read the pair of code points. */
	uint32_t c1 = kdgu_decode(k, idx);
	if (!kdgu_inc(k, &idx) || idx == k->len) return false;
	uint32_t c2 = kdgu_decode(k, idx);

	idx = now;

	const struct codepoint *cp1 = codepoint(c1);
	const struct codepoint *cp2 = codepoint(c2);
	if (!cp1 || !cp2) return false;

	uint32_t composition = lookup_comp(c1, c2);
	/* No valid composition exists; do nothing. */
	if (composition == UINT32_MAX) return false;

	/*
	 * Only at this point are we sure we have a valid composition.
	 */

	kdgu_inc(k, &idx), kdgu_inc(k, &idx);
	kdgu_delete(k, now, idx);
	insert_point(k, now, composition);

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
	if ((k->norm == KDGU_NORM_NFC && !compat)
	    || (k->norm == KDGU_NORM_NFKC && compat))
		return;

	decompose(k, compat);

	for (unsigned i = 0; i < k->len; kdgu_inc(k, &i)) {
		/*
		 * We decrement here to offset the increment that will
		 * occur in the next iteration of the loop. This is
		 * done so that when a successful composition occurs
		 * the next pair will include the new composition code
		 * point; this correctly composes longer sequences of
		 * combining marks on a single starter code point.
		 */

		if (compose_char(k, i) && !kdgu_dec(k, &i)) {
			compose(k, compat);
			break;
		}
	}

	k->norm = compat ? KDGU_NORM_NFKC : KDGU_NORM_NFC;
}

bool
kdgu_normalize(kdgu *k, enum normalization norm)
{
	if (!k || !k->len) return false;
	if (k->fmt == KDGU_FMT_ASCII) return true;

	switch (norm) {
	case KDGU_NORM_NONE:                      break;
	case KDGU_NORM_NFD:  decompose(k, false); break;
	case KDGU_NORM_NFC:  compose(k, false);   break;
	case KDGU_NORM_NFKD: decompose(k, true);  break;
	case KDGU_NORM_NFKC: compose(k, true);    break;
	}

	return true;
}

bool
kdgu_cmp(const kdgu *k1, const kdgu *k2, bool insensitive, const char *locale)
{
	if (!k1 || !k2) return false;
	unsigned l1 = kdgu_len(k1);
	unsigned l2 = kdgu_len(k2);
	return kdgu_ncmp(k1, k2, 0, 0, l1 < l2 ? l1 : l2, insensitive, locale)
		&& (insensitive ? true : kdgu_len(k1) == kdgu_len(k2));
}

bool
kdgu_fuzzy(const kdgu *k1, const kdgu *k2)
{
	if (!k1 || !k2) return false;
	unsigned i = 0, j = 0;

	do {
		while (kdgu_chrcmp(k1, i, '-')
		       || kdgu_chrcmp(k1, i, '_')
		       || kdgu_chrcmp(k1, i, ' '))
			kdgu_inc(k1, &i);
		while (kdgu_chrcmp(k2, j, '-')
		       || kdgu_chrcmp(k2, j, '_')
		       || kdgu_chrcmp(k2, j, ' '))
			kdgu_inc(k2, &j);
		if (tolower(kdgu_decode(k1, i)) != tolower(kdgu_decode(k2, j)))
			return false;
	} while (kdgu_inc(k1, &i) && kdgu_inc(k2, &j));

	return i >= k1->len && j >= k2->len;
}

static uint32_t
fold(uint32_t c, struct locale l)
{
	if (c == 0x0049 && (l.lang == LANG_TUR || l.lang == LANG_AZE))
		c = 0x0131;
	if (c == 0x0130 && (l.lang == LANG_TUR || l.lang == LANG_AZE))
		c = 0x0069;
	return c;
}

bool
kdgu_ncmp(const kdgu *k1,
	  const kdgu *k2,
	  unsigned i,
	  unsigned j,
	  int n,
          bool insensitive,
          const char *locale)
{
	if (!k1 || !k2 || i > k1->len || j > k2->len) return false;
	int c = 0;
	struct locale L = parse_locale(locale);

	do {
		uint32_t c1 = fold(kdgu_decode(k1, i), L);
		uint32_t c2 = fold(kdgu_decode(k2, j), L);

		if (!insensitive) {
			if (c1 != c2) return false;
			continue;
		}

		uint32_t *seq1 = NULL;
		uint32_t *seq2 = NULL;
		unsigned l1 = lookup_fold(c1, &seq1);
		unsigned l2 = lookup_fold(c2, &seq2);
		uint32_t *a = (uint32_t []){c1};
		uint32_t *b = (uint32_t []){c2};

		if (!l1) {
			l1 = 1;
			seq1 = a;
		}

		if (!l2) {
			l2 = 1;
			seq2 = b;
		}

		if (l1 > l2) {
			kdgu_inc(k2, &j);
			seq2 = (uint32_t []){c2, kdgu_decode(k2, j)};
			l2++;
		} else if (l2 > l1) {
			kdgu_inc(k1, &i);
			if (kdgu_chrbound(k1, i)) c++;
			seq1 = (uint32_t []){c1, kdgu_decode(k1, i)};
			l1++;
		}

		if (l1 > l2) {
			uint32_t t = seq2[1];
			kdgu_inc(k2, &j);
			seq2 = (uint32_t []){c2, t, kdgu_decode(k2, j)};
			l2++;
		} else if (l2 > l1) {
			uint32_t t = seq1[1];
			kdgu_inc(k1, &i);
			if (kdgu_chrbound(k1, i)) c++;
			seq1 = (uint32_t []){c1, t, kdgu_decode(k1, i)};
			l1++;
		}

		for (unsigned z = 0; z < l1; z++)
			if (seq1[z] != seq2[z])
				return false;
	} while (i < k1->len
	         && j < k2->len
	         && (n < 0
	          ? (kdgu_chrbound(k1, i) ? ++c : c) < -n
	             && kdgu_dec(k1, &i) && kdgu_dec(k2, &j)
		  : (kdgu_chrbound(k1, i) ? ++c : c) < n
	             && kdgu_inc(k1, &i) && kdgu_inc(k2, &j))
		 && i < k1->len && j < k2->len);

	return c == abs(n);
}

static void
uputc(uint32_t c, FILE *f)
{
	uint8_t buf[100];
	unsigned len;
	struct error err = kdgu_encode(c, buf, &len, KDGU_FMT_UTF8, 0, KDGU_ENDIAN_NONE);
	if (err.kind) return;
	for (unsigned i = 0; i < len; i++)
		fputc(buf[i], f);
}

void
kdgu_print(const kdgu *k, FILE *f)
{
	if (!k) return;
	for (unsigned i = 0; i < k->len; kdgu_inc(k, &i))
		uputc(kdgu_decode(k, i), f);
}

void
kdgu_debugprint1(const kdgu *k, FILE *f)
{
	if (!k) return;

	fprintf(f, "{%u} <", k->len);

	for (unsigned i = 0; i < k->len; i++) {
		fprintf(f, "%02X", (uint8_t)k->s[i]);
		if (i != k->len - 1) fputc(' ', f);
	}

	fputc('>', f);
}

void
kdgu_debugprint2(const kdgu *k, FILE *f)
{
	if (!k) return;

	fprintf(f, "{%u} <", kdgu_len(k));
	unsigned i = 0;

	do {
		uint32_t c1 = kdgu_decode(k, i);
		if (c1 == UINT32_MAX) break;

		kdgu_inc(k, &i);
		uint32_t c2 = kdgu_decode(k, i);

		fprintf(f, "U+%02"PRIX32, c1);
		if (c2 == UINT32_MAX) break;

		if (grapheme_break(codepoint(c1)->bound,
		                   codepoint(c2)->bound))
			fprintf(f, " | ");
		else fputc(' ', f);
	} while (true);

	fputc('>', f);
}

void
kdgu_pchr(const kdgu *k, unsigned idx, FILE *f)
{
	if (!k || idx >= k->len) return;
	uputc(kdgu_decode(k, idx), f);
}

unsigned
kdgu_chomp(kdgu *k)
{
	unsigned i = k->len;

	while (kdgu_dec(k, &i) && kdgu_whitespace(k, i));
	kdgu_inc(k, &i);
	kdgu_delete(k, i, k->len);

	return k->len - i;
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
kdgu_decode(const kdgu *k, unsigned idx)
{
	if (!k->len || idx >= k->len) return UINT32_MAX;
	uint32_t c = 0;

	switch (k->fmt) {
	case KDGU_FMT_CP1252:
		return cp1252[k->s[idx]];

	case KDGU_FMT_EBCDIC:
		return ebcdic[k->s[idx]];

	case KDGU_FMT_ASCII:
		return (uint32_t)k->s[idx];

	case KDGU_FMT_UTF8: {
		unsigned len = utf8chrlen(k->s + idx,
		                          k->len - idx);

		c = (k->s[idx] & ((1 << (8 - len)) - 1))
			<< (len - 1) * 6;
		for (unsigned i = 1; i < len; i++)
			c |= (k->s[idx + i] & 0x3F)
				<< (len - i - 1) * 6;
	} break;

	case KDGU_FMT_UTF16BE:
	case KDGU_FMT_UTF16LE:
	case KDGU_FMT_UTF16: {
		uint16_t d = READUTF16(GETENDIAN(k->fmt), k->s + idx);
		if (d <= 0xD7FF || d >= 0xE000) return d;

		/* It's a surrogate upper byte. */
		uint16_t e = READUTF16(GETENDIAN(k->fmt),
		                       k->s + idx + 2);
		c = (d - 0xD800) * 0x400 + e - 0xDC00 + 0x10000;
	} break;

	case KDGU_FMT_UTF32BE:
	case KDGU_FMT_UTF32LE:
	case KDGU_FMT_UTF32:
		return READUTF32(GETENDIAN(k->fmt), k->s + idx);
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
	case KDGU_FMT_CP1252:
		if (c == KDGU_REPLACEMENT) c = '?';
		*len = 1, buf[0] = c & 0x7F;
		if (!IS_VALID_CP1252(c))
			ENCERR(kdgu_encode, buf, len,
			       KDGU_FMT_CP1252, idx, endian);
		break;

	case KDGU_FMT_EBCDIC: {
		if (c == KDGU_REPLACEMENT) c = '?';
		uint32_t o = -1;

		for (unsigned i = 0; i < 256 && o == UINT32_MAX; i++)
			o = (c == ebcdic[i]) ? i : o;

		*len = 1, buf[0] = o;
		if (o != UINT32_MAX) break;
		ENCERR(kdgu_encode, buf, len,
		       KDGU_FMT_EBCDIC, idx, endian);
	} break;

	case KDGU_FMT_ASCII:
		if (c == KDGU_REPLACEMENT) c = '?';
		*len = 1, buf[0] = c & 0xFF;
		if (c > 127)
			ENCERR(kdgu_encode, buf, len,
			       KDGU_FMT_ASCII, idx, endian);
		break;

	case KDGU_FMT_UTF8: err = utf8encode(c, buf, len, idx); break;
	case KDGU_FMT_UTF16LE:
	case KDGU_FMT_UTF16BE:
	case KDGU_FMT_UTF16:
		err = utf16encode(c, buf, len, idx, endian);
		break;

	case KDGU_FMT_UTF32LE:
	case KDGU_FMT_UTF32BE:
	case KDGU_FMT_UTF32:
		*len = 4;

		if (endian == KDGU_ENDIAN_LITTLE) {
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
kdgu_chrsize(const kdgu *k, unsigned idx)
{
	return kdgu_next(k, &idx);
}

bool
kdgu_chrbound(const kdgu *k, unsigned idx)
{
	uint32_t c = kdgu_decode(k, idx);
	if (!kdgu_inc(k, &idx)) return true;
	if (grapheme_break(codepoint(c)->bound,
			   codepoint(kdgu_decode(k, idx))->bound)) {
		kdgu_dec(k, &idx);
		return true;
	} else {
		kdgu_dec(k, &idx);
		return false;
	}

	return false;
}

bool
kdgu_chrcmp(const kdgu *k, unsigned idx, uint32_t c)
{
	return kdgu_decode(k, idx) == c && kdgu_chrbound(k, idx);
}

kdgu *
kdgu_substr(const kdgu *k, unsigned a, unsigned b)
{
	if (b < a || a > k->len || b > k->len) return NULL;
	return kdgu_new(k->fmt, k->s + a, b - a);
}

bool
kdgu_chrappend(kdgu *k, uint32_t c)
{
	if (!k) return false;
	return insert_point(k, k->len, c);
}

bool
kdgu_append(kdgu *k1, const kdgu *k2)
{
	if (!k1 || !k2) return false;
	for (unsigned i = 0; i < k2->len; kdgu_inc(k2, &i))
		if (!insert_point(k1, k1->len, kdgu_decode(k2, i)))
			return false;
	return true;
}

bool
kdgu_setappend(kdgu *k1, const kdgu *k2)
{
	if (!k1 || !k2) return false;
	for (unsigned i = 0; i < k2->len; kdgu_inc(k2, &i)) {
		uint32_t c = kdgu_decode(k2, i);
		if (kdgu_contains(k1, c)) continue;
		if (!insert_point(k1, k1->len, c))
			return false;
	}
	return true;
}

bool
kdgu_contains(const kdgu *k, uint32_t c)
{
	for (unsigned i = 0; i < k->len; kdgu_next(k, &i))
		if (kdgu_chrcmp(k, i, c))
			return true;
	return false;
}

kdgu *
kdgu_getchr(const kdgu *k, unsigned idx)
{
	return kdgu_new(k->fmt, k->s + idx, kdgu_chrsize(k, idx));
}

const struct codepoint *
kdgu_codepoint(uint32_t c)
{
	return codepoint(c);
}

const char *
kdgu_getname(uint32_t c)
{
	if (c > 0x10FFFF) return NULL;
	for (int i = 0; i < num_names; i++)
		if (c == names[i].c)
			return names[i].name;
	return NULL;
}

static kdgu *
fuzzify(const kdgu *k)
{
	kdgu *r = kdgu_new(k->fmt, NULL, 0);
	unsigned i = 0;

	do {
		if (kdgu_chrcmp(k, i, '-')
		       || kdgu_chrcmp(k, i, '_')
		       || kdgu_chrcmp(k, i, ' '))
			continue;
		kdgu_chrappend(r, tolower(kdgu_decode(k, i)));
	} while (kdgu_inc(k, &i));

	return r;
}

uint32_t
kdgu_getcode(const kdgu *k)
{
	for (int i = 0; i < num_names; i++)
		if (kdgu_fuzzy(k, &KDGU(names[i].name)))
			return names[i].c;

	for (int i = 0; i < num_name_aliases; i++)
		for (int j = 0; j < name_aliases[i].num; j++)
			if (kdgu_fuzzy(k, &KDGU(name_aliases[i].name[j])))
				return name_aliases[i].c;

	kdgu *r = fuzzify(k), *str = NULL;
	enum category cat = 0;

	/*
	 * The reserved and control categories are here because they
	 * are excluded by http://unicode.org/reports/tr18/#RL2.5 and
	 * handled in gen.pl, respectively.
	 */

	if (kdgu_ncmp(r, (cat = CATEGORY_CO, str = &KDGU("privateuse")), 0, 0, 10, false, NULL)
	    || kdgu_ncmp(r, (cat = CATEGORY_CS, str = &KDGU("surrogate")), 0, 0, 9, false, NULL)
	    || kdgu_ncmp(r, (cat = CATEGORY_CN, str = &KDGU("noncharacter")), 0, 0, 12, false, NULL)) {
		unsigned idx = 0;
		for (unsigned i = 0, n = kdgu_len(str); i < n; i++)
			kdgu_next(r, &idx);

		int32_t n = 0;
		uint32_t c = tolower(kdgu_decode(r, idx));

		while (idx < r->len
		       && ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))
		       && kdgu_chrbound(r, idx)) {
			if (n < 0) n = 0;
			n *= 16;
			n += (c >= 'a' && c <= 'f') ? c - 'a' + 10 : c - '0';
			kdgu_next(r, &idx);
			c = tolower(kdgu_decode(r, idx));
		}

		if (n <= 0x10FFFF && n >= 0 && idx == r->len
		    && codepoint(n)->category == cat)
			return n;
	}

	return UINT32_MAX;
}

uint32_t
kdgu_getcat(const kdgu *k)
{
	for (int i = 0; i < num_category_aliases; i++)
		if (kdgu_fuzzy(k, &KDGU(category_aliases[i].a))
		    || kdgu_fuzzy(k, &KDGU(category_aliases[i].b))
		    || (category_aliases[i].c && kdgu_fuzzy(k, &KDGU(category_aliases[i].c))))
			return category_aliases[i].cat;

	return UINT32_MAX;
}

char *
kdgu_getcatname(uint32_t c)
{
	for (int i = 0; i < num_category_aliases; i++)
		if (c == category_aliases[i].cat)
			return category_aliases[i].b;
	return NULL;
}

char *scripts[] = {
	"Adlam",
	"Caucasian_Albanian",
	"Ahom",
	"Arabic",
	"Imperial_Aramaic",
	"Armenian",
	"Avestan",
	"Balinese",
	"Bamum",
	"Bassa_Vah",
	"Batak",
	"Bengali",
	"Bhaiksuki",
	"Bopomofo",
	"Brahmi",
	"Braille",
	"Buginese",
	"Buhid",
	"Chakma",
	"Canadian_Aboriginal",
	"Carian",
	"Cham",
	"Cherokee",
	"Coptic",
	"Cypriot",
	"Cyrillic",
	"Devanagari",
	"Deseret",
	"Duployan",
	"Egyptian_Hieroglyphs",
	"Elbasan",
	"Ethiopic",
	"Georgian",
	"Glagolitic",
	"Masaram_Gondi",
	"Gothic",
	"Grantha",
	"Greek",
	"Gujarati",
	"Gurmukhi",
	"Hangul",
	"Han",
	"Hanunoo",
	"Hatran",
	"Hebrew",
	"Hiragana",
	"Anatolian_Hieroglyphs",
	"Pahawh_Hmong",
	"Katakana_Or_Hiragana",
	"Old_Hungarian",
	"Old_Italic",
	"Javanese",
	"Kayah_Li",
	"Katakana",
	"Kharoshthi",
	"Khmer",
	"Khojki",
	"Kannada",
	"Kaithi",
	"Tai_Tham",
	"Lao",
	"Latin",
	"Lepcha",
	"Limbu",
	"Linear_A",
	"Linear_B",
	"Lisu",
	"Lycian",
	"Lydian",
	"Mahajani",
	"Mandaic",
	"Manichaean",
	"Marchen",
	"Mende_Kikakui",
	"Meroitic_Cursive",
	"Meroitic_Hieroglyphs",
	"Malayalam",
	"Modi",
	"Mongolian",
	"Mro",
	"Meetei_Mayek",
	"Multani",
	"Myanmar",
	"Old_North_Arabian",
	"Nabataean",
	"Newa",
	"Nko",
	"Nushu",
	"Ogham",
	"Ol_Chiki",
	"Old_Turkic",
	"Oriya",
	"Osage",
	"Osmanya",
	"Palmyrene",
	"Pau_Cin_Hau",
	"Old_Permic",
	"Phags_Pa",
	"Inscriptional_Pahlavi",
	"Psalter_Pahlavi",
	"Phoenician",
	"Miao",
	"Inscriptional_Parthian",
	"Rejang",
	"Runic",
	"Samaritan",
	"Old_South_Arabian",
	"Saurashtra",
	"SignWriting",
	"Shavian",
	"Sharada",
	"Siddham",
	"Khudawadi",
	"Sinhala",
	"Sora_Sompeng",
	"Soyombo",
	"Sundanese",
	"Syloti_Nagri",
	"Syriac",
	"Tagbanwa",
	"Takri",
	"Tai_Le",
	"New_Tai_Lue",
	"Tamil",
	"Tangut",
	"Tai_Viet",
	"Telugu",
	"Tifinagh",
	"Tagalog",
	"Thaana",
	"Thai",
	"Tibetan",
	"Tirhuta",
	"Ugaritic",
	"Vai",
	"Warang_Citi",
	"Old_Persian",
	"Cuneiform",
	"Yi",
	"Zanabazar_Square",
	"Inherited",
	"Common",
	"Unknown"
};

int
kdgu_getscript(const kdgu *k)
{
	for (unsigned i = 0;
	     i < sizeof scripts / sizeof *scripts;
	     i++)
		if (kdgu_fuzzy(&KDGU(scripts[i]), k))
			return (int)i;
	return -1;
}

const char *
kdgu_getscriptname(enum script script)
{
	if (script > SCRIPT_UNKNOWN) return NULL;
	return scripts[script];
}

#define IS_HEX_DIGIT(X)	  \
	(((X) >= 'A' && (X) <= 'F') \
	 || ((X) >= 'a' && (X) <= 'f') \
	 || ((X) >= '0' && (X) <= '9'))

#define IS_DEC_DIGIT(X)\
	((X) >= '0' && (X) <= '9')

#define IS_OCT_DIGIT(X)\
	((X) >= '0' && (X) <= '7')

double
kdgu_hexadecimal(const kdgu *k, unsigned *idx)
{
	double n = 0;
	uint32_t c = kdgu_decode(k, *idx);

	while (IS_HEX_DIGIT(c) && kdgu_chrbound(k, *idx)) {
		c = tolower(c);
		n *= 16, n += !isdigit(c) ? c - 'a' + 10 : c - '0';
		if (!kdgu_inc(k, idx)) break;
		c = kdgu_decode(k, *idx);
	}

	return n;
}

double
kdgu_decimal(const kdgu *k, unsigned *idx)
{
	double n = 0;
	uint32_t c = kdgu_decode(k, *idx);

	while (IS_DEC_DIGIT(c) && kdgu_chrbound(k, *idx)) {
		n *= 10, n += c - '0';
		if (!kdgu_inc(k, idx)) break;
		c = kdgu_decode(k, *idx);
	}

	return n;
}

double
kdgu_octal(const kdgu *k, unsigned *idx)
{
	double n = 0;
	uint32_t c = kdgu_decode(k, *idx);

	while (IS_OCT_DIGIT(c) && kdgu_chrbound(k, *idx)) {
		n *= 10, n += c - '0';
		if (!kdgu_inc(k, idx)) break;
		c = kdgu_decode(k, *idx);
	}

	return n;
}

void
kdgu_move(const kdgu *k, unsigned *idx, int n)
{
	for (int i = 0; i < (n < 0 ? -n : n); i++)
		n < 0 ? kdgu_prev(k, idx) : kdgu_next(k, idx);
}
