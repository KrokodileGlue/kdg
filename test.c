#include <stdlib.h>
#include <stdio.h>

#define KTRE_IMPLEMENTATION
#include "ktre.h"

static inline void
do_regex(struct ktre *re, const char *subject, FILE *f)
{
	int **vec = NULL;

	if (ktre_exec(re, subject, &vec)) {
		for (int i = 0; i < re->num_matches; i++) {
			for (int j = 0; j < re->num_groups; j++) {
				if (vec[i][j * 2 + 1] && (int)strlen(subject) != vec[i][j * 2]) {
					fprintf(f, "%.*s\n", vec[i][j * 2 + 1], subject + vec[i][j * 2]);
				}
			}
		}
	} else if (re->err) {
		fprintf(f, "runtime error.\n");
	} else {
		fprintf(f, "no match.\n");
	}
}

int
main(int argc, char *argv[])
{
	if (argc < 3) return 0;

	char *subject = argv[1], *regex = argv[2];
	struct ktre *re = ktre_compile(regex, KTRE_UNANCHORED);

	FILE *f = fopen("output.txt", "w");

	if (re->err) {
		fprintf(f, "syntax error.\n");
		return EXIT_FAILURE;
	}

	do_regex(re, subject, f);
	ktre_free(re);

	return EXIT_SUCCESS;
}
