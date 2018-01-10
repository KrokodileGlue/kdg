#include <stdlib.h>
#include <stdio.h>

#define KTRE_DEBUG
#define KTRE_IMPLEMENTATION
#include "ktre.h"

int
main(int argc, char *argv[])
{
	if (argc < 3 || argc > 4) {
		fprintf(stderr, "Usage: subject regex [replacement]\n");
		return EXIT_FAILURE;
	}

	char *command = argv[1];
	char *subject = argv[2],
		*regex = argv[3],
		*replacement = (argc == 5) ? argv[4] : NULL;

	struct ktre *re = ktre_compile(regex, KTRE_GLOBAL | KTRE_INSENSITIVE);

	if (!strcmp(command, "split")) {
		int len = 0;
		char **split = ktre_split(re, subject, &len);
		fputc('\n', stderr);

		for (int i = 0; i < len; i++) {
			fprintf(stderr, "split %2d: %s\n", i, split[i]);
			free(split[i]);
		}

		free(split);
	} else if (!strcmp(command, "match")) {
		ktre_exec(re, subject, NULL);
	} else if (!strcmp(command, "replace")) {
		free(ktre_filter(re, subject, replacement, "$"));
	}

	if (re->err) {
		ktre_free(re);
		fputc('\n', stderr);
		return EXIT_FAILURE;
	}

	ktre_free(re);
	fputc('\n', stderr);

	return EXIT_SUCCESS;
}
