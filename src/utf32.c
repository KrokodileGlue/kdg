#include <string.h>
#include <assert.h>

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

	uint32_t c = READUTF32(endian, s + *i);
	if (c == (uint32_t)-1) {
		/* TODO: Check for non-characters n stuff. */
		assert(false);
	}

	memcpy(r + *idx, s + *i, 4);
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

	if (!endian && buflen >= 4) {
		/* Check the BOM. */
		uint32_t c = READUTF32(ENDIAN_BIG, s);

		if (c == (uint32_t)0xFEFF) {
			endian = ENDIAN_BIG;
			s += 4;
			buflen -= 4;
		} else if (c == (uint32_t)0xFFFE0000) {
			endian = ENDIAN_LITTLE;
			s += 4;
			buflen -= 4;
		} else {
			endian = ENDIAN_BIG;
		}
	}

	for (unsigned i = 0; i < buflen;) {
		struct kdgu_error err =
			utf32validatechar(s, r, &i,
			                  &idx, buflen, endian);

		if (!err.kind) continue;

		if (!pusherror(k, err)) {
			free(r);
			return NULL;
		}
	}

	k->endian = endian;
	*l = idx;

	return r;
}
