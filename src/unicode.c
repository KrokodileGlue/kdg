#include "unicode.h"

/*
 * Refer to Section 2.4 (Code Points and Characters) and
 * Section 23.7 (Noncharacters) for this stuff.
 */

bool
is_noncharacter(uint32_t c)
{
	if (c >= 0xFDD0 && c <= 0xFDEF) return true;
	for (uint32_t i = 0x0FFFE; i <= 0x10FFFE; i += 0x10000)
		if (c == i || c == i + 1) return true;
	return false;
}
