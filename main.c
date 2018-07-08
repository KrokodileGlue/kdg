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

int
main(int argc, char **argv)
{
	unsigned len = 0;
	char *text = load_file(argv[argc - 1], &len);
	if (!text) return EXIT_FAILURE;
	char *filename = argv[argc - 1];

	/* kdgu *k = kdgu_new(FMT_UTF8, text, len); */

	/* kdgu_chomp(k); */
	/* kdgu_debugprint1(k); putchar('\n'); */
	/* kdgu_debugprint2(k); putchar('\n'); */
	/* kdgu_print(k); putchar('\n'); */

	/* kdgu_normalize(k, NORM_NFKC); */
	/* kdgu_uc(k); */
	/* kdgu_debugprint1(k); putchar('\n'); */
	/* kdgu_debugprint2(k); putchar('\n'); */
	/* kdgu_print(k); putchar('\n'); */

	/* printf("length: %u\n", kdgu_len(k)); */

	/* print_errors(k->errlist, filename); */
	/* free(text); */
	/* kdgu_free(k); */

	ktre *re = ktre_compile("[+-]?(\\d+(\\.\\d+)?|\\.\\d+)([eE][+-]?\\d+)?", KTRE_UNANCHORED | KTRE_GLOBAL | KTRE_DEBUG);
	ktre_exec(re, "-123.4e-56", NULL);

	ktre_free(re);
	free(text);

	return EXIT_SUCCESS;
}
