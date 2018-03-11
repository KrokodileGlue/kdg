#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#define KDGU_DEBUG
#define KDGU_IMPLEMENTATION
#include "kdgu.h"

char *
load_file(const char *path, int *l)
{
	char *b = NULL;
	FILE *f = fopen(path, "r");
	if (!f) return NULL;

	if (fseek(f, 0L, SEEK_END) == 0) {
		int len = ftell(f);
		if (len == -1) goto err;
		b = malloc(len + 1);
		if (fseek(f, 0L, SEEK_SET)) goto err;
		len = fread(b, 1, len, f);
		if (ferror(f))  goto err;
		else b[len++] = 0;
		if (l) *l = len;
	}

	fclose(f);
	return b;

 err:
	if (f) fclose(f);
	return NULL;
}

int
main(void)
{
	int len = 0;
	char *s1 = load_file("test.txt", &len);
	kdgu *k = kdgu_new(s1, len);
	free(s1);

	while (kdgu_inc(k));
	do kdgu_pchr(k); while (kdgu_dec(k));
	kdgu_free(k);

	return EXIT_SUCCESS;
}
