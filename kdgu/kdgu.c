#define KDGU_DEBUG
#define KDGU_IMPLEMENTATION
#include "kdgu.h"

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
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
print_errors(struct kdgu_errorlist *errlist, char *path)
{
	if (!errlist) return;

	for (unsigned i = 0; i < errlist->num; i++) {
		printf("error:%s:%u: ",
		       path, errlist->err[i].loc);

		kdgu_print_error(errlist->err[i]);
		putchar('\n');
	}
}

int
main(int argc, char **argv)
{
	unsigned len = 0;
	char *text = load_file(argv[argc - 1], &len);
	if (!text) return EXIT_FAILURE;

	kdgu *q = kdgu_new(KDGU_FMT_EBCDIC, text, len);
	kdgu_chomp(q);
	kdgu_uc(q);

	kdgu *r = kdgu_copy(q);
	kdgu_convert(r, KDGU_FMT_UTF16);
	kdgu_convert(r, KDGU_FMT_UTF8);

	assert(kdgu_cmp(q, r));
	assert(r->len == q->len);

	/*
	 * TODO: It seems that non shortest form UTF-8 sequences are
	 * not being properly caught?
	 *
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

	kdgu_print(q); putchar('\n');
	kdgu_print(r); putchar('\n');
	printf("length: %zu\n", kdgu_len(q));
	printf("length: %zu\n", kdgu_len(r));

	printf("first character: '");
	kdgu_nth(r, 0); kdgu_pchr(r, stdout);
	puts("'");

	printf("last character: '");
	kdgu_nth(r, kdgu_len(r) - 1); kdgu_pchr(r, stdout);
	puts("'");

	print_errors(q->errlist, argv[argc - 1]);
	print_errors(r->errlist, argv[argc - 1]);

	kdgu_free(q);
	kdgu_free(r);
	free(text);

	/* ========================== */
	kdgu *a = kdgu_news("foo ");
	kdgu *b = kdgu_news("bar");
	kdgu_uc(b);
	kdgu_cat(a, b);

	kdgu_print(a); putchar('\n');

	kdgu_free(a);
	kdgu_free(b);

	return EXIT_SUCCESS;
}
