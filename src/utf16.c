#include <string.h>

#include "kdgu.h"
#include "utf16.h"
#include "error.h"
#include "unicode.h"

struct error
utf16encode(uint32_t c, uint8_t *buf, unsigned *len,
	    int idx, int endian)
{
	struct error err = ERR(ERR_NO_ERROR, idx);
	*len = 2;

	if (endian == KDGU_ENDIAN_LITTLE) {
		buf[0] = c & 0xFF;
		buf[1] = (c >> 8) & 0xFF;
	} else {
		buf[0] = (c >> 8) & 0xFF;
		buf[1] = c & 0xFF;
	}

	if ((c >= 0xE000 && c <= 0xFFFF) || c <= 0xD7FF) return err;
	if (is_noncharacter(c)) return ERR(ERR_NONCHARACTER, idx);

	c -= 0x10000;
	uint16_t high = (c >> 10) + 0xD800;
	uint16_t low = (c & 0x3FF) + 0xDC00;
	if (UTF16HIGH_SURROGATE(high)) {
		*len = 4;
		if (endian == KDGU_ENDIAN_LITTLE) {
			buf[0] = low & 0xFF;
			buf[1] = (low >> 8) & 0xFF;
			buf[2] = high & 0xFF;
			buf[3] = (high >> 8) & 0xFF;
		} else {
			buf[0] = (high >> 8) & 0xFF;
			buf[1] = high & 0xFF;
			buf[2] = (low >> 8) & 0xFF;
			buf[3] = low & 0xFF;
		}
	}

	return err;
}

static struct error
utf16validatechar(const uint8_t *s, uint8_t *r, unsigned *i,
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
		if (endian == KDGU_ENDIAN_LITTLE) {
			r[*idx]     = c & 0xFF;
			r[*idx + 1] = (c & 0xFF00) >> 8;
		} else {
			r[*idx + 1] = c & 0xFF;
			r[*idx]     = (c & 0xFF00) >> 8;
		}

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

	if (endian == KDGU_ENDIAN_LITTLE) {
		r[*idx]     = c & 0xFF;
		r[*idx + 1] = (c & 0xFF00) >> 8;
		r[*idx + 2] = c2 & 0xFF;
		r[*idx + 3] = (c2 & 0xFF00) >> 8;
	} else {
		r[*idx + 3] = c & 0xFF;
		r[*idx + 2] = (c & 0xFF00) >> 8;
		r[*idx + 1] = c2 & 0xFF;
		r[*idx + 0] = (c2 & 0xFF00) >> 8;
	}

	*idx += 4;

	return err;
}

uint8_t *
utf16validate(kdgu *k, const uint8_t *s, size_t *l, int endian)
{
	uint8_t *r = malloc(*l);
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
		uint16_t c = (uint8_t)s[1] << 8 | (uint8_t)s[0];

		if (c == (uint16_t)0xFFFE) {
			endian = KDGU_ENDIAN_LITTLE;
			s += 2;
			buflen -= 2;
		} else if (c == (uint16_t)0xFEFF) {
			endian = KDGU_ENDIAN_BIG;
			s += 2;
			buflen -= 2;
		}
	}

	for (unsigned i = 0; i < buflen;) {
		struct error err = utf16validatechar(s,
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

	*l = idx;
	if (k->fmt == KDGU_FMT_UTF16 && endian == KDGU_ENDIAN_LITTLE)
		k->fmt = KDGU_FMT_UTF16LE;

	return r;
}
