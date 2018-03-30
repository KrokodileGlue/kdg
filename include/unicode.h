#include <stdint.h>

enum {
	KDGU_CATEGORY_CN, /* Other - not assigned        */
	KDGU_CATEGORY_LU, /* Letter - uppercase          */
	KDGU_CATEGORY_LL, /* Letter - lowercase          */
	KDGU_CATEGORY_LT, /* Letter - titlecase          */
	KDGU_CATEGORY_LM, /* Letter - modifier           */
	KDGU_CATEGORY_LO, /* Letter - other              */
	KDGU_CATEGORY_MN, /* Mark - nonspacing           */
	KDGU_CATEGORY_MC, /* Mark - spacing combining    */
	KDGU_CATEGORY_ME, /* Mark - enclosing            */
	KDGU_CATEGORY_ND, /* Number - decimal digit      */
	KDGU_CATEGORY_NL, /* Number - letter             */
	KDGU_CATEGORY_NO, /* Number - other              */
	KDGU_CATEGORY_PC, /* Punctuation - connector     */
	KDGU_CATEGORY_PD, /* Punctuation - dash          */
	KDGU_CATEGORY_PS, /* Punctuation - open          */
	KDGU_CATEGORY_PE, /* Punctuation - close         */
	KDGU_CATEGORY_PI, /* Punctuation - initial quote */
	KDGU_CATEGORY_PF, /* Punctuation - final quote   */
	KDGU_CATEGORY_PO, /* Punctuation - other         */
	KDGU_CATEGORY_SM, /* Symbol - math               */
	KDGU_CATEGORY_SC, /* Symbol - currency           */
	KDGU_CATEGORY_SK, /* Symbol - modifier           */
	KDGU_CATEGORY_SO, /* Symbol - other              */
	KDGU_CATEGORY_ZS, /* Separator - space           */
	KDGU_CATEGORY_ZL, /* Separator - line            */
	KDGU_CATEGORY_ZP, /* Separator - paragraph       */
	KDGU_CATEGORY_CC, /* Other - control             */
	KDGU_CATEGORY_CF, /* Other - format              */
	KDGU_CATEGORY_CS, /* Other - surrogate           */
	KDGU_CATEGORY_CO  /* Other - private use         */
};

enum {
	KDGU_DECOMP_TYPE_FONT,     /* Font     */
	KDGU_DECOMP_TYPE_NOBREAK,  /* Nobreak  */
	KDGU_DECOMP_TYPE_INITIAL,  /* Initial  */
	KDGU_DECOMP_TYPE_MEDIAL,   /* Medial   */
	KDGU_DECOMP_TYPE_FINAL,    /* Final    */
	KDGU_DECOMP_TYPE_ISOLATED, /* Isolated */
	KDGU_DECOMP_TYPE_CIRCLE,   /* Circle   */
	KDGU_DECOMP_TYPE_SUPER,    /* Super    */
	KDGU_DECOMP_TYPE_SUB,      /* Sub      */
	KDGU_DECOMP_TYPE_VERTICAL, /* Vertical */
	KDGU_DECOMP_TYPE_WIDE,     /* Wide     */
	KDGU_DECOMP_TYPE_NARROW,   /* Narrow   */
	KDGU_DECOMP_TYPE_SMALL,    /* Small    */
	KDGU_DECOMP_TYPE_SQUARE,   /* Square   */
	KDGU_DECOMP_TYPE_FRACTION, /* Fraction */
	KDGU_DECOMP_TYPE_COMPAT,   /* Compat   */
};

enum {
	KDGU_BOUNDCLASS_START,              /* Start                   */
	KDGU_BOUNDCLASS_OTHER,              /* Other                   */
	KDGU_BOUNDCLASS_CR,                 /* Cr                      */
	KDGU_BOUNDCLASS_LF,                 /* Lf                      */
	KDGU_BOUNDCLASS_CONTROL,            /* Control                 */
	KDGU_BOUNDCLASS_EXTEND,             /* Extend                  */
	KDGU_BOUNDCLASS_L,                  /* L                       */
	KDGU_BOUNDCLASS_V,                  /* V                       */
	KDGU_BOUNDCLASS_T,                  /* T                       */
	KDGU_BOUNDCLASS_LV,                 /* Lv                      */
	KDGU_BOUNDCLASS_LVT,                /* Lvt                     */
	KDGU_BOUNDCLASS_REGIONAL_INDICATOR, /* Regional indicator      */
	KDGU_BOUNDCLASS_SPACINGMARK,        /* Spacingmark             */
	KDGU_BOUNDCLASS_PREPEND,            /* Prepend                 */
	KDGU_BOUNDCLASS_ZWJ,                /* Zero Width Joiner       */
	KDGU_BOUNDCLASS_E_BASE,             /* Emoji Base              */
	KDGU_BOUNDCLASS_E_MODIFIER,         /* Emoji Modifier          */
	KDGU_BOUNDCLASS_GLUE_AFTER_ZWJ,     /* Glue_After_ZWJ          */
	KDGU_BOUNDCLASS_E_BASE_GAZ          /* E_BASE + GLUE_AFTER_ZJW */
};

enum {
	KDGU_BIDI_L,   /* Left-to-Right              */
	KDGU_BIDI_LRE, /* Left-to-Right Embedding    */
	KDGU_BIDI_LRO, /* Left-to-Right Override     */
	KDGU_BIDI_R,   /* Right-to-Left              */
	KDGU_BIDI_AL,  /* Right-to-Left Arabic       */
	KDGU_BIDI_RLE, /* Right-to-Left Embedding    */
	KDGU_BIDI_RLO, /* Right-to-Left Override     */
	KDGU_BIDI_PDF, /* Pop Directional Format     */
	KDGU_BIDI_EN,  /* European Number            */
	KDGU_BIDI_ES,  /* European Separator         */
	KDGU_BIDI_ET,  /* European Number Terminator */
	KDGU_BIDI_AN,  /* Arabic Number              */
	KDGU_BIDI_CS,  /* Common Number Separator    */
	KDGU_BIDI_NSM, /* Nonspacing Mark            */
	KDGU_BIDI_BN,  /* Boundary Neutral           */
	KDGU_BIDI_B,   /* Paragraph Separator        */
	KDGU_BIDI_S,   /* Segment Separator          */
	KDGU_BIDI_WS,  /* Whitespace                 */
	KDGU_BIDI_ON,  /* Other Neutrals             */
	KDGU_BIDI_LRI, /* Left-to-Right Isolate      */
	KDGU_BIDI_RLI, /* Right-to-Left Isolate      */
	KDGU_BIDI_FSI, /* First Strong Isolate       */
	KDGU_BIDI_PDI  /* Pop Directional Isolate    */
};

struct kdgu_codepoint {
	uint16_t category;
	uint16_t combining;
	uint16_t bidi;
	uint16_t decomp_type;

	uint16_t decomp;
	uint16_t casefold;

	uint16_t uc;
	uint16_t lc;
	uint16_t tc;

	uint16_t comb;

	unsigned bidi_mirrored;
	unsigned comp_exclusion;
	unsigned ignorable;
	unsigned control_boundary;
	unsigned width;
	unsigned pad;
	unsigned bound;
};

struct kdgu_case {
	uint32_t c;

	unsigned lower_len;
	uint32_t lower[5];

	unsigned title_len;
	uint32_t title[5];

	unsigned upper_len;
	uint32_t upper[5];
};
