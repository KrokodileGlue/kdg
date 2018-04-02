#include <string.h>

#include "kdgu.h"
#include "utf32.h"
#include "error.h"

static struct kdgu_error
utf32validatechar(const char *s, char *r, unsigned *i,
                  unsigned *idx, size_t buflen, int endian)
{
	struct kdgu_error err = ERR(KDGU_ERR_NO_ERROR, *i);

	if (buflen - *i < 4)
		return ERR(KDGU_ERR_UTF32_EOS, *i);

	uint32_t c = READUTF32(s + *i);
	memcpy(r + *idx, &c, sizeof c);
	*idx += 4, *i += 4;

	return err;
}

char *
utf32validate(kdgu *k, const char *s, size_t *l, int endian)
{
	char *r = malloc(*l);
	if (!r) return NULL;

	unsigned idx = 0;
	size_t buflen = *l;

	if (!endian && buflen >= 2) {
		/* Check the BOM. */
		endian = ENDIAN_BIG;
		uint32_t c = READUTF32(s);

		if (c == (uint32_t)0xFFFE0000) {
			endian = ENDIAN_LITTLE;
			s += 4;
			buflen -= 4;
		} else if (c == (uint32_t)0xFEFF) {
			endian = ENDIAN_BIG;
			s += 4;
			buflen -= 4;
		}
	}

	for (unsigned i = 0; i < buflen;) {
		struct kdgu_error err =
			utf32validatechar(s, r, &i,
			                  &idx, buflen, endian);

		if (!err.kind) continue;

		if (!pusherror(k, err)) {
			free(r), free(r);
			return NULL;
		}
	}

	*l = idx;
	return r;
}
