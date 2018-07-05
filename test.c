#include <stdio.h>
#include <assert.h>

#include "kdgu.h"
#include "unicode_data.h"

char *decomp_str[] = {
	"Font",
	"Nobreak",
	"Initial",
	"Medial",
	"Final",
	"Isolated",
	"Circle",
	"Super",
	"Sub",
	"Vertical",
	"Wide",
	"Narrow",
	"Small",
	"Square",
	"Fraction",
	"Compat"
};

char *directionality_str[] = {
	"Left-to-Right",
	"Left-to-Right Embedding",
	"Left-to-Right Override",
	"Right-to-Left",
	"Right-to-Left Arabic",
	"Right-to-Left Embedding",
	"Right-to-Left Override",
	"Pop Directional Format",
	"European Number",
	"European Separator",
	"European Number Terminator",
	"Arabic Number",
	"Common Number Separator",
	"Nonspacing Mark",
	"Boundary Neutral",
	"Paragraph Separator",
	"Segment Separator",
	"Whitespace",
	"Other Neutrals",
	"Left-to-Right Isolate",
	"Right-to-Left Isolate",
	"First Strong Isolate",
	"Pop Directional Isolate"
};

static void
uputchar(uint32_t c)
{
	char buf[10];
	unsigned len;
	fflush(stdout);

	if (c <= 0x7F) {
		len = 1;

		buf[0] = c & 0xFF;
	} else if (c >= 0x80 && c <= 0x7FF) {
		len = 2;

		buf[0] = 0xC0 | ((c >> 6) & 0x1F);
		buf[1] = 0x80 | (c & 0x3F);
	} else if (c >= 0x800 && c <= 0xFFFF) {
		len = 3;

		buf[0] = 0xE0 | ((c >> 12) & 0xF);
		buf[1] = 0x80 | ((c >> 6) & 0x3F);
		buf[2] = 0x80 | (c & 0x3F);
	} else if (c >= 0x10000 && c <= 0x10FFFF) {
		len = 4;

		buf[0] = 0xF0 | ((c >> 18) & 0x7);
		buf[1] = 0x80 | ((c >> 12) & 0x3F);
		buf[2] = 0x80 | ((c >> 6) & 0x3F);
		buf[3] = 0x80 | (c & 0x3F);
	} else assert(0);

	for (unsigned i = 0; i < len; i++)
		putchar(buf[i]);
}

static void
printbuf(uint32_t *buf, unsigned len)
{
	printf("[");
	for (unsigned i = 0; i < len; i++) {
		printf("U+%04"PRIX32, buf[i]);
		if (i != len - 1) putchar(' ');
	}
	printf("] === [");
	for (unsigned i = 0; i < len; i++)
		uputchar(buf[i]);
	printf("]\n");
}

static void
test_decomp_type(uint32_t c, unsigned expect)
{
	struct codepoint *cp = codepoint(c);
	printf("U+%04"PRIX32" (", c);
	uputchar(0x34F), uputchar(c);
	printf(") decomposition: %8s - ", decomp_str[cp->decomp_type]);
	assert(cp->decomp_type == expect);

	uint32_t buf[100];
	unsigned len = write_sequence(buf, codepoint(c)->decomp);
	printbuf(buf, len);
}

static void
test_directionality(uint32_t c, unsigned expect)
{
	struct codepoint *cp = codepoint(c);
	printf("U+%05"PRIX32" (", c);
	uputchar(0x34F), uputchar(c);
	printf(")    direction: %s\n",
	       directionality_str[cp->bidi]);
	assert(cp->bidi == expect);
}

static void
test_composition(uint32_t a, uint32_t b, uint32_t expect)
{
	uint32_t c = lookup_comp(a, b);
	printf("U+%05"PRIX32" (", c);
	uputchar(0x34F), uputchar(c);
	printf(")  composition: ");
	uputchar(a),
		putchar('+'),
		uputchar(0x34F),
		uputchar(b),
		putchar('='),
		uputchar(c),
		putchar('\n');
	fflush(stdout);
	assert(c == expect);
}

static void
print_upper(uint32_t c)
{
	printf("U+%04"PRIX32" (", c);
	uputchar(0x34F), uputchar(c);
	printf(")   uppercasing:          - ");
	uint32_t buf[100];
	unsigned len = write_sequence(buf, codepoint(c)->upper);
	printbuf(buf, len);
}

static void
test_comparison(uint32_t *s1, unsigned l1, uint32_t *s2, unsigned l2)
{
	kdgu *k1 = kdgu_new(FMT_UTF32, (uint8_t *)s1, l1 * sizeof *s1);
	kdgu *k2 = kdgu_new(FMT_UTF32, (uint8_t *)s2, l2 * sizeof *s2);
	kdgu_convert(k1, FMT_UTF8);
	kdgu_convert(k2, FMT_UTF8);
	kdgu_print(k1), putchar('\n');
	kdgu_print(k2), putchar('\n');
	assert(kdgu_cmp(k1, k2));
}

int main(void)
{
	test_decomp_type(0x2460, DECOMP_TYPE_CIRCLE);
	test_decomp_type(0x207F, DECOMP_TYPE_SUPER);
	test_decomp_type(0xBC, DECOMP_TYPE_FRACTION);
	test_decomp_type(0x2155, DECOMP_TYPE_FRACTION);
	test_decomp_type(0xAF, DECOMP_TYPE_COMPAT);

	print_upper('a');
	print_upper('z');
	print_upper('A');
	print_upper('Z');
	print_upper(0x24D0);
	print_upper(0x24E9);

	test_directionality(0x640, BIDI_AL);
	test_directionality(0x905, BIDI_L);
	test_directionality('a', BIDI_L);
	test_directionality(0x1D1AB, BIDI_NSM);

	test_composition('e', 0x301, 0xE9);
	test_composition('a', 0x301, 0xE1);
	test_composition('A', 0x308, 0xC4);

	test_comparison((uint32_t []){0xFEFF,'e',0x301}, 3,
			(uint32_t []){0xFEFF,0xE9}, 2);

	return 0;
}
