#include <stdint.h>
#include <limits.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <inttypes.h>

#include "kdgu.h"

#include "unicode.h"
#include "encoding.h"
#include "error.h"

#include "utf8.h"
#include "utf16.h"
#include "utf32.h"

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
	"UTF-16-BE",
	"UTF-16-LE",
	"UTF-32",
	"UTF-32-LE",
	"UTF-32-BE"
};

_Static_assert(CHAR_BIT == 8,
               "kdgu only supports systems on"
               " which CHAR_BIT == 8 is true.");

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

void
kdgu_print_error(struct kdgu_error err)
{
	if (err.kind == KDGU_ERR_NO_CONVERSION) {
		printf(error[err.kind], err.codepoint, err.data);
	} else {
		printf("%s", error[err.kind]);
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
	if (u >= 0x110000) return codepoints;
	else return &codepoints[stage2table[stage1table[u >> 8]
	                                    + (u & 0xFF)]];
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

	char buf[4];
	unsigned len;

	struct kdgu_error err =
		kdgu_encode(k->fmt, c, buf, &len, k->idx, k->endian);

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
insert_buffer(kdgu *k, char *buf, unsigned len)
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
	do l++; while (kdgu_next(k));

	return l;
}

static struct kdgu_case *
get_special_case(uint32_t c)
{
	for (unsigned i = 0; i < num_special_case; i++)
		if (special_case[i].c == c)
			return &special_case[i];

	return NULL;
}

bool
kdgu_uc(kdgu *k)
{
	if (!k || !k->len) return false;
	k->idx = 0;

	do {
		uint32_t c = kdgu_decode(k);
		uint32_t u = upperize(c);

		if (u != c) {
			delete_point(k);
			insert_point(k, u);
			continue;
		}

		struct kdgu_case *sc = get_special_case(c);

		if (!sc || !sc->upper_len) continue;
		delete_point(k);

		for (unsigned i = 0; i < sc->upper_len; i++) {
			insert_point(k, sc->upper[i]);
			kdgu_inc(k);
		}

		kdgu_dec(k);
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
		uint32_t u = lowerize(c);

		if (u != c) {
			delete_point(k);
			insert_point(k, u);
			continue;
		}

		struct kdgu_case *sc = get_special_case(c);

		if (!sc || !sc->lower_len) continue;
		delete_point(k);

		for (unsigned i = 0; i < sc->lower_len; i++) {
			insert_point(k, sc->lower[i]);
			kdgu_inc(k);
		}

		kdgu_dec(k);
	} while (kdgu_inc(k));

	return true;
}

bool
kdgu_reverse(kdgu *k)
{
	if (!k || !k->len || k->len == 1) return false;

	unsigned end = kdgu_len(k) - 1;
	k->idx = 0; while (kdgu_next(k));
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

	case KDGU_FMT_UTF16LE: endian = ENDIAN_LITTLE;
	case KDGU_FMT_UTF16BE: if (!endian) endian = ENDIAN_BIG;
	case KDGU_FMT_UTF16:
		k->s = utf16validate(k, s, &len, endian);
		break;

	case KDGU_FMT_UTF32LE: endian = ENDIAN_LITTLE;
	case KDGU_FMT_UTF32BE: if (!endian) endian = ENDIAN_BIG;
	case KDGU_FMT_UTF32:
		k->s = utf32validate(k, s, &len, endian);
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
	if (!k1 || !k2 || k1->fmt != k2->fmt) return true;

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

	case KDGU_FMT_UTF16BE:
	case KDGU_FMT_UTF16LE:
	case KDGU_FMT_UTF16:
		idx += 2;
		if (idx >= k->len) break;
		if (UTF16HIGH_SURROGATE(READUTF16(k->endian,
		                                  k->s + idx)))
			idx += 2;
		break;

	case KDGU_FMT_UTF32BE:
	case KDGU_FMT_UTF32LE:
	case KDGU_FMT_UTF32:
		idx += 4;
		break;
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

	case KDGU_FMT_UTF16BE:
	case KDGU_FMT_UTF16LE:
	case KDGU_FMT_UTF16:
		idx -= 2;
		if (UTF16LOW_SURROGATE(READUTF16(k->endian,
		                                 k->s + idx - 2)))
			idx -= 2;
		break;

	case KDGU_FMT_UTF32BE:
	case KDGU_FMT_UTF32LE:
	case KDGU_FMT_UTF32:
		idx -= 4;
		break;
	}

	unsigned r = k->idx - idx;
	k->idx = idx;

	return r;
}

bool
kdgu_nth(kdgu *k, unsigned n)
{
	if (!k) return false;

	if (k->fmt == KDGU_FMT_ASCII
	    || k->fmt == KDGU_FMT_EBCDIC
	    || k->fmt == KDGU_FMT_CP1252) {
		if (n >= k->len) return false;
		k->idx = n;
		return true;
	}

	if (k->fmt == KDGU_FMT_UTF32) {
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
kdgu_convert(kdgu *k, enum kdgu_fmt fmt)
{
	if (!k) return false;
	if (!k->len || k->fmt == fmt) {
		k->fmt = fmt;
		return true;
	}

	k->idx = 0;
	int endian = k->endian;
	if (!endian) endian = ENDIAN_BIG;

	while (true) {
		unsigned len;
		uint32_t c = kdgu_decode(k);
		char buf[4];

		struct kdgu_error err =
			kdgu_encode(fmt, c, buf,
			            &len, k->idx, endian);

		if (err.kind) {
			err.codepoint = c;
			err.data = format[fmt];
			pusherror(k, err);

			kdgu_encode(fmt, KDGU_REPLACEMENT, buf,
			            &len, k->idx, endian);
		}

		delete_point(k);
		insert_buffer(k, buf, len);
		k->idx += len;

		if (k->idx >= k->len) break;
	}

	k->fmt = fmt;
	k->endian = endian;

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

unsigned
kdgu_chomp(kdgu *k)
{
	k->idx = 0;

	while (kdgu_inc(k));
	while (kdgu_whitespace(k) && kdgu_dec(k));
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

	case KDGU_FMT_UTF16BE:
	case KDGU_FMT_UTF16LE:
	case KDGU_FMT_UTF16: {
		uint16_t d = READUTF16(k->endian, k->s + k->idx);
		if (d <= 0xD7FF || d >= 0xE000) return d;

		/* It's a surrogate upper byte. */
		uint16_t e = READUTF16(k->endian, k->s + k->idx + 2);
		c = (d - 0xD800) * 0x400 + e - 0xDC00 + 0x10000;
	} break;

	case KDGU_FMT_UTF32BE:
	case KDGU_FMT_UTF32LE:
	case KDGU_FMT_UTF32:
		return READUTF32(k->endian, k->s + k->idx);
	}

	return c;
}

/* TODO: Make sure NO_CONVERSION errors are consistent. */
/* TODO: KDGU_REPLACEMENT doesn't work for most encodings. */
struct kdgu_error
kdgu_encode(enum kdgu_fmt fmt, uint32_t c, char *buf,
            unsigned *len, unsigned idx, int endian)
{
	struct kdgu_error err = ERR(KDGU_ERR_NO_ERROR, 0);

	switch (fmt) {
	case KDGU_FMT_CP1252:
		*len = 1;
		buf[0] = c & 0x7F;

		if (!IS_VALID_CP1252(c)) {
			err = ERR(KDGU_ERR_NO_CONVERSION, idx);
			buf[0] = KDGU_REPLACEMENT;
		}
		break;

	/* TODO: Move this stuff out. */
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
		*len = 1;
		if (c > 127) {
			err = ERR(KDGU_ERR_NO_CONVERSION, idx);
			buf[0] = KDGU_REPLACEMENT;
			break;
		}

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
		} else {
			err = ERR(KDGU_ERR_NO_CONVERSION, idx);
			*len = 1;
		}
		break;

	case KDGU_FMT_UTF16LE:
	case KDGU_FMT_UTF16BE:
	case KDGU_FMT_UTF16:
		*len = 2;
		if (c <= 0xD7FF || (c >= 0xE000 && c <= 0xFFFF)) {
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

	case KDGU_FMT_UTF32LE:
	case KDGU_FMT_UTF32BE:
	case KDGU_FMT_UTF32:
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
	unsigned idx = k->idx;
	unsigned c = kdgu_next(k);
	unsigned ret = k->len - k->idx;
	k->idx = idx;
	if (c) ret = c;
	return ret;
}

bool
kdgu_append(kdgu *k, const char *s, size_t l)
{
	kdgu_size(k, k->len + l);
	memcpy(k->s + k->len, s, l);
	k->len = k->len + l;

	return true;
}
