#include "kdgu.h"
#include "unicode.h"

struct special_case special_case[] = {
	{ 0x00DF, 1, (uint32_t []){ 0x00DF }, 2, (uint32_t []){ 0x0053, 0x0073 }, 2, (uint32_t []){ 0x0053, 0x0053 } }, /* LATIN SMALL LETTER SHARP S */
	{ 0x0130, 2, (uint32_t []){ 0x0069, 0x0307 }, 1, (uint32_t []){ 0x0130 }, 1, (uint32_t []){ 0x0130 } }, /* LATIN CAPITAL LETTER I WITH DOT ABOVE */
	{ 0xFB00, 1, (uint32_t []){ 0xFB00 }, 2, (uint32_t []){ 0x0046, 0x0066 }, 2, (uint32_t []){ 0x0046, 0x0046 } }, /* LATIN SMALL LIGATURE FF */
	{ 0xFB01, 1, (uint32_t []){ 0xFB01 }, 2, (uint32_t []){ 0x0046, 0x0069 }, 2, (uint32_t []){ 0x0046, 0x0049 } }, /* LATIN SMALL LIGATURE FI */
	{ 0xFB02, 1, (uint32_t []){ 0xFB02 }, 2, (uint32_t []){ 0x0046, 0x006C }, 2, (uint32_t []){ 0x0046, 0x004C } }, /* LATIN SMALL LIGATURE FL */
	{ 0xFB03, 1, (uint32_t []){ 0xFB03 }, 3, (uint32_t []){ 0x0046, 0x0066, 0x0069 }, 3, (uint32_t []){ 0x0046, 0x0046, 0x0049 } }, /* LATIN SMALL LIGATURE FFI */
	{ 0xFB04, 1, (uint32_t []){ 0xFB04 }, 3, (uint32_t []){ 0x0046, 0x0066, 0x006C }, 3, (uint32_t []){ 0x0046, 0x0046, 0x004C } }, /* LATIN SMALL LIGATURE FFL */
	{ 0xFB05, 1, (uint32_t []){ 0xFB05 }, 2, (uint32_t []){ 0x0053, 0x0074 }, 2, (uint32_t []){ 0x0053, 0x0054 } }, /* LATIN SMALL LIGATURE LONG S T */
	{ 0xFB06, 1, (uint32_t []){ 0xFB06 }, 2, (uint32_t []){ 0x0053, 0x0074 }, 2, (uint32_t []){ 0x0053, 0x0054 } }, /* LATIN SMALL LIGATURE ST */
	{ 0x0587, 1, (uint32_t []){ 0x0587 }, 2, (uint32_t []){ 0x0535, 0x0582 }, 2, (uint32_t []){ 0x0535, 0x0552 } }, /* ARMENIAN SMALL LIGATURE ECH YIWN */
	{ 0xFB13, 1, (uint32_t []){ 0xFB13 }, 2, (uint32_t []){ 0x0544, 0x0576 }, 2, (uint32_t []){ 0x0544, 0x0546 } }, /* ARMENIAN SMALL LIGATURE MEN NOW */
	{ 0xFB14, 1, (uint32_t []){ 0xFB14 }, 2, (uint32_t []){ 0x0544, 0x0565 }, 2, (uint32_t []){ 0x0544, 0x0535 } }, /* ARMENIAN SMALL LIGATURE MEN ECH */
	{ 0xFB15, 1, (uint32_t []){ 0xFB15 }, 2, (uint32_t []){ 0x0544, 0x056B }, 2, (uint32_t []){ 0x0544, 0x053B } }, /* ARMENIAN SMALL LIGATURE MEN INI */
	{ 0xFB16, 1, (uint32_t []){ 0xFB16 }, 2, (uint32_t []){ 0x054E, 0x0576 }, 2, (uint32_t []){ 0x054E, 0x0546 } }, /* ARMENIAN SMALL LIGATURE VEW NOW */
	{ 0xFB17, 1, (uint32_t []){ 0xFB17 }, 2, (uint32_t []){ 0x0544, 0x056D }, 2, (uint32_t []){ 0x0544, 0x053D } }, /* ARMENIAN SMALL LIGATURE MEN XEH */
	{ 0x0149, 1, (uint32_t []){ 0x0149 }, 2, (uint32_t []){ 0x02BC, 0x004E }, 2, (uint32_t []){ 0x02BC, 0x004E } }, /* LATIN SMALL LETTER N PRECEDED BY APOSTROPHE */
	{ 0x0390, 1, (uint32_t []){ 0x0390 }, 3, (uint32_t []){ 0x0399, 0x0308, 0x0301 }, 3, (uint32_t []){ 0x0399, 0x0308, 0x0301 } }, /* GREEK SMALL LETTER IOTA WITH DIALYTIKA AND TONOS */
	{ 0x03B0, 1, (uint32_t []){ 0x03B0 }, 3, (uint32_t []){ 0x03A5, 0x0308, 0x0301 }, 3, (uint32_t []){ 0x03A5, 0x0308, 0x0301 } }, /* GREEK SMALL LETTER UPSILON WITH DIALYTIKA AND TONOS */
	{ 0x01F0, 1, (uint32_t []){ 0x01F0 }, 2, (uint32_t []){ 0x004A, 0x030C }, 2, (uint32_t []){ 0x004A, 0x030C } }, /* LATIN SMALL LETTER J WITH CARON */
	{ 0x1E96, 1, (uint32_t []){ 0x1E96 }, 2, (uint32_t []){ 0x0048, 0x0331 }, 2, (uint32_t []){ 0x0048, 0x0331 } }, /* LATIN SMALL LETTER H WITH LINE BELOW */
	{ 0x1E97, 1, (uint32_t []){ 0x1E97 }, 2, (uint32_t []){ 0x0054, 0x0308 }, 2, (uint32_t []){ 0x0054, 0x0308 } }, /* LATIN SMALL LETTER T WITH DIAERESIS */
	{ 0x1E98, 1, (uint32_t []){ 0x1E98 }, 2, (uint32_t []){ 0x0057, 0x030A }, 2, (uint32_t []){ 0x0057, 0x030A } }, /* LATIN SMALL LETTER W WITH RING ABOVE */
	{ 0x1E99, 1, (uint32_t []){ 0x1E99 }, 2, (uint32_t []){ 0x0059, 0x030A }, 2, (uint32_t []){ 0x0059, 0x030A } }, /* LATIN SMALL LETTER Y WITH RING ABOVE */
	{ 0x1E9A, 1, (uint32_t []){ 0x1E9A }, 2, (uint32_t []){ 0x0041, 0x02BE }, 2, (uint32_t []){ 0x0041, 0x02BE } }, /* LATIN SMALL LETTER A WITH RIGHT HALF RING */
	{ 0x1F50, 1, (uint32_t []){ 0x1F50 }, 2, (uint32_t []){ 0x03A5, 0x0313 }, 2, (uint32_t []){ 0x03A5, 0x0313 } }, /* GREEK SMALL LETTER UPSILON WITH PSILI */
	{ 0x1F52, 1, (uint32_t []){ 0x1F52 }, 3, (uint32_t []){ 0x03A5, 0x0313, 0x0300 }, 3, (uint32_t []){ 0x03A5, 0x0313, 0x0300 } }, /* GREEK SMALL LETTER UPSILON WITH PSILI AND VARIA */
	{ 0x1F54, 1, (uint32_t []){ 0x1F54 }, 3, (uint32_t []){ 0x03A5, 0x0313, 0x0301 }, 3, (uint32_t []){ 0x03A5, 0x0313, 0x0301 } }, /* GREEK SMALL LETTER UPSILON WITH PSILI AND OXIA */
	{ 0x1F56, 1, (uint32_t []){ 0x1F56 }, 3, (uint32_t []){ 0x03A5, 0x0313, 0x0342 }, 3, (uint32_t []){ 0x03A5, 0x0313, 0x0342 } }, /* GREEK SMALL LETTER UPSILON WITH PSILI AND PERISPOMENI */
	{ 0x1FB6, 1, (uint32_t []){ 0x1FB6 }, 2, (uint32_t []){ 0x0391, 0x0342 }, 2, (uint32_t []){ 0x0391, 0x0342 } }, /* GREEK SMALL LETTER ALPHA WITH PERISPOMENI */
	{ 0x1FC6, 1, (uint32_t []){ 0x1FC6 }, 2, (uint32_t []){ 0x0397, 0x0342 }, 2, (uint32_t []){ 0x0397, 0x0342 } }, /* GREEK SMALL LETTER ETA WITH PERISPOMENI */
	{ 0x1FD2, 1, (uint32_t []){ 0x1FD2 }, 3, (uint32_t []){ 0x0399, 0x0308, 0x0300 }, 3, (uint32_t []){ 0x0399, 0x0308, 0x0300 } }, /* GREEK SMALL LETTER IOTA WITH DIALYTIKA AND VARIA */
	{ 0x1FD3, 1, (uint32_t []){ 0x1FD3 }, 3, (uint32_t []){ 0x0399, 0x0308, 0x0301 }, 3, (uint32_t []){ 0x0399, 0x0308, 0x0301 } }, /* GREEK SMALL LETTER IOTA WITH DIALYTIKA AND OXIA */
	{ 0x1FD6, 1, (uint32_t []){ 0x1FD6 }, 2, (uint32_t []){ 0x0399, 0x0342 }, 2, (uint32_t []){ 0x0399, 0x0342 } }, /* GREEK SMALL LETTER IOTA WITH PERISPOMENI */
	{ 0x1FD7, 1, (uint32_t []){ 0x1FD7 }, 3, (uint32_t []){ 0x0399, 0x0308, 0x0342 }, 3, (uint32_t []){ 0x0399, 0x0308, 0x0342 } }, /* GREEK SMALL LETTER IOTA WITH DIALYTIKA AND PERISPOMENI */
	{ 0x1FE2, 1, (uint32_t []){ 0x1FE2 }, 3, (uint32_t []){ 0x03A5, 0x0308, 0x0300 }, 3, (uint32_t []){ 0x03A5, 0x0308, 0x0300 } }, /* GREEK SMALL LETTER UPSILON WITH DIALYTIKA AND VARIA */
	{ 0x1FE3, 1, (uint32_t []){ 0x1FE3 }, 3, (uint32_t []){ 0x03A5, 0x0308, 0x0301 }, 3, (uint32_t []){ 0x03A5, 0x0308, 0x0301 } }, /* GREEK SMALL LETTER UPSILON WITH DIALYTIKA AND OXIA */
	{ 0x1FE4, 1, (uint32_t []){ 0x1FE4 }, 2, (uint32_t []){ 0x03A1, 0x0313 }, 2, (uint32_t []){ 0x03A1, 0x0313 } }, /* GREEK SMALL LETTER RHO WITH PSILI */
	{ 0x1FE6, 1, (uint32_t []){ 0x1FE6 }, 2, (uint32_t []){ 0x03A5, 0x0342 }, 2, (uint32_t []){ 0x03A5, 0x0342 } }, /* GREEK SMALL LETTER UPSILON WITH PERISPOMENI */
	{ 0x1FE7, 1, (uint32_t []){ 0x1FE7 }, 3, (uint32_t []){ 0x03A5, 0x0308, 0x0342 }, 3, (uint32_t []){ 0x03A5, 0x0308, 0x0342 } }, /* GREEK SMALL LETTER UPSILON WITH DIALYTIKA AND PERISPOMENI */
	{ 0x1FF6, 1, (uint32_t []){ 0x1FF6 }, 2, (uint32_t []){ 0x03A9, 0x0342 }, 2, (uint32_t []){ 0x03A9, 0x0342 } }, /* GREEK SMALL LETTER OMEGA WITH PERISPOMENI */
	{ 0x1F80, 1, (uint32_t []){ 0x1F80 }, 1, (uint32_t []){ 0x1F88 }, 2, (uint32_t []){ 0x1F08, 0x0399 } }, /* GREEK SMALL LETTER ALPHA WITH PSILI AND YPOGEGRAMMENI */
	{ 0x1F81, 1, (uint32_t []){ 0x1F81 }, 1, (uint32_t []){ 0x1F89 }, 2, (uint32_t []){ 0x1F09, 0x0399 } }, /* GREEK SMALL LETTER ALPHA WITH DASIA AND YPOGEGRAMMENI */
	{ 0x1F82, 1, (uint32_t []){ 0x1F82 }, 1, (uint32_t []){ 0x1F8A }, 2, (uint32_t []){ 0x1F0A, 0x0399 } }, /* GREEK SMALL LETTER ALPHA WITH PSILI AND VARIA AND YPOGEGRAMMENI */
	{ 0x1F83, 1, (uint32_t []){ 0x1F83 }, 1, (uint32_t []){ 0x1F8B }, 2, (uint32_t []){ 0x1F0B, 0x0399 } }, /* GREEK SMALL LETTER ALPHA WITH DASIA AND VARIA AND YPOGEGRAMMENI */
	{ 0x1F84, 1, (uint32_t []){ 0x1F84 }, 1, (uint32_t []){ 0x1F8C }, 2, (uint32_t []){ 0x1F0C, 0x0399 } }, /* GREEK SMALL LETTER ALPHA WITH PSILI AND OXIA AND YPOGEGRAMMENI */
	{ 0x1F85, 1, (uint32_t []){ 0x1F85 }, 1, (uint32_t []){ 0x1F8D }, 2, (uint32_t []){ 0x1F0D, 0x0399 } }, /* GREEK SMALL LETTER ALPHA WITH DASIA AND OXIA AND YPOGEGRAMMENI */
	{ 0x1F86, 1, (uint32_t []){ 0x1F86 }, 1, (uint32_t []){ 0x1F8E }, 2, (uint32_t []){ 0x1F0E, 0x0399 } }, /* GREEK SMALL LETTER ALPHA WITH PSILI AND PERISPOMENI AND YPOGEGRAMMENI */
	{ 0x1F87, 1, (uint32_t []){ 0x1F87 }, 1, (uint32_t []){ 0x1F8F }, 2, (uint32_t []){ 0x1F0F, 0x0399 } }, /* GREEK SMALL LETTER ALPHA WITH DASIA AND PERISPOMENI AND YPOGEGRAMMENI */
	{ 0x1F88, 1, (uint32_t []){ 0x1F80 }, 1, (uint32_t []){ 0x1F88 }, 2, (uint32_t []){ 0x1F08, 0x0399 } }, /* GREEK CAPITAL LETTER ALPHA WITH PSILI AND PROSGEGRAMMENI */
	{ 0x1F89, 1, (uint32_t []){ 0x1F81 }, 1, (uint32_t []){ 0x1F89 }, 2, (uint32_t []){ 0x1F09, 0x0399 } }, /* GREEK CAPITAL LETTER ALPHA WITH DASIA AND PROSGEGRAMMENI */
	{ 0x1F8A, 1, (uint32_t []){ 0x1F82 }, 1, (uint32_t []){ 0x1F8A }, 2, (uint32_t []){ 0x1F0A, 0x0399 } }, /* GREEK CAPITAL LETTER ALPHA WITH PSILI AND VARIA AND PROSGEGRAMMENI */
	{ 0x1F8B, 1, (uint32_t []){ 0x1F83 }, 1, (uint32_t []){ 0x1F8B }, 2, (uint32_t []){ 0x1F0B, 0x0399 } }, /* GREEK CAPITAL LETTER ALPHA WITH DASIA AND VARIA AND PROSGEGRAMMENI */
	{ 0x1F8C, 1, (uint32_t []){ 0x1F84 }, 1, (uint32_t []){ 0x1F8C }, 2, (uint32_t []){ 0x1F0C, 0x0399 } }, /* GREEK CAPITAL LETTER ALPHA WITH PSILI AND OXIA AND PROSGEGRAMMENI */
	{ 0x1F8D, 1, (uint32_t []){ 0x1F85 }, 1, (uint32_t []){ 0x1F8D }, 2, (uint32_t []){ 0x1F0D, 0x0399 } }, /* GREEK CAPITAL LETTER ALPHA WITH DASIA AND OXIA AND PROSGEGRAMMENI */
	{ 0x1F8E, 1, (uint32_t []){ 0x1F86 }, 1, (uint32_t []){ 0x1F8E }, 2, (uint32_t []){ 0x1F0E, 0x0399 } }, /* GREEK CAPITAL LETTER ALPHA WITH PSILI AND PERISPOMENI AND PROSGEGRAMMENI */
	{ 0x1F8F, 1, (uint32_t []){ 0x1F87 }, 1, (uint32_t []){ 0x1F8F }, 2, (uint32_t []){ 0x1F0F, 0x0399 } }, /* GREEK CAPITAL LETTER ALPHA WITH DASIA AND PERISPOMENI AND PROSGEGRAMMENI */
	{ 0x1F90, 1, (uint32_t []){ 0x1F90 }, 1, (uint32_t []){ 0x1F98 }, 2, (uint32_t []){ 0x1F28, 0x0399 } }, /* GREEK SMALL LETTER ETA WITH PSILI AND YPOGEGRAMMENI */
	{ 0x1F91, 1, (uint32_t []){ 0x1F91 }, 1, (uint32_t []){ 0x1F99 }, 2, (uint32_t []){ 0x1F29, 0x0399 } }, /* GREEK SMALL LETTER ETA WITH DASIA AND YPOGEGRAMMENI */
	{ 0x1F92, 1, (uint32_t []){ 0x1F92 }, 1, (uint32_t []){ 0x1F9A }, 2, (uint32_t []){ 0x1F2A, 0x0399 } }, /* GREEK SMALL LETTER ETA WITH PSILI AND VARIA AND YPOGEGRAMMENI */
	{ 0x1F93, 1, (uint32_t []){ 0x1F93 }, 1, (uint32_t []){ 0x1F9B }, 2, (uint32_t []){ 0x1F2B, 0x0399 } }, /* GREEK SMALL LETTER ETA WITH DASIA AND VARIA AND YPOGEGRAMMENI */
	{ 0x1F94, 1, (uint32_t []){ 0x1F94 }, 1, (uint32_t []){ 0x1F9C }, 2, (uint32_t []){ 0x1F2C, 0x0399 } }, /* GREEK SMALL LETTER ETA WITH PSILI AND OXIA AND YPOGEGRAMMENI */
	{ 0x1F95, 1, (uint32_t []){ 0x1F95 }, 1, (uint32_t []){ 0x1F9D }, 2, (uint32_t []){ 0x1F2D, 0x0399 } }, /* GREEK SMALL LETTER ETA WITH DASIA AND OXIA AND YPOGEGRAMMENI */
	{ 0x1F96, 1, (uint32_t []){ 0x1F96 }, 1, (uint32_t []){ 0x1F9E }, 2, (uint32_t []){ 0x1F2E, 0x0399 } }, /* GREEK SMALL LETTER ETA WITH PSILI AND PERISPOMENI AND YPOGEGRAMMENI */
	{ 0x1F97, 1, (uint32_t []){ 0x1F97 }, 1, (uint32_t []){ 0x1F9F }, 2, (uint32_t []){ 0x1F2F, 0x0399 } }, /* GREEK SMALL LETTER ETA WITH DASIA AND PERISPOMENI AND YPOGEGRAMMENI */
	{ 0x1F98, 1, (uint32_t []){ 0x1F90 }, 1, (uint32_t []){ 0x1F98 }, 2, (uint32_t []){ 0x1F28, 0x0399 } }, /* GREEK CAPITAL LETTER ETA WITH PSILI AND PROSGEGRAMMENI */
	{ 0x1F99, 1, (uint32_t []){ 0x1F91 }, 1, (uint32_t []){ 0x1F99 }, 2, (uint32_t []){ 0x1F29, 0x0399 } }, /* GREEK CAPITAL LETTER ETA WITH DASIA AND PROSGEGRAMMENI */
	{ 0x1F9A, 1, (uint32_t []){ 0x1F92 }, 1, (uint32_t []){ 0x1F9A }, 2, (uint32_t []){ 0x1F2A, 0x0399 } }, /* GREEK CAPITAL LETTER ETA WITH PSILI AND VARIA AND PROSGEGRAMMENI */
	{ 0x1F9B, 1, (uint32_t []){ 0x1F93 }, 1, (uint32_t []){ 0x1F9B }, 2, (uint32_t []){ 0x1F2B, 0x0399 } }, /* GREEK CAPITAL LETTER ETA WITH DASIA AND VARIA AND PROSGEGRAMMENI */
	{ 0x1F9C, 1, (uint32_t []){ 0x1F94 }, 1, (uint32_t []){ 0x1F9C }, 2, (uint32_t []){ 0x1F2C, 0x0399 } }, /* GREEK CAPITAL LETTER ETA WITH PSILI AND OXIA AND PROSGEGRAMMENI */
	{ 0x1F9D, 1, (uint32_t []){ 0x1F95 }, 1, (uint32_t []){ 0x1F9D }, 2, (uint32_t []){ 0x1F2D, 0x0399 } }, /* GREEK CAPITAL LETTER ETA WITH DASIA AND OXIA AND PROSGEGRAMMENI */
	{ 0x1F9E, 1, (uint32_t []){ 0x1F96 }, 1, (uint32_t []){ 0x1F9E }, 2, (uint32_t []){ 0x1F2E, 0x0399 } }, /* GREEK CAPITAL LETTER ETA WITH PSILI AND PERISPOMENI AND PROSGEGRAMMENI */
	{ 0x1F9F, 1, (uint32_t []){ 0x1F97 }, 1, (uint32_t []){ 0x1F9F }, 2, (uint32_t []){ 0x1F2F, 0x0399 } }, /* GREEK CAPITAL LETTER ETA WITH DASIA AND PERISPOMENI AND PROSGEGRAMMENI */
	{ 0x1FA0, 1, (uint32_t []){ 0x1FA0 }, 1, (uint32_t []){ 0x1FA8 }, 2, (uint32_t []){ 0x1F68, 0x0399 } }, /* GREEK SMALL LETTER OMEGA WITH PSILI AND YPOGEGRAMMENI */
	{ 0x1FA1, 1, (uint32_t []){ 0x1FA1 }, 1, (uint32_t []){ 0x1FA9 }, 2, (uint32_t []){ 0x1F69, 0x0399 } }, /* GREEK SMALL LETTER OMEGA WITH DASIA AND YPOGEGRAMMENI */
	{ 0x1FA2, 1, (uint32_t []){ 0x1FA2 }, 1, (uint32_t []){ 0x1FAA }, 2, (uint32_t []){ 0x1F6A, 0x0399 } }, /* GREEK SMALL LETTER OMEGA WITH PSILI AND VARIA AND YPOGEGRAMMENI */
	{ 0x1FA3, 1, (uint32_t []){ 0x1FA3 }, 1, (uint32_t []){ 0x1FAB }, 2, (uint32_t []){ 0x1F6B, 0x0399 } }, /* GREEK SMALL LETTER OMEGA WITH DASIA AND VARIA AND YPOGEGRAMMENI */
	{ 0x1FA4, 1, (uint32_t []){ 0x1FA4 }, 1, (uint32_t []){ 0x1FAC }, 2, (uint32_t []){ 0x1F6C, 0x0399 } }, /* GREEK SMALL LETTER OMEGA WITH PSILI AND OXIA AND YPOGEGRAMMENI */
	{ 0x1FA5, 1, (uint32_t []){ 0x1FA5 }, 1, (uint32_t []){ 0x1FAD }, 2, (uint32_t []){ 0x1F6D, 0x0399 } }, /* GREEK SMALL LETTER OMEGA WITH DASIA AND OXIA AND YPOGEGRAMMENI */
	{ 0x1FA6, 1, (uint32_t []){ 0x1FA6 }, 1, (uint32_t []){ 0x1FAE }, 2, (uint32_t []){ 0x1F6E, 0x0399 } }, /* GREEK SMALL LETTER OMEGA WITH PSILI AND PERISPOMENI AND YPOGEGRAMMENI */
	{ 0x1FA7, 1, (uint32_t []){ 0x1FA7 }, 1, (uint32_t []){ 0x1FAF }, 2, (uint32_t []){ 0x1F6F, 0x0399 } }, /* GREEK SMALL LETTER OMEGA WITH DASIA AND PERISPOMENI AND YPOGEGRAMMENI */
	{ 0x1FA8, 1, (uint32_t []){ 0x1FA0 }, 1, (uint32_t []){ 0x1FA8 }, 2, (uint32_t []){ 0x1F68, 0x0399 } }, /* GREEK CAPITAL LETTER OMEGA WITH PSILI AND PROSGEGRAMMENI */
	{ 0x1FA9, 1, (uint32_t []){ 0x1FA1 }, 1, (uint32_t []){ 0x1FA9 }, 2, (uint32_t []){ 0x1F69, 0x0399 } }, /* GREEK CAPITAL LETTER OMEGA WITH DASIA AND PROSGEGRAMMENI */
	{ 0x1FAA, 1, (uint32_t []){ 0x1FA2 }, 1, (uint32_t []){ 0x1FAA }, 2, (uint32_t []){ 0x1F6A, 0x0399 } }, /* GREEK CAPITAL LETTER OMEGA WITH PSILI AND VARIA AND PROSGEGRAMMENI */
	{ 0x1FAB, 1, (uint32_t []){ 0x1FA3 }, 1, (uint32_t []){ 0x1FAB }, 2, (uint32_t []){ 0x1F6B, 0x0399 } }, /* GREEK CAPITAL LETTER OMEGA WITH DASIA AND VARIA AND PROSGEGRAMMENI */
	{ 0x1FAC, 1, (uint32_t []){ 0x1FA4 }, 1, (uint32_t []){ 0x1FAC }, 2, (uint32_t []){ 0x1F6C, 0x0399 } }, /* GREEK CAPITAL LETTER OMEGA WITH PSILI AND OXIA AND PROSGEGRAMMENI */
	{ 0x1FAD, 1, (uint32_t []){ 0x1FA5 }, 1, (uint32_t []){ 0x1FAD }, 2, (uint32_t []){ 0x1F6D, 0x0399 } }, /* GREEK CAPITAL LETTER OMEGA WITH DASIA AND OXIA AND PROSGEGRAMMENI */
	{ 0x1FAE, 1, (uint32_t []){ 0x1FA6 }, 1, (uint32_t []){ 0x1FAE }, 2, (uint32_t []){ 0x1F6E, 0x0399 } }, /* GREEK CAPITAL LETTER OMEGA WITH PSILI AND PERISPOMENI AND PROSGEGRAMMENI */
	{ 0x1FAF, 1, (uint32_t []){ 0x1FA7 }, 1, (uint32_t []){ 0x1FAF }, 2, (uint32_t []){ 0x1F6F, 0x0399 } }, /* GREEK CAPITAL LETTER OMEGA WITH DASIA AND PERISPOMENI AND PROSGEGRAMMENI */
	{ 0x1FB3, 1, (uint32_t []){ 0x1FB3 }, 1, (uint32_t []){ 0x1FBC }, 2, (uint32_t []){ 0x0391, 0x0399 } }, /* GREEK SMALL LETTER ALPHA WITH YPOGEGRAMMENI */
	{ 0x1FBC, 1, (uint32_t []){ 0x1FB3 }, 1, (uint32_t []){ 0x1FBC }, 2, (uint32_t []){ 0x0391, 0x0399 } }, /* GREEK CAPITAL LETTER ALPHA WITH PROSGEGRAMMENI */
	{ 0x1FC3, 1, (uint32_t []){ 0x1FC3 }, 1, (uint32_t []){ 0x1FCC }, 2, (uint32_t []){ 0x0397, 0x0399 } }, /* GREEK SMALL LETTER ETA WITH YPOGEGRAMMENI */
	{ 0x1FCC, 1, (uint32_t []){ 0x1FC3 }, 1, (uint32_t []){ 0x1FCC }, 2, (uint32_t []){ 0x0397, 0x0399 } }, /* GREEK CAPITAL LETTER ETA WITH PROSGEGRAMMENI */
	{ 0x1FF3, 1, (uint32_t []){ 0x1FF3 }, 1, (uint32_t []){ 0x1FFC }, 2, (uint32_t []){ 0x03A9, 0x0399 } }, /* GREEK SMALL LETTER OMEGA WITH YPOGEGRAMMENI */
	{ 0x1FFC, 1, (uint32_t []){ 0x1FF3 }, 1, (uint32_t []){ 0x1FFC }, 2, (uint32_t []){ 0x03A9, 0x0399 } }, /* GREEK CAPITAL LETTER OMEGA WITH PROSGEGRAMMENI */
	{ 0x1FB2, 1, (uint32_t []){ 0x1FB2 }, 2, (uint32_t []){ 0x1FBA, 0x0345 }, 2, (uint32_t []){ 0x1FBA, 0x0399 } }, /* GREEK SMALL LETTER ALPHA WITH VARIA AND YPOGEGRAMMENI */
	{ 0x1FB4, 1, (uint32_t []){ 0x1FB4 }, 2, (uint32_t []){ 0x0386, 0x0345 }, 2, (uint32_t []){ 0x0386, 0x0399 } }, /* GREEK SMALL LETTER ALPHA WITH OXIA AND YPOGEGRAMMENI */
	{ 0x1FC2, 1, (uint32_t []){ 0x1FC2 }, 2, (uint32_t []){ 0x1FCA, 0x0345 }, 2, (uint32_t []){ 0x1FCA, 0x0399 } }, /* GREEK SMALL LETTER ETA WITH VARIA AND YPOGEGRAMMENI */
	{ 0x1FC4, 1, (uint32_t []){ 0x1FC4 }, 2, (uint32_t []){ 0x0389, 0x0345 }, 2, (uint32_t []){ 0x0389, 0x0399 } }, /* GREEK SMALL LETTER ETA WITH OXIA AND YPOGEGRAMMENI */
	{ 0x1FF2, 1, (uint32_t []){ 0x1FF2 }, 2, (uint32_t []){ 0x1FFA, 0x0345 }, 2, (uint32_t []){ 0x1FFA, 0x0399 } }, /* GREEK SMALL LETTER OMEGA WITH VARIA AND YPOGEGRAMMENI */
	{ 0x1FF4, 1, (uint32_t []){ 0x1FF4 }, 2, (uint32_t []){ 0x038F, 0x0345 }, 2, (uint32_t []){ 0x038F, 0x0399 } }, /* GREEK SMALL LETTER OMEGA WITH OXIA AND YPOGEGRAMMENI */
	{ 0x1FB7, 1, (uint32_t []){ 0x1FB7 }, 3, (uint32_t []){ 0x0391, 0x0342, 0x0345 }, 3, (uint32_t []){ 0x0391, 0x0342, 0x0399 } }, /* GREEK SMALL LETTER ALPHA WITH PERISPOMENI AND YPOGEGRAMMENI */
	{ 0x1FC7, 1, (uint32_t []){ 0x1FC7 }, 3, (uint32_t []){ 0x0397, 0x0342, 0x0345 }, 3, (uint32_t []){ 0x0397, 0x0342, 0x0399 } }, /* GREEK SMALL LETTER ETA WITH PERISPOMENI AND YPOGEGRAMMENI */
	{ 0x1FF7, 1, (uint32_t []){ 0x1FF7 }, 3, (uint32_t []){ 0x03A9, 0x0342, 0x0345 }, 3, (uint32_t []){ 0x03A9, 0x0342, 0x0399 } }, /* GREEK SMALL LETTER OMEGA WITH PERISPOMENI AND YPOGEGRAMMENI */
	{ 0x03A3, 1, (uint32_t []){ 0x03C2 }, 1, (uint32_t []){ 0x03A3 }, 1, (uint32_t []){ 0x03A3 } }, /* GREEK CAPITAL LETTER SIGMA */
	{ 0x0307, 1, (uint32_t []){ 0x0307 }, 0, (uint32_t []){ 0 }, 0, (uint32_t []){ 0 } }, /* COMBINING DOT ABOVE */
	{ 0x0049, 2, (uint32_t []){ 0x0069, 0x0307 }, 1, (uint32_t []){ 0x0049 }, 1, (uint32_t []){ 0x0049 } }, /* LATIN CAPITAL LETTER I */
	{ 0x004A, 2, (uint32_t []){ 0x006A, 0x0307 }, 1, (uint32_t []){ 0x004A }, 1, (uint32_t []){ 0x004A } }, /* LATIN CAPITAL LETTER J */
	{ 0x012E, 2, (uint32_t []){ 0x012F, 0x0307 }, 1, (uint32_t []){ 0x012E }, 1, (uint32_t []){ 0x012E } }, /* LATIN CAPITAL LETTER I WITH OGONEK */
	{ 0x00CC, 3, (uint32_t []){ 0x0069, 0x0307, 0x0300 }, 1, (uint32_t []){ 0x00CC }, 1, (uint32_t []){ 0x00CC } }, /* LATIN CAPITAL LETTER I WITH GRAVE */
	{ 0x00CD, 3, (uint32_t []){ 0x0069, 0x0307, 0x0301 }, 1, (uint32_t []){ 0x00CD }, 1, (uint32_t []){ 0x00CD } }, /* LATIN CAPITAL LETTER I WITH ACUTE */
	{ 0x0128, 3, (uint32_t []){ 0x0069, 0x0307, 0x0303 }, 1, (uint32_t []){ 0x0128 }, 1, (uint32_t []){ 0x0128 } }, /* LATIN CAPITAL LETTER I WITH TILDE */
	{ 0x0130, 1, (uint32_t []){ 0x0069 }, 1, (uint32_t []){ 0x0130 }, 1, (uint32_t []){ 0x0130 } }, /* LATIN CAPITAL LETTER I WITH DOT ABOVE */
	{ 0x0130, 1, (uint32_t []){ 0x0069 }, 1, (uint32_t []){ 0x0130 }, 1, (uint32_t []){ 0x0130 } }, /* LATIN CAPITAL LETTER I WITH DOT ABOVE */
	{ 0x0307, 0, (uint32_t []){ 0 }, 1, (uint32_t []){ 0x0307 }, 1, (uint32_t []){ 0x0307 } }, /* COMBINING DOT ABOVE */
	{ 0x0307, 0, (uint32_t []){ 0 }, 1, (uint32_t []){ 0x0307 }, 1, (uint32_t []){ 0x0307 } }, /* COMBINING DOT ABOVE */
	{ 0x0049, 1, (uint32_t []){ 0x0131 }, 1, (uint32_t []){ 0x0049 }, 1, (uint32_t []){ 0x0049 } }, /* LATIN CAPITAL LETTER I */
	{ 0x0049, 1, (uint32_t []){ 0x0131 }, 1, (uint32_t []){ 0x0049 }, 1, (uint32_t []){ 0x0049 } }, /* LATIN CAPITAL LETTER I */
	{ 0x0069, 1, (uint32_t []){ 0x0069 }, 1, (uint32_t []){ 0x0130 }, 1, (uint32_t []){ 0x0130 } }, /* LATIN SMALL LETTER I */
	{ 0x0069, 1, (uint32_t []){ 0x0069 }, 1, (uint32_t []){ 0x0130 }, 1, (uint32_t []){ 0x0130 } }, /* LATIN SMALL LETTER I */
};
size_t num_special_case = 119;
