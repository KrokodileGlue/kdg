#include <stdlib.h>
#include <stdio.h>

#define KTRE_DEBUG
#define KTRE_IMPLEMENTATION
#include "ktre.h"

int
main(int argc, char *argv[])
{
	if (argc < 3 || argc > 4) {
		fprintf(stderr, "Usage: subject regex [replacement]");
		return EXIT_FAILURE;
	}

	char *subject = argv[1],
		*regex = argv[2],
		*replacement = (argc == 4) ? argv[3] : NULL;

	struct ktre *re = ktre_compile(regex, KTRE_GLOBAL);

	if (re->err) {
		ktre_free(re);
		fputc('\n', stderr);
		return EXIT_FAILURE;
	}

	if (replacement)
		free(ktre_filter(re, subject, replacement, "$"));
	else
		ktre_exec(re, subject, NULL);

	ktre_free(re);
	fputc('\n', stderr);

	return EXIT_SUCCESS;
}
