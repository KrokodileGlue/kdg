#include <string.h>
#include "locale.h"

#define LOCALE_DEFAULT (struct locale){LANG_ENG, SCRIPT_LATIN, REG_US}

struct locale
parse_locale(const char *locale)
{
	if (!locale) return LOCALE_DEFAULT;
	if (!strcmp(locale, "tr"))
		return (struct locale){LANG_TUR, SCRIPT_LATIN, REG_US};
	for (unsigned i = 0, n = strlen(locale); i < n; i++)
		if ((locale[i] >= 'a' && locale[i] <= 'z')
		    || (locale[i] >= 'A' && locale[i] <= 'Z'))
			return LOCALE_DEFAULT;
	return LOCALE_DEFAULT;
}
