#include <string.h>

#include "kdgu.h"
#include "utf16.h"
#include "error.h"

static struct error
utf16validatechar(const char *s, char *r, unsigned *i,
                  unsigned *idx, size_t buflen, int endian)
{
	struct error err = ERR(ERR_NO_ERROR, *i);

	if (buflen - *i < 2) {
		if (buflen - *i == 1) (*i)++;
		return ERR(ERR_UTF16_EOS, *i);
	}

	uint16_t c = READUTF16(endian, s + *i);

	/* It's not in the surrogate pair range. */
	if (!UTF16HIGH_SURROGATE(c)) {
		memcpy(r + *idx, &c, sizeof c);
		*idx += 2;
		*i += 2;
		return err;
	}

	if (buflen - *i < 4)
		return (*i) += 2, ERR(ERR_UTF16_EOS, *i);

	/* It's in the surrogate pair range. */
	uint16_t c2 = READUTF16(endian, s + *i + 2);
	*i += 4;

	if (!UTF16LOW_SURROGATE(c2)) {
		r[(*idx)++] = KDGU_REPLACEMENT;
		return ERR(ERR_UTF16_MISSING_SURROGATE, *i);
	}

	memcpy(r + *idx, &c, sizeof c);
	memcpy(r + *idx + 2, &c2, sizeof c2);

	*idx += 4;

	return err;
}

char *
utf16validate(kdgu *k, const char *s, size_t *l, int endian)
{
	char *r = malloc(*l);
	if (!r) return NULL;

	unsigned idx = 0;
	size_t buflen = *l;

	/*
	 * Page 41 table 2-4 indicates that the BOM should not appear
	 * in in the UTF-16BE or UTF-16LE encodings. Simply ignoring
	 * it will cause it (if it is present) to be interpreted as a
	 * zero width space in utf16validatechar.
	 */

	if (!endian && buflen >= 2) {
		/* Check the BOM. */
		uint16_t c = (uint8_t)s[1] << 8
			| (uint8_t)s[0];

		if (c == (uint16_t)0xFFFE) {
			endian = ENDIAN_LITTLE;
			s += 2;
			buflen -= 2;
		} else if (c == (uint16_t)0xFEFF) {
			endian = ENDIAN_BIG;
			s += 2;
			buflen -= 2;
		}
	}

	for (unsigned i = 0; i < buflen;) {
		struct error err =
			utf16validatechar(s, r, &i,
			                  &idx, buflen, endian);

		if (!err.kind) continue;

		if (!pusherror(k, err)) {
			free(r);
			return NULL;
		}
	}

	*l = idx;
	return r;
}
