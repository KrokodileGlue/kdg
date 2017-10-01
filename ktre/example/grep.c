#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#endif

#define KTRE_IMPLEMENTATION
#include "../ktre.h"

static void
grepfile(const char *name, const char *pat, bool ploc)
{
	FILE* f = fopen(name, "r");
	if (!f) return;
	char line[256];

	int i = 0;
	while (fgets(line, sizeof line, f)) {
		i++;
		if (ktre_match(line, pat, KTRE_GLOBAL | KTRE_UNANCHORED, NULL)) {
			if (ploc)
				printf("%s:%d: %s", name, i, line);
			else
				printf("%s", line);
		}
	}

	fclose(f);
}

#ifdef _WIN32
static void
grep(const char *dir, const char *pat, bool rec, bool ploc)
{
	WIN32_FIND_DATA fd;
	char path[2048];
	sprintf(path, "%s\\*.*", dir);
	HANDLE handle = FindFirstFile(path, &fd);

	if (handle == INVALID_HANDLE_VALUE)
		return;

	do {
		if (strcmp(fd.cFileName, ".")
		   && strcmp(fd.cFileName, "..")) {
			sprintf(path, "%s\\%s", dir, fd.cFileName);
			if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
				if (rec) grep(path, pat, rec, ploc);
			} else {
				grepfile(path, pat, ploc);
			}
		}
	} while(FindNextFile(handle, &fd));

	FindClose(handle);
}
#endif

int
main(int argc, char *argv[])
{
	bool rec = false, ploc = false;
	char *file = NULL, *pat = NULL;

	bool doing_options = true;
	for (int i = 1; i < argc; i++) {
		if (doing_options) {
			if (!strcmp(argv[i], "-r")) rec = true;
			else if (!strcmp(argv[i], "-s")) ploc = true;
			else {
				doing_options = false;
				i--;
			}
			continue;
		} else {
			if (!pat) pat = argv[i];
			else file = argv[i];
		}
	}

	grep(file, pat, rec, ploc);
	return 0;
}
