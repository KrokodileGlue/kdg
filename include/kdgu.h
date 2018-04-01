#ifndef KDGU_H
#define KDGU_H

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

typedef struct kdgu {
	unsigned alloc, len, idx;
	char *s;

	struct kdgu_errorlist {
		struct kdgu_error *err;
		unsigned num;
	} *errlist;

	enum kdgu_fmt {
		KDGU_FMT_CP1252,
		KDGU_FMT_EBCDIC,
		KDGU_FMT_ASCII,
		KDGU_FMT_UTF8,
		KDGU_FMT_UTF16,
		KDGU_FMT_UTF16BE,
		KDGU_FMT_UTF16LE
	} fmt;

	enum kdgu_endian {
		ENDIAN_NONE,
		ENDIAN_BIG,
		ENDIAN_LITTLE
	} endian;
} kdgu;

kdgu *kdgu_new(enum kdgu_fmt fmt, const char *s, size_t len);
kdgu *kdgu_news(const char *s);
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

bool kdgu_convert(kdgu *k, enum kdgu_fmt fmt);

bool kdgu_cmp(kdgu *k1, kdgu *k2);
bool kdgu_nth(kdgu *k, unsigned n);
bool kdgu_append(kdgu *k, const char *s, size_t l);

void kdgu_delete(kdgu *k, size_t a, size_t b);
void kdgu_print(kdgu *k);
void kdgu_debugprint(kdgu *k);
void kdgu_pchr(kdgu *k, FILE *f);
void kdgu_print_error(struct kdgu_error err);

uint32_t kdgu_decode(kdgu *k);
struct kdgu_error kdgu_encode(enum kdgu_fmt fmt, uint32_t c,
                              char *buf, unsigned *len, unsigned idx);

unsigned kdgu_len(kdgu *k);
unsigned kdgu_chrsize(kdgu *k);
unsigned kdgu_chomp(kdgu *k);

void kdgu_size(kdgu *k, size_t n);

#include "error.h"

#endif /* ifndef KDGU_H */
