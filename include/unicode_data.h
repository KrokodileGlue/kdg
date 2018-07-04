#ifndef UNICODE_DATA_H
#define UNICODE_DATA_H

#include <inttypes.h>

struct codepoint {
	enum category {
		CATEGORY_CN, /* Other - not assigned        */
		CATEGORY_LU, /* Letter - uppercase          */
		CATEGORY_LL, /* Letter - lowercase          */
		CATEGORY_LT, /* Letter - titlecase          */
		CATEGORY_LM, /* Letter - modifier           */
		CATEGORY_LO, /* Letter - other              */
		CATEGORY_MN, /* Mark - nonspacing           */
		CATEGORY_MC, /* Mark - spacing combining    */
		CATEGORY_ME, /* Mark - enclosing            */
		CATEGORY_ND, /* Number - decimal digit      */
		CATEGORY_NL, /* Number - letter             */
		CATEGORY_NO, /* Number - other              */
		CATEGORY_PC, /* Punctuation - connector     */
		CATEGORY_PD, /* Punctuation - dash          */
		CATEGORY_PS, /* Punctuation - open          */
		CATEGORY_PE, /* Punctuation - close         */
		CATEGORY_PI, /* Punctuation - initial quote */
		CATEGORY_PF, /* Punctuation - final quote   */
		CATEGORY_PO, /* Punctuation - other         */
		CATEGORY_SM, /* Symbol - math               */
		CATEGORY_SC, /* Symbol - currency           */
		CATEGORY_SK, /* Symbol - modifier           */
		CATEGORY_SO, /* Symbol - other              */
		CATEGORY_ZS, /* Separator - space           */
		CATEGORY_ZL, /* Separator - line            */
		CATEGORY_ZP, /* Separator - paragraph       */
		CATEGORY_CC, /* Other - control             */
		CATEGORY_CF, /* Other - format              */
		CATEGORY_CS, /* Other - surrogate           */
		CATEGORY_CO  /* Other - private use         */
	} category;

	enum {
		BIDI_L,   /* Left-to-Right              */
		BIDI_LRE, /* Left-to-Right Embedding    */
		BIDI_LRO, /* Left-to-Right Override     */
		BIDI_R,   /* Right-to-Left              */
		BIDI_AL,  /* Right-to-Left Arabic       */
		BIDI_RLE, /* Right-to-Left Embedding    */
		BIDI_RLO, /* Right-to-Left Override     */
		BIDI_PDF, /* Pop Directional Format     */
		BIDI_EN,  /* European Number            */
		BIDI_ES,  /* European Separator         */
		BIDI_ET,  /* European Number Terminator */
		BIDI_AN,  /* Arabic Number              */
		BIDI_CS,  /* Common Number Separator    */
		BIDI_NSM, /* Nonspacing Mark            */
		BIDI_BN,  /* Boundary Neutral           */
		BIDI_B,   /* Paragraph Separator        */
		BIDI_S,   /* Segment Separator          */
		BIDI_WS,  /* Whitespace                 */
		BIDI_ON,  /* Other Neutrals             */
		BIDI_LRI, /* Left-to-Right Isolate      */
		BIDI_RLI, /* Right-to-Left Isolate      */
		BIDI_FSI, /* First Strong Isolate       */
		BIDI_PDI  /* Pop Directional Isolate    */
	} bidi;

	enum {
		DECOMP_TYPE_FONT,     /* Font     */
		DECOMP_TYPE_NOBREAK,  /* Nobreak  */
		DECOMP_TYPE_INITIAL,  /* Initial  */
		DECOMP_TYPE_MEDIAL,   /* Medial   */
		DECOMP_TYPE_FINAL,    /* Final    */
		DECOMP_TYPE_ISOLATED, /* Isolated */
		DECOMP_TYPE_CIRCLE,   /* Circle   */
		DECOMP_TYPE_SUPER,    /* Super    */
		DECOMP_TYPE_SUB,      /* Sub      */
		DECOMP_TYPE_VERTICAL, /* Vertical */
		DECOMP_TYPE_WIDE,     /* Wide     */
		DECOMP_TYPE_NARROW,   /* Narrow   */
		DECOMP_TYPE_SMALL,    /* Small    */
		DECOMP_TYPE_SQUARE,   /* Square   */
		DECOMP_TYPE_FRACTION, /* Fraction */
		DECOMP_TYPE_COMPAT    /* Compat   */
	} decomp_type;

	int bidi_mirrored;

	uint32_t lower;
	uint32_t upper;
	uint32_t title;

	uint16_t decomp;
};

#if 0
struct special_case {
	uint32_t *lower;
	uint32_t *title;
	uint32_t *upper;
};
#endif

extern struct codepoint codepoints[];
extern uint16_t stage1[];
extern uint16_t stage2[];
extern uint16_t sequences[];
extern uint32_t compositions[];

struct codepoint *codepoint(uint32_t c);
unsigned write_sequence(uint32_t *buf, uint16_t idx);
uint32_t lookup_comp(uint32_t a, uint32_t b);

#endif
