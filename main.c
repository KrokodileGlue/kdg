#include "kdgu.h"
#include <assert.h>

char *
load_file(const char *p, unsigned *l)
{
	char *b = NULL;
	FILE *f = NULL;
	int len = -1;

	*l = 0;
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
	*l = len, fclose(f);

	return b;
}

int
main(int argc, char **argv)
{
	unsigned len = 0;
	char *text = load_file(argv[argc - 1], &len);
	if (!text) return EXIT_FAILURE;

	char *a = "foo (?Si)bar baz(?-Si) heck";
	char *b = "foo bar          BAZ heck";
	kdgu *c = kdgu_news(a);
	kdgu *d = kdgu_news(b);

	ktre *re = ktre_compile(c, KTRE_UNANCHORED | KTRE_GLOBAL | KTRE_DEBUG);
	if (!re->err) ktre_exec(re, d, NULL);

	kdgu_free(c), kdgu_free(d);
	ktre_free(re);

	assert(kdgu_cmp(&KDGU("<süß>"), &KDGU("<SÜẞ>"), true));
	assert(kdgu_cmp(&KDGU("<süß>"), &KDGU("<SÜss>"), true));
	assert(kdgu_cmp(&KDGU("<süss>"), &KDGU("<SÜß>"), true));
	assert(kdgu_cmp(&KDGU("<süss>"), &KDGU("<SÜẞ>"), true));
	assert(kdgu_cmp(&KDGU("ThiS IS FoO bAR"), &KDGU("this is foo bar"), true));
	assert(!kdgu_cmp(&KDGU("ThiS IS FoO bAR"), &KDGU("this is foo bar"), false));
	assert(kdgu_cmp(&KDGU("ⓕⓞⓞ ⓑⓐⓡ ⓑⓐⓩ"), &KDGU("ⒻⓄⓄ ⒷⒶⓇ ⒷⒶⓏ"), true));
	assert(kdgu_cmp(&KDGU("ὒ"), &KDGU("\U000003C5\U00000313\U00000300"), true));
	assert(kdgu_cmp(&KDGU("\U00000130"), &KDGU("i\U00000307"), true));
	/* assert(kdgu_cmp(&KDGU("ı"), &KDGU("I"), true)); */

	free(text);

	return 0;
}
