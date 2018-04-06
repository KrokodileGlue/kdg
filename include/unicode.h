#ifndef UNICODE_H
#define UNICODE_H

#include <stdlib.h>
#include <stdint.h>

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
};

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
};

enum boundclass {
	BOUNDCLASS_START,              /* Start                   */
	BOUNDCLASS_OTHER,              /* Other                   */
	BOUNDCLASS_CR,                 /* Cr                      */
	BOUNDCLASS_LF,                 /* Lf                      */
	BOUNDCLASS_CONTROL,            /* Control                 */
	BOUNDCLASS_EXTEND,             /* Extend                  */
	BOUNDCLASS_L,                  /* L                       */
	BOUNDCLASS_V,                  /* V                       */
	BOUNDCLASS_T,                  /* T                       */
	BOUNDCLASS_LV,                 /* Lv                      */
	BOUNDCLASS_LVT,                /* Lvt                     */
	BOUNDCLASS_REGIONAL_INDICATOR, /* Regional indicator      */
	BOUNDCLASS_SPACINGMARK,        /* Spacingmark             */
	BOUNDCLASS_PREPEND,            /* Prepend                 */
	BOUNDCLASS_ZWJ,                /* Zero Width Joiner       */
	BOUNDCLASS_E_BASE,             /* Emoji Base              */
	BOUNDCLASS_E_MODIFIER,         /* Emoji Modifier          */
	BOUNDCLASS_GLUE_AFTER_ZWJ,     /* Glue_After_ZWJ          */
	BOUNDCLASS_E_BASE_GAZ          /* E_BASE + GLUE_AFTER_ZJW */
};

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
};

enum normalization {
	NORM_NONE,
	NORM_NFD,
	NORM_NFC,
	NORM_NFKD,
	NORM_NFKC
};

struct codepoint {
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

struct special_case {
	uint32_t c;

	unsigned lower_len;
	uint32_t *lower;

	unsigned title_len;
	uint32_t *title;

	unsigned upper_len;
	uint32_t *upper;
};

extern uint16_t sequences[];
extern uint16_t stage1table[];
extern uint16_t stage2table[];
extern struct codepoint codepoints[];

extern size_t num_special_case;
extern struct special_case special_case[];

#define HANGUL_SBASE  0xAC00
#define HANGUL_LBASE  0x1100
#define HANGUL_VBASE  0x1161
#define HANGUL_TBASE  0x11A7
#define HANGUL_LCOUNT 19
#define HANGUL_VCOUNT 21
#define HANGUL_TCOUNT 28
#define HANGUL_NCOUNT 588
#define HANGUL_SCOUNT 11172

#define HANGUL_L_START  0x1100
#define HANGUL_L_END    0x115A
#define HANGUL_L_FILLER 0x115F
#define HANGUL_V_START  0x1160
#define HANGUL_V_END    0x11A3
#define HANGUL_T_START  0x11A8
#define HANGUL_T_END    0x11FA
#define HANGUL_S_START  0xAC00
#define HANGUL_S_END    0xD7A4

#endif
