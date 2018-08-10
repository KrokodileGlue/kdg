#ifndef UNICODE_DATA_H
#define UNICODE_DATA_H

#include "locale.h"
#include <inttypes.h>

enum category {
	CATEGORY_CN = 1 << 0,  /* Other - not assigned        */
	CATEGORY_LU = 1 << 1,  /* Letter - uppercase          */
	CATEGORY_LL = 1 << 2,  /* Letter - lowercase          */
	CATEGORY_LT = 1 << 3,  /* Letter - titlecase          */
	CATEGORY_LM = 1 << 4,  /* Letter - modifier           */
	CATEGORY_LO = 1 << 5,  /* Letter - other              */
	CATEGORY_MN = 1 << 6,  /* Mark - nonspacing           */
	CATEGORY_MC = 1 << 7,  /* Mark - spacing combining    */
	CATEGORY_ME = 1 << 8,  /* Mark - enclosing            */
	CATEGORY_ND = 1 << 9,  /* Number - decimal digit      */
	CATEGORY_NL = 1 << 10, /* Number - letter             */
	CATEGORY_NO = 1 << 11, /* Number - other              */
	CATEGORY_PC = 1 << 12, /* Punctuation - connector     */
	CATEGORY_PD = 1 << 13, /* Punctuation - dash          */
	CATEGORY_PS = 1 << 14, /* Punctuation - open          */
	CATEGORY_PE = 1 << 15, /* Punctuation - close         */
	CATEGORY_PI = 1 << 16, /* Punctuation - initial quote */
	CATEGORY_PF = 1 << 17, /* Punctuation - final quote   */
	CATEGORY_PO = 1 << 18, /* Punctuation - other         */
	CATEGORY_SM = 1 << 19, /* Symbol - math               */
	CATEGORY_SC = 1 << 20, /* Symbol - currency           */
	CATEGORY_SK = 1 << 21, /* Symbol - modifier           */
	CATEGORY_SO = 1 << 22, /* Symbol - other              */
	CATEGORY_ZS = 1 << 23, /* Separator - space           */
	CATEGORY_ZL = 1 << 24, /* Separator - line            */
	CATEGORY_ZP = 1 << 25, /* Separator - paragraph       */
	CATEGORY_CC = 1 << 26, /* Other - control             */
	CATEGORY_CF = 1 << 27, /* Other - format              */
	CATEGORY_CS = 1 << 28, /* Other - surrogate           */
	CATEGORY_CO = 1 << 29  /* Other - private use         */
};

enum boundclass {
	BOUNDCLASS_START,              /* Start                   */
	BOUNDCLASS_XX,                 /* Other                   */
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

enum bidiclass {
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

enum decomptype {
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

struct codepoint {
	enum category category;
	enum boundclass bound;
	enum bidiclass bidi;
	enum decomptype decomp_type;
	enum script script;

	int bidi_mirrored;
	int ccc;		/* Canonical Combining Class */

	uint16_t lower;
	uint16_t upper;
	uint16_t title;

	uint16_t special_lc;
	uint16_t special_tc;
	uint16_t special_uc;

	uint16_t decomp;
};

struct name {
	uint32_t c;
	char *name;
};

struct name_alias {
	uint32_t c;
	int num;
	char **name;
};

struct named_sequence {
	int num;
	uint32_t *c;
	char *name;
};

/* General category data from `PropertyValueAliases.txt`. */
struct category_alias {
	char *a, *b, *c;
	enum category cat;
};

struct casefold {
	uint32_t c;
	int num;
	uint32_t *name;
};

extern const struct codepoint codepoints[];

extern const struct name names[];
extern const struct name_alias name_aliases[];
extern const struct category_alias category_aliases[];
extern const struct named_sequence named_sequences[];
extern const struct casefold casefold[];

extern const uint16_t stage1[];
extern const uint16_t stage2[];
extern const uint16_t sequences[];
extern const uint32_t compositions[];

const struct codepoint *codepoint(uint32_t c);
unsigned write_sequence(uint32_t *buf, uint16_t idx);
uint32_t lookup_comp(uint32_t a, uint32_t b);
unsigned lookup_fold(uint32_t a, uint32_t **seq);

extern int num_names;
extern int num_name_aliases;
extern int num_category_aliases;
extern int num_casefold;

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
