#include "kdgu.h"

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <inttypes.h>

char *
load_file(const char *p, unsigned *l)
{
	char *b = NULL;
	FILE *f = NULL;
	int len = -1;

	f = fopen(p, "rb");

	if (!f) return NULL;
	if (fseek(f, 0L, SEEK_END)) return fclose(f), NULL;

	len = ftell(f);
	if (len == -1) return fclose(f), NULL;

	b = malloc(len);

	if (!b) return fclose(f), NULL;
	if (fseek(f, 0L, SEEK_SET)) return fclose(f), free(b), NULL;

	len = fread(b, 1, len, f);

	if (ferror(f)) return fclose(f), free(b), NULL;
	if (l) *l = len;

	fclose(f);
	return b;
}

void
print_errors(struct errorlist *errlist, char *path)
{
	if (!errlist) return;

	for (unsigned i = 0; i < errlist->num; i++) {
		printf("error:%s:%u: ",
		       path, errlist->err[i].loc);

		kdgu_print_error(errlist->err[i]);
		putchar('\n');
	}
}

char *filename = NULL;

void
demo(char *text, unsigned len)
{
	kdgu *q = kdgu_new(FMT_UTF8, text, len);
	kdgu_convert(q, FMT_UTF16);

	kdgu_chomp(q);
	kdgu_uc(q);
	/* kdgu_lc(q); */
	kdgu_reverse(q);
	kdgu_convert(q, FMT_UTF8);

	kdgu *r = kdgu_copy(q);

	kdgu_convert(r, FMT_UTF16);
	kdgu_convert(r, FMT_UTF8);

	/* assert(kdgu_cmp(q, r)); */

	/*
	 * TODO: It seems that non shortest form UTF-8 sequences are
	 * not being properly caught?
	 *
	 * This stuff only makes sense if the format of r and q are
	 * the same, of course.
	 *
	 * assert(r->len == q->len);
	 * assert(!memcmp(r->s, q->s, r->len));
	 */

#if 0
	for (unsigned i = 0; i < q->len; i++) {
		if (q->s[i] != r->s[i]) {
			printf("0x%X and 0x%X at %u\n",
			       (unsigned char)q->s[i],
			       (unsigned char)r->s[i],
			       i);
		}
	}
#endif

	kdgu_print(r); putchar('\n');
	printf("length: %zu\n", kdgu_len(r));

	printf("first character: '");
	kdgu_nth(r, 0); kdgu_pchr(r, stdout);
	puts("'");

	printf("last character: '");
	kdgu_nth(r, kdgu_len(r) - 1); kdgu_pchr(r, stdout);
	puts("'");

	print_errors(q->errlist, filename);
	print_errors(r->errlist, filename);

	kdgu_free(q);
	kdgu_free(r);

	/* ========================== */
	kdgu *a = kdgu_news("foo ");
	kdgu *b = kdgu_news("bar");

	kdgu_convert(a, FMT_UTF8);
	kdgu_convert(b, FMT_UTF8);

	kdgu_uc(b);
	kdgu_cat(a, b);

	kdgu_print(a); putchar('\n');

	kdgu_free(a);
	kdgu_free(b);
}

int
main(int argc, char **argv)
{
	unsigned len = 0;
	char *text = load_file(argv[argc - 1], &len);
	if (!text) return EXIT_FAILURE;
	filename = argv[argc - 1];

#if 1
	kdgu *k = kdgu_new(FMT_UTF8, text, len);

	kdgu_chomp(k);
	kdgu_debugprint1(k); putchar('\n');
	kdgu_debugprint2(k); putchar('\n');
	kdgu_print(k); putchar('\n');

	kdgu_normalize(k, NORM_NFC);
	kdgu_uc(k);
	kdgu_debugprint1(k); putchar('\n');
	kdgu_debugprint2(k); putchar('\n');
	kdgu_print(k); putchar('\n');

	printf("length: %u\n", kdgu_len(k));

	print_errors(k->errlist, filename);
	free(text);
	kdgu_free(k);
#elif 0
	kdgu *k = kdgu_new(FMT_UTF32, text, len);
	kdgu_convert(k, FMT_UTF8);
	kdgu_chomp(k);

	kdgu_print(k); putchar('\n');
	printf("length: %u\n", kdgu_len(k));

	kdgu_reverse(k);
	kdgu_uc(k);

	kdgu_print(k); putchar('\n');
	printf("length: %u\n", kdgu_len(k));

	print_errors(k->errlist, filename);
	free(text);
	kdgu_free(k);
#else
	filename = argv[argc - 1];
	demo(text, len);
	free(text);
#endif

	return EXIT_SUCCESS;
}
