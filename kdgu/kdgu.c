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
	if (!f || fseek(f, 0L, SEEK_END)) return NULL;

	len = ftell(f);
	if (len == -1) return NULL;

	b = malloc(len + 1);
	if (!b) return NULL;

	if (fseek(f, 0L, SEEK_SET)) return NULL;

	len = fread(b, 1, len, f);
	if (ferror(f)) return NULL;

	b[len] = 0;
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

	kdgu *k = kdgu_new(s1, len, true);
	free(s1);

	kdgu_chomp(k);
	kdgu_print(k);
	printf("\nlength: %zu\n", kdgu_len(k));

	kdgu *chr = kdgu_getnth(k, 0);
	printf("first character: '"); kdgu_print(chr); puts("'");
	kdgu_free(chr);
	if (k->err) puts("The file contains invalid UTF-8.");
	kdgu_free(k);

	/* while (kdgu_inc(k)); */
	/* do kdgu_pchr(k); while (kdgu_dec(k)); */

	return EXIT_SUCCESS;
}
