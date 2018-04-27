#ifndef KDGU_H
#define KDGU_H

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "unicode_data.h"

typedef struct kdgu {
	unsigned alloc, len, idx;
	uint8_t *s;
	enum normalization norm;

	struct errorlist {
		struct error *err;
		unsigned num;
	} *errlist;

	enum fmt {
		FMT_CP1252,
		FMT_EBCDIC,
		FMT_ASCII,

		FMT_UTF8,

		FMT_UTF16,
		FMT_UTF16BE,
		FMT_UTF16LE,

		FMT_UTF32,
		FMT_UTF32LE,
		FMT_UTF32BE
	} fmt;

	enum endian {
		ENDIAN_NONE,
		ENDIAN_BIG,
		ENDIAN_LITTLE
	} endian;
} kdgu;

kdgu *kdgu_new(enum fmt fmt, const uint8_t *s, size_t len);
kdgu *kdgu_news(const uint8_t *s);
void kdgu_free(kdgu *k);

kdgu *kdgu_copy(kdgu *k);
kdgu *kdgu_getnth(kdgu *k, unsigned n);

bool kdgu_cat(kdgu *k1, kdgu *k2);
bool kdgu_uc(kdgu *k);
bool kdgu_lc(kdgu *k);
bool kdgu_reverse(kdgu *k);
bool kdgu_whitespace(kdgu *k);

unsigned kdgu_inc(kdgu *k);
unsigned kdgu_dec(kdgu *k);

unsigned kdgu_next(kdgu *k);
unsigned kdgu_prev(kdgu *k);

bool kdgu_convert(kdgu *k, enum fmt fmt);
bool kdgu_normalize(kdgu *k, enum normalization norm);

bool kdgu_cmp(kdgu *k1, kdgu *k2);
bool kdgu_nth(kdgu *k, unsigned n);
bool kdgu_append(kdgu *k, const uint8_t *s, size_t l);

void kdgu_delete(kdgu *k, size_t a, size_t b);
void kdgu_print(kdgu *k);
void kdgu_debugprint1(kdgu *k);
void kdgu_debugprint2(kdgu *k);
void kdgu_pchr(kdgu *k, FILE *f);
void kdgu_print_error(struct error err);

uint32_t kdgu_decode(kdgu *k);
struct error kdgu_encode(enum fmt fmt, uint32_t c,
                         uint8_t *buf, unsigned *len,
                         unsigned idx, int endian);

unsigned kdgu_len(kdgu *k);
unsigned kdgu_chrsize(kdgu *k);
unsigned kdgu_chomp(kdgu *k);

void kdgu_size(kdgu *k, size_t n);

#include "error.h"

#endif /* ifndef KDGU_H */
