#ifndef KDGU_H
#define KDGU_H

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

/* TODO: Make it so the things accept NULL/0 sometimes. */

enum endian {
	KDGU_ENDIAN_NONE,
	KDGU_ENDIAN_BIG,
	KDGU_ENDIAN_LITTLE
};

typedef struct kdgu {
	unsigned alloc, len;
	uint8_t *s;

	struct errorlist {
		struct error *err;
		unsigned num;
	} *errlist;

	enum normalization {
		KDGU_NORM_NONE,
		KDGU_NORM_NFC,
		KDGU_NORM_NFD,
		KDGU_NORM_NFKC,
		KDGU_NORM_NFKD
	} norm;

	enum fmt {
		KDGU_FMT_CP1252,
		KDGU_FMT_EBCDIC,
		KDGU_FMT_ASCII,

		KDGU_FMT_UTF8,

		KDGU_FMT_UTF16,
		KDGU_FMT_UTF16BE,
		KDGU_FMT_UTF16LE,

		KDGU_FMT_UTF32,
		KDGU_FMT_UTF32LE,
		KDGU_FMT_UTF32BE
	} fmt;
} kdgu;

#define GETENDIAN(X)	  \
	((X) == KDGU_FMT_UTF32LE || (X) == KDGU_FMT_UTF16LE \
	 ? KDGU_ENDIAN_LITTLE : KDGU_ENDIAN_BIG)

kdgu *kdgu_new(enum fmt fmt, const uint8_t *s, size_t len);
kdgu *kdgu_news(const char *s);
kdgu *kdgu_copy(const kdgu *k);
kdgu *kdgu_substr(const kdgu *k, unsigned a, unsigned b);
kdgu *kdgu_getchr(const kdgu *k, unsigned idx);

bool kdgu_nth(const kdgu *k, unsigned *idx, unsigned n);

bool kdgu_setchr(kdgu *k, unsigned idx, uint32_t c);
bool kdgu_uc(kdgu *k);
bool kdgu_lc(kdgu *k);
bool kdgu_reverse(kdgu *k);

bool kdgu_whitespace(const kdgu *k, unsigned idx);
bool kdgu_contains(const kdgu *k, uint32_t c);
bool kdgu_convert(kdgu *k, enum fmt fmt);
bool kdgu_normalize(kdgu *k, enum normalization norm);

bool kdgu_cmp(const kdgu *k1, const kdgu *k2);
bool kdgu_ncmp(const kdgu *k1, const kdgu *k2, unsigned i, unsigned j, int n, bool insensitive);
bool kdgu_fuzzy(const kdgu *k1, const kdgu *k2);

bool kdgu_append(kdgu *k1, const kdgu *k2);
bool kdgu_setappend(kdgu *k1, const kdgu *k2);

bool kdgu_chrbound(const kdgu *k, unsigned idx);
bool kdgu_chrcmp(const kdgu *k, unsigned n, uint32_t c);
bool kdgu_chrcat(kdgu *k, uint32_t c);
bool kdgu_chrappend(kdgu *k, uint32_t c);

unsigned kdgu_inc(const kdgu *k, unsigned *idx);
unsigned kdgu_dec(const kdgu *k, unsigned *idx);
unsigned kdgu_next(const kdgu *k, unsigned *idx);
unsigned kdgu_prev(const kdgu *k, unsigned *idx);
unsigned kdgu_len(const kdgu *k);
unsigned kdgu_chrsize(const kdgu *k, unsigned idx);
unsigned kdgu_chomp(kdgu *k);

void kdgu_delete(kdgu *k, size_t a, size_t b);
void kdgu_print(const kdgu *k, FILE *f);
void kdgu_debugprint1(const kdgu *k, FILE *f);
void kdgu_debugprint2(const kdgu *k, FILE *f);
void kdgu_pchr(const kdgu *k, unsigned idx, FILE *f);
void kdgu_print_error(struct error err);
void kdgu_free(kdgu *k);
void kdgu_size(kdgu *k, size_t n);

double kdgu_hexadecimal(const kdgu *k, unsigned *idx);
double kdgu_decimal(const kdgu *k, unsigned *idx);
double kdgu_octal(const kdgu *k, unsigned *idx);

uint32_t kdgu_decode(const kdgu *k, unsigned idx);
struct error kdgu_encode(uint32_t c, uint8_t *buf, unsigned *len,
			 enum fmt fmt, unsigned idx, int endian);

#include "unicode_data.h"

const struct codepoint *kdgu_codepoint(uint32_t c);
const char *kdgu_getname(uint32_t c);
uint32_t kdgu_getcode(const kdgu *k);
uint32_t kdgu_getcat(const kdgu *k);
char *kdgu_getcatname(uint32_t c);
int kdgu_getscript(const kdgu *k);
const char *kdgu_getscriptname(enum script script);

#include "error.h"
#include "ktre.h"

#define KDGU(X)							\
	(struct kdgu){0,strlen(X),(uint8_t *)X,NULL,KDGU_NORM_NFC,KDGU_FMT_UTF8}

#endif /* ifndef KDGU_H */
