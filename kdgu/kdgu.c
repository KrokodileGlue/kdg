#define KDGU_DEBUG
#define KDGU_IMPLEMENTATION
#include "kdgu.h"

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

char *
load_file(const char *p, int *l)
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

int
main(int argc, char **argv)
{
	int len = 0;
	char *s1 = load_file(argv[argc - 1], &len);
	if (!s1) return EXIT_FAILURE;

	kdgu *k = kdgu_new(KDGU_FMT_UTF8, s1, len);
	free(s1);

	kdgu_chomp(k);
	kdgu_print(k);
	printf("\nlength: %zu\n", kdgu_len(k));

	kdgu *chr = kdgu_getnth(k, 0);

	printf("first character: '");
	kdgu_print(chr);
	puts("'");

	kdgu_free(chr);

	printf("also the first character: '");
	kdgu_nth(k, 0);
	kdgu_pchr(k);
	puts("'");

	if (k->errlist) {
		puts("The file contains invalid text:");

		for (unsigned i = 0; i < k->errlist->num; i++)
			printf("error:%s:%u: %s\n",
			       argv[argc - 1],
			       k->errlist->err[i].loc,
			       k->errlist->err[i].msg);
	}

	kdgu_free(k);

	return EXIT_SUCCESS;
}
