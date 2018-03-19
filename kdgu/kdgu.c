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

	f = fopen(p, "r");

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
print_errors(struct kdgu_errorlist *errlist, int argc, char **argv)
{
	if (!errlist) return;

	for (unsigned i = 0; i < errlist->num; i++) {
		printf("error:%s:%u: ",
		       argv[argc - 1],
		       errlist->err[i].loc);
		kdgu_print_error(errlist->err[i]);
		putchar('\n');
	}
}

int
main(int argc, char **argv)
{
	unsigned len = 0;
	char *s1 = load_file(argv[argc - 1], &len);
	if (!s1) return EXIT_FAILURE;

	kdgu *u8 = kdgu_new(KDGU_FMT_UTF8, s1, len);
	kdgu_chomp(u8);

	kdgu *u16 = kdgu_convert(u8, KDGU_FMT_UTF16);
	kdgu *t = kdgu_convert(u16, KDGU_FMT_UTF8);
	kdgu *t2 = kdgu_copy(t);

	kdgu_print(t2);
	printf("\nlength: %zu\n", kdgu_len(t));

	printf("first character: '");
	kdgu_nth(t, 0); kdgu_pchr(t, stdout);
	puts("'");

	printf("last character: '");
	kdgu_nth(t, kdgu_len(t) - 1); kdgu_pchr(t, stdout);
	puts("'");

	/* print_errors(u16->errlist, argc, argv); */
	/* print_errors(u8->errlist, argc, argv); */
	/* print_errors(t->errlist, argc, argv); */

	kdgu_free(u8);
	kdgu_free(u16);
	kdgu_free(t);
	kdgu_free(t2);
	free(s1);

	return EXIT_SUCCESS;
}
