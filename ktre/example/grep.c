#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#endif

#define KTRE_IMPLEMENTATION
//#define KTRE_DEBUG
#include "../ktre.h"

static void
grepfile(const char *name, struct ktre *re, bool ploc)
{
	FILE* f = fopen(name, "r");
	if (!f) return;
	char line[1024];

	int i = 0;
	while (fgets(line, sizeof line, f)) {
		i++;
		if (ktre_exec(re, line, NULL)) {
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
grep(const char *dir, struct ktre *re, bool rec, bool ploc)
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
				if (rec) grep(path, re, rec, ploc);
			} else {
				grepfile(path, re, ploc);
			}
		}
	} while(FindNextFile(handle, &fd));

	FindClose(handle);
}
#endif

#ifdef __GNUC__
#include <dirent.h>

static void
grep(const char *dir, struct ktre *re, bool rec, bool ploc)
{
	DIR *d = opendir(dir);
	if (!d) return;

	char path[128];

	struct dirent *directory;
	while ((directory = readdir(d))) {
		if (directory->d_type != DT_DIR) {
			sprintf(path, "%s/%s", dir, directory->d_name);
			grepfile(path, re, ploc);
		}
	}

	closedir(d);
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

	if (!file || !pat) {
		puts("Usage: [options] pattern file");
	}

	struct ktre *re = ktre_compile(pat, KTRE_UNANCHORED);
	grep(file, re, rec, ploc);
	ktre_free(re);

	return 0;
}
