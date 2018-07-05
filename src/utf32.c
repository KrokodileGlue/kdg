#include <string.h>
#include <assert.h>

#include "kdgu.h"
#include "utf32.h"
#include "error.h"
#include "unicode.h"

static struct error
utf32validatechar(const uint8_t *s, uint8_t *r, unsigned *i,
                  unsigned *idx, size_t buflen, int endian)
{
	struct error err = ERR(ERR_NO_ERROR, *i);

	if (buflen - *i < 4) {
		*i = buflen;
		return ERR(ERR_UTF32_EOS, *i);
	}

	uint32_t c = READUTF32(endian, s + *i);
	if (is_noncharacter(c))
		err = ERR(ERR_NONCHARACTER, *i);

	memcpy(r + *idx, s + *i, 4);
	*idx += 4, *i += 4;

	return err;
}

uint8_t *
utf32validate(kdgu *k, const uint8_t *s, size_t *l, int endian)
{
	uint8_t *r = malloc(*l);
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
		struct error err = utf32validatechar(s,
						     r,
						     &i,
						     &idx,
						     buflen,
						     endian);
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
