#include <stdbool.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

#include "ktre.h"

#define WHITESPACE " \t\r\n\v\f"
#define DIGIT      "0123456789"
#define WORD       "_0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"

#include <stdio.h>
#include <assert.h>

#define DBG(...) ((re->opt & KTRE_DEBUG) ? fprintf(stderr, __VA_ARGS__) : 0)

static void print_node(ktre *re, struct node *n);
static void error(ktre *re, enum ktre_error err, int loc, const char *fmt, ...);

static inline bool
is_hex_digit(uint32_t c)
{
	return (c >= 'A' && c <= 'F')
		|| (c >= 'a' && c <= 'f')
		|| (c >= '0' && c <= '9');
}

static void
dbgf(const ktre *re, const kdgu *str, unsigned idx)
{
	if ((re->opt & KTRE_DEBUG) == 0) return;
	if (!str) {
		fputs("null", stderr);
		return;
	}
	for (unsigned i = idx; i < str->len; kdgu_inc(str, &i)) {
		uint32_t c = kdgu_decode(str, i);
		if ((!kdgu_whitespace(str, i) && c != '\\') || c == ' ') {
			kdgu_pchr(str, i, stderr);
			continue;
		}

		fputc('\\', stderr);
		switch (kdgu_decode(str, i)) {
		case '\t': fputc('t', stderr); break;
		case '\r': fputc('r', stderr); break;
		case '\n': fputc('n', stderr); break;
		case '\v': fputc('v', stderr); break;
		case '\f': fputc('f', stderr); break;
		case '\\': fputc('\\',stderr); break;
		default: kdgu_pchr(str, i, stderr);
		}
	}
}

static int
add_group(ktre *re)
{
	if (re->gp >= KTRE_MAX_GROUPS) {
		error(re,
		      KTRE_ERROR_TOO_MANY_GROUPS,
		      re->i,
		      "regex contains too many groups");

		return -1;
	}

	re->group = realloc(re->group, (re->gp + 1) * sizeof *re->group);
	if (!re->group) return -1;

	re->group[re->gp].is_compiled = false;
	re->group[re->gp].address     = -1;
	re->group[re->gp].is_called   = false;
	re->group[re->gp].name        = NULL;

	return re->gp++;
}

struct instr {
	enum {
		INSTR_MATCH,
		INSTR_JMP,
		INSTR_BRANCH,
		INSTR_ANY,
		INSTR_MANY,
		INSTR_CLASS,
		INSTR_TSTR,
		INSTR_STR,
		INSTR_NOT,
		INSTR_BACKREF,
		INSTR_BOL,
		INSTR_EOL,
		INSTR_BOS,
		INSTR_EOS,
		INSTR_SETOPT,
		INSTR_TRY,
		INSTR_CATCH,
		INSTR_SET_START,
		INSTR_WB,
		INSTR_NWB,
		INSTR_SAVE,
		INSTR_CALL,
		INSTR_PLA,
		INSTR_PLA_WIN,
		INSTR_NLA,
		INSTR_NLA_FAIL,
		INSTR_PLB,
		INSTR_PLB_WIN,
		INSTR_NLB,
		INSTR_NLB_FAIL,
		INSTR_PROG,
		INSTR_DIGIT,
		INSTR_SPACE,
		INSTR_WORD,
		INSTR_NDIGIT,
		INSTR_NSPACE,
		INSTR_NWORD,
		INSTR_RET,

		/* Unicode stuff. */

		INSTR_CATEGORY,
		INSTR_CATEGORIES
	} op;

	union {
		struct {
			int a, b;
		};
		uint32_t c;
		kdgu *class;
	};

	int loc;
};

static void
grow_code(ktre *re, int n)
{
	if (!re->instr_alloc) {
		re->instr_alloc = 25;
		re->c = malloc(re->instr_alloc * sizeof *re->c);
	}
	if (!re->c) return;
	if (re->ip + n >= re->instr_alloc) {
		if (re->ip + n >= re->instr_alloc * 2) {
			re->instr_alloc = re->ip + n;
		} else {
			re->instr_alloc *= 2;
		}

		re->c = realloc(re->c, re->instr_alloc * sizeof *re->c);
	}
}

static void
emit_ab(ktre *re, int instr, int a, int b, int loc)
{
	grow_code(re, 1);
	if (!re->c) return;

	re->c[re->ip].op = instr;

	re->c[re->ip].a = a;
	re->c[re->ip].b = b;
	re->c[re->ip].loc = loc;

	re->ip++;
}

static void
emit_c(ktre *re, int instr, int c, int loc)
{
	grow_code(re, 1);
	if (!re->c) return;

	re->c[re->ip].op = instr;
	re->c[re->ip].c = c;
	re->c[re->ip].loc = loc;

	re->ip++;
}

static void
emit_class(ktre *re, int instr, kdgu *class, int loc)
{
	grow_code(re, 1);
	if (!re->c) return;

	re->c[re->ip].op = instr;
	re->c[re->ip].class = class;
	re->c[re->ip].loc = loc;

	re->ip++;
}

static void
emit(ktre *re, int instr, int loc)
{
	grow_code(re, 1);
	if (!re->c) return;

	re->c[re->ip].op = instr;
	re->c[re->ip].loc = loc;
	re->ip++;
}

struct node {
	enum {
		NODE_NONE,
		NODE_SEQUENCE,
		NODE_ASTERISK,
		NODE_PLUS,
		NODE_OR,
		NODE_GROUP,
		NODE_QUESTION,
		NODE_ANY,     /* matches anything                   */
		NODE_MANY,    /* multiline any                      */
		NODE_CLASS,   /* character class                    */
		NODE_NOT,     /* negated character class            */
		NODE_STR,
		NODE_BACKREF, /* backreference to an existing group */
		NODE_BOL,
		NODE_EOL,
		NODE_BOS,     /* beginning of string                */
		NODE_EOS,     /* end of string                      */

		/*
		 * Sets the options for the current thread of
		 * execution.
		 */
		NODE_SETOPT,
		NODE_REP,	/* counted repetition               */
		NODE_ATOM,	/* atomic group                     */

		/*
		 * Sets the start position of the group capturing the
		 * entire match.
		 */
		NODE_SET_START,
		NODE_WB,	/* word boundary                    */
		NODE_NWB,       /* negated word boundary            */
		NODE_CALL,
		NODE_PLA,	/* positive lookahead               */
		NODE_PLB,	/* positive lookbehind              */
		NODE_NLA,	/* negative lookahead               */
		NODE_NLB,	/* negative lookbehind              */

		/*
		 * Attempts to match the entire regex again at the
		 * current point.
		 */
		NODE_RECURSE,

		NODE_DIGIT,
		NODE_SPACE,
		NODE_WORD,

		NODE_NDIGIT,
		NODE_NSPACE,
		NODE_NWORD,

		NODE_CATEGORY,
		NODE_CATEGORIES
	} type;

	/* TODO: Why is this still so terrible? */
	struct node *a, *b;
	int gi; /* group index */
	int32_t c, d;
	kdgu *class;

	int loc;
};

static void
free_node(ktre *re, struct node *n)
{
	/* Sometimes parse errors can produce NULL nodes. */
	if (!n) return;

	switch (n->type) {
	case NODE_SEQUENCE: case NODE_OR:
		free_node(re, n->a);
		free_node(re, n->b);
		break;
	case NODE_QUESTION: case NODE_REP:   case NODE_ASTERISK:
	case NODE_PLUS:     case NODE_GROUP: case NODE_ATOM:
	case NODE_PLA:      case NODE_NLA:   case NODE_PLB:
	case NODE_NLB:
		free_node(re, n->a);
		break;
	case NODE_CLASS: case NODE_NOT: case NODE_STR:
		kdgu_free(n->class);
		break;
	default: break;
	}

	free(n);
}

static void
error(ktre *re, enum ktre_error err, int loc, const char *fmt, ...)
{
	if (re->err) return;

	re->err = err;
	re->loc = loc;

	if (!fmt) {
		re->err_str = NULL;
		return;
	}

	re->err_str = malloc(KTRE_MAX_ERROR_LEN);

	va_list args;
	va_start(args, fmt);
	vsnprintf(re->err_str, KTRE_MAX_ERROR_LEN, fmt, args);
	va_end(args);
}

static struct node *parse(ktre *re);
static struct node *term(ktre *re);

static bool
is_word(ktre *re, uint32_t c) {
	if (re->opt & KTRE_ECMA) return !!strchr(DIGIT, c);
	enum category cat = codepoint(c)->category;
	return cat == CATEGORY_LL
	    || cat == CATEGORY_LU
	    || cat == CATEGORY_LT
	    || cat == CATEGORY_LO
	    || cat == CATEGORY_LM
	    || cat == CATEGORY_ND
	    || cat == CATEGORY_PC;
}

static bool
is_digit(ktre *re, uint32_t c)
{
	if (re->opt & KTRE_ECMA) return !!strchr(DIGIT, c);
	return codepoint(c)->category == CATEGORY_ND;
}

static bool
is_space(ktre *re, uint32_t c)
{
	if (re->opt & KTRE_ECMA) return !!strchr(WHITESPACE, c);
	enum category cat = codepoint(c)->category;
	return strchr(WHITESPACE, c)
		|| c == 0x85
		|| cat == CATEGORY_ZL
		|| cat == CATEGORY_ZP
		|| cat == CATEGORY_ZS;
}

static inline uint32_t
lc(uint32_t c)
{
	return c >= 'A' && c <= 'Z' ? (c - 'A') + 'a' : c;
}

static inline uint32_t
uc(uint32_t c)
{
	return c >= 'a' && c <= 'z' ? (c - 'a') + 'A' : c;
}

static struct node *
new_node(ktre *re)
{
	struct node *n = malloc(sizeof *n);
	if (!n) return NULL;
	memset(n, 0, sizeof *n);
	n->loc = re->i;
	return n;
}

static int
dec_num(ktre *re, int cap)
{
	int n = -1;

	uint32_t c = kdgu_decode(re->s, re->i);
	while ((cap > 0 ? n < cap : true)
	       && re->i < re->s->len
	       && isdigit(c)) {
		if (n < 0) n = 0;
		n = n * 10 + (c - '0');
		kdgu_next(re->s, &re->i);
		c = kdgu_decode(re->s, re->i);
	}

	return (cap > 0 && n > cap) ? kdgu_prev(re->s, &re->i), n / 10 : n;
}

static int
hex_num(ktre *re, int cap)
{
	int n = -1;

	uint32_t c = kdgu_decode(re->s, re->i);
	while ((cap > 0 ? n < cap : true)
	       && re->i < re->s->len
	       && is_hex_digit(c)
	       && kdgu_chrbound(re->s, re->i)) {
		if (n < 0) n = 0;
		c = lc(c), n *= 16;
		n += (c >= 'a' && c <= 'f') ? c - 'a' + 10 : c - '0';
		kdgu_next(re->s, &re->i);
		c = kdgu_decode(re->s, re->i);
	}

	return (cap > 0 && n > cap) ? kdgu_prev(re->s, &re->i), n / 16 : n;
}

static int
oct_num(ktre *re, int cap)
{
	int n = -1;

	uint32_t c = kdgu_decode(re->s, re->i);
	while ((cap > 0 ? n < cap : true)
	       && re->i < re->s->len
	       && c >= '0' && c <= '7') {
		if (n < 0) n = 0;
		n *= 8;
		n += c - '0';
		kdgu_next(re->s, &re->i);
		c = kdgu_decode(re->s, re->i);
	}

	return (cap > 0 && n > cap) ? kdgu_prev(re->s, &re->i), n / 8 : n;
}

static int
parse_dec_num(ktre *re, int cap)
{
	int n = dec_num(re, cap);
	if (n < 0 && !re->err)
		error(re, KTRE_ERROR_SYNTAX_ERROR,
		      re->i, "expected a number");
	return n;
}

static int
parse_hex_num(ktre *re, int cap)
{
	int n = hex_num(re, cap);
	if (n < 0 && !re->err)
		error(re, KTRE_ERROR_SYNTAX_ERROR,
		      re->i, "expected a number");
	return n;
}

static int
parse_oct_num(ktre *re, int cap)
{
	int n = oct_num(re, cap);
	if (n < 0 && !re->err)
		error(re, KTRE_ERROR_SYNTAX_ERROR,
		      re->i, "expected a number");
	return n;
}

static struct node *
parse_mode_modifiers(ktre *re)
{
	struct node *left = new_node(re);
	left->type = NODE_SETOPT;
	bool neg = false;

	/*
	 * Here we preserve the original options in case they
	 * ever need to be restored.
	 */
	int old = re->popt;
	int opt = re->popt;

	while (re->i < re->s->len
	       && !kdgu_chrcmp(re->s, re->i, ')')
	       && !kdgu_chrcmp(re->s, re->i, ':')) {
		bool off = false;
		int  bit = 0;

		if (!kdgu_chrbound(re->s, re->i)) continue;

		switch (kdgu_decode(re->s, re->i)) {
		case 'c': off = true;
		case 'i': bit = KTRE_INSENSITIVE; break;
		case 't': off = true;
		case 'x': bit = KTRE_EXTENDED;    break;
		case 'm': bit = KTRE_MULTILINE;   break;

		case '-':
			neg = true;
			kdgu_next(re->s, &re->i);
			continue;

		default:
			error(re, KTRE_ERROR_SYNTAX_ERROR,
			      re->i,
			      "invalid mode modifier");

			free_node(re, left);
			return NULL;
		}

		if (off || neg) opt &= ~bit;
		else            opt |=  bit;

		kdgu_next(re->s, &re->i);
	}

	re->popt = opt;
	left->c = opt;

	if (kdgu_chrcmp(re->s, re->i, ':')) {
		kdgu_next(re->s, &re->i);

		/*
		 * These are inline mode modifiers: to handle these,
		 * we'll have to put a SETOPT instruction at the
		 * beginning of the group, and another SETOPT at the
		 * end to undo what was done by the first. We also
		 * have to set and restore the parser's options.
		 *
		 * Because the only way we have of stringing nodes
		 * together is by creating SEQUENCE nodes, the code
		 * here is a little ugly.
		 */

		struct node *tmp1 = new_node(re);
		struct node *tmp2 = new_node(re);
		struct node *tmp3 = new_node(re);

		tmp1->type = NODE_SEQUENCE;
		tmp2->type = NODE_SEQUENCE;
		tmp3->type = NODE_SETOPT;

		tmp3->c = old;

		tmp2->a = parse(re);
		tmp2->b = tmp3;

		tmp1->a = left;
		tmp1->b = tmp2;

		left = tmp1;

		re->popt = old;
	}

	return left;
}

static struct node *
parse_branch_reset(ktre *re)
{
	struct node *left = NULL;

	int bottom = re->gp;
	int top = -1;

	do {
		if (kdgu_chrcmp(re->s, re->i, '|')) kdgu_next(re->s, &re->i);
		struct node *tmp = new_node(re);

		if (left) {
			tmp->type = NODE_OR;
			tmp->a = left;
			tmp->b = term(re);
		} else {
			free_node(re, tmp);
			tmp = term(re);
		}

		left = tmp;
		top = top > re->gp ? top : re->gp;
		re->gp = bottom;
	} while (kdgu_chrcmp(re->s, re->i, '|'));

	re->gp = top;

	if (!kdgu_chrcmp(re->s, re->i, ')')) {
		error(re, KTRE_ERROR_SYNTAX_ERROR,
		      re->i,
		      "expected ')'");

		free_node(re, left);
		return NULL;
	}

	return left;
}

static struct node *
parse_named_group(ktre *re)
{
	struct node *left = new_node(re);

	if (strchr(WORD, kdgu_decode(re->s, re->i))
	    && kdgu_chrbound(re->s, re->i)) {
		unsigned a = re->i;
		while (re->i < re->s->len
		       && strchr(WORD, kdgu_decode(re->s, re->i))
		       && kdgu_chrbound(re->s, re->i))
			kdgu_next(re->s, &re->i);
		unsigned b = re->i;

		if (!kdgu_chrcmp(re->s, re->i, '>')) {
			error(re, KTRE_ERROR_SYNTAX_ERROR,
			      re->i,
			      "expected '>'");

			free_node(re, left);
			return NULL;
		}

		kdgu_next(re->s, &re->i);
		left->type = NODE_GROUP;
		left->gi = add_group(re);

		if (left->gi < 0) {
			free_node(re, left);
			return NULL;
		}

		re->group[left->gi].is_called = false;
		re->group[left->gi].name = kdgu_new(re->s->fmt, re->s->s + re->i, b - a);

		left->a = parse(re);
		return left;
	}

	if      (kdgu_chrcmp(re->s, re->i, '=')) left->type = NODE_PLB;
	else if (kdgu_chrcmp(re->s, re->i, '!')) left->type = NODE_NLB;
	else {
		error(re, KTRE_ERROR_SYNTAX_ERROR,
		      left->loc, "invalid group syntax");
		free_node(re, left);
		return NULL;
	}

	kdgu_next(re->s, &re->i);
	left->a = parse(re);

	return left;
}

/* Parses a backreference to a named group. */
static struct node *
parse_named_backreference(ktre *re)
{
	struct node *left = new_node(re);

	if (kdgu_chrcmp(re->s, re->i, '=')) {
		kdgu_next(re->s, &re->i);

		unsigned a = re->i;
		while (re->i < re->s->len
		       && strchr(WORD, kdgu_decode(re->s, re->i))
		       && kdgu_chrbound(re->s, re->i))
			kdgu_next(re->s, &re->i);
		unsigned b = re->i;

		if (!kdgu_chrcmp(re->s, re->i, ')')) {
			error(re, KTRE_ERROR_SYNTAX_ERROR,
			      re->i, "expected ')'");
			free_node(re, left);
			return NULL;
		}

		left->type = NODE_BACKREF;
		left->c = -1;

		for (int i = 0; i < re->gp; i++) {
			if (!re->group[i].name) continue;
			if (kdgu_ncmp(re->group[i].name, re->s, re->i, 0, b - a, false)
			    && b - a == re->group[i].name->len) {
				left->c = i;
				break;
			}
		}

		if (left->c < 0) {
			error(re, KTRE_ERROR_SYNTAX_ERROR,
			      re->i,
			      "name references a group that does not exist");

			free_node(re, left);
			return NULL;
		}

		return left;
	}

	if (!kdgu_chrcmp(re->s, re->i, '<')) {
		error(re, KTRE_ERROR_SYNTAX_ERROR,
		      re->i, "expected '<'");
		free_node(re, left);
		return NULL;
	}

	kdgu_next(re->s, &re->i);
	unsigned a = re->i;
	while (re->i < re->s->len
	       && strchr(WORD, kdgu_decode(re->s, re->i)))
		kdgu_next(re->s, &re->i);
	unsigned b = re->i;

	if (!kdgu_chrcmp(re->s, re->i, '>')) {
		error(re, KTRE_ERROR_SYNTAX_ERROR,
		      re->i, "expected '>'");
		free_node(re, left);
		return NULL;
	}

	kdgu_next(re->s, &re->i);
	left->type = NODE_GROUP;
	left->gi = add_group(re);

	if (left->gi < 0) {
		free_node(re, left);
		return NULL;
	}

	re->group[left->gi].is_called = false;
	re->group[left->gi].name = kdgu_substr(re->s, a, b);

	left->a = parse(re);

	return left;
}

static struct node *
parse_special_group(ktre *re)
{
	struct node *left = NULL;
	uint32_t c = kdgu_decode(re->s, re->i);
	kdgu_next(re->s, &re->i);

	switch (c) {
	case ':': left = parse(re); break;
	case '|': left = parse_branch_reset(re); break;
	case 'P': return parse_named_backreference(re);
	case '<': return parse_named_group(re);

	case '#':
		left = new_node(re);
		left->type = NODE_NONE;
		while (re->i < re->s->len
		       && !kdgu_chrcmp(re->s, re->i, ')'))
			kdgu_next(re->s, &re->i);
		break;

	case '\'': {
		left = new_node(re);
		unsigned a = re->i;
		while (re->i < re->s->len
		       && strchr(WORD, kdgu_decode(re->s, re->i))
		       && kdgu_chrbound(re->s, re->i))
			kdgu_next(re->s, &re->i);
		unsigned b = re->i;

		if (!kdgu_chrcmp(re->s, re->i, '\'')) {
			error(re, KTRE_ERROR_SYNTAX_ERROR,
			      re->i, "expected '\''");
			free_node(re, left);
			return NULL;
		}

		kdgu_next(re->s, &re->i);
		left->type = NODE_GROUP;
		left->gi = add_group(re);

		if (left->gi < 0) {
			free_node(re, left);
			return NULL;
		}

		re->group[left->gi].is_called = false;
		re->group[left->gi].name = kdgu_substr(re->s, a, b);

		left->a = parse(re);
	} break;

	case '>':
		left = new_node(re);
		left->type = NODE_ATOM;
		left->a = parse(re);
		break;

	case '=':
		left = new_node(re);
		left->type = NODE_PLA;
		left->a = parse(re);
		break;

	case '!':
		left = new_node(re);
		left->type = NODE_NLA;
		left->a = parse(re);
		break;

	case '0': case '1': case '2': case '3': case '4':
	case '5': case '6': case '7': case '8': case '9':
		left = new_node(re);
		left->type = NODE_CALL;
		kdgu_prev(re->s, &re->i);
		left->c = parse_dec_num(re, 0);

		if (left->c >= 0 && left->c < re->gp)
			re->group[left->c].is_called = true;
		break;

	default:
		free_node(re, left);
		kdgu_prev(re->s, &re->i);
		left = parse_mode_modifiers(re);
		break;
	}

	return left;
}

static struct node *
parse_group(ktre *re)
{
	struct node *left = new_node(re);
	kdgu_next(re->s, &re->i);

	if (kdgu_ncmp(re->s, &KDGU("?R"), re->i, 0, 2, false)) {
		left->type = NODE_RECURSE;
		kdgu_next(re->s, &re->i), kdgu_next(re->s, &re->i);

		re->group[0].is_called = true;
	} else if (kdgu_chrcmp(re->s, re->i, '?')) {
		kdgu_next(re->s, &re->i);

		free_node(re, left);
		left = parse_special_group(re);
	} else {
		left->type = NODE_GROUP;
		left->gi = add_group(re);

		if (left->gi < 0)
			return free_node(re, left), NULL;

		re->group[left->gi].is_called = false;
		left->a = parse(re);
	}

	if (!kdgu_chrcmp(re->s, re->i, ')') && !re->err) {
		error(re, KTRE_ERROR_SYNTAX_ERROR, left->loc,
		      "unmatched '('");
		return free_node(re, left), NULL;
	}

	kdgu_next(re->s, &re->i);
	return left;
}

/* Parses something like \u{dead beef}, \x28, \U1234, etc. */

static void
unicodepoint(ktre *re, kdgu *a)
{
	uint32_t c = kdgu_decode(re->s, re->i);
	kdgu_next(re->s, &re->i);
	bool bracketed = kdgu_chrcmp(re->s, re->i, '{');

	if (bracketed) {
		kdgu_next(re->s, &re->i);

		do {
			while (is_space(re, kdgu_decode(re->s, re->i)))
				kdgu_next(re->s, &re->i);
			if (kdgu_chrcmp(re->s, re->i, '}'))
				break;
			kdgu_chrappend(a, parse_hex_num(re, 0));
		} while (is_space(re, kdgu_decode(re->s, re->i)));

		if (!kdgu_chrcmp(re->s, re->i, '}') && !re->err)
			error(re, KTRE_ERROR_SYNTAX_ERROR,
			      re->i, "incomplete token");
	} else {
		unsigned idx = re->i;

		for (unsigned i = 0; i < (c == 'x' ? 2 : 4); kdgu_next(re->s, &idx), i++) {
			if (!is_hex_digit(kdgu_decode(re->s, idx))
			    || !kdgu_chrbound(re->s, idx)) {
				error(re, KTRE_ERROR_SYNTAX_ERROR,
				      idx, "expected a hexadecimal digit");
				return;
			}
		}

		kdgu_chrappend(a, parse_hex_num(re, c == 'x' ? 0xFF : 0xFFFF));
		kdgu_prev(re->s, &re->i);
	}
}

static struct {
	char *name;
	char *class;
} pclasses[] = {
	{ "[:upper:]", "ABCDEFGHIJKLMNOPQRSTUVWXYZ" },
	{ "[:lower:]", "abcdefghijklmnopqrstuvwxyz" },
	{ "[:alpha:]", "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ" },
	{ "[:digit:]", "0123456789" },
	{ "[:xdigit:]", "0123456789ABCDEFabcdef" },
	{ "[:alnum:]", "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ" },
	{ "[:punct:]", "!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~" },
	{ "[:blank:]", " \t" },
	{ "[:space:]", WHITESPACE },
	{ "[:cntrl:]",
	  "\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b"
	  "\x0c\x0d\x0e\x0f\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19"
	  "\x1a\x1b\x1c\x1d\x1e\x1f\x7F"
	}
};

/* TODO: [:graph:] and [:print:] */

static kdgu *
parse_character_class_character(ktre *re)
{
	kdgu *a = NULL;

	if (kdgu_chrcmp(re->s, re->i, '[')) {
		for (unsigned i = 0;
		     i < sizeof pclasses / sizeof *pclasses;
		     i++) {
			if (kdgu_ncmp(re->s, &KDGU(pclasses[i].name), re->i, 0, strlen(pclasses[i].name), false)) {
				for (unsigned j = 0, n = strlen(pclasses[i].name); j < n; j++)
					kdgu_next(re->s, &re->i);
				a = kdgu_news(pclasses[i].class), kdgu_convert(a, re->s->fmt);
				return a;
			}
		}

		a = kdgu_chrget(re->s, re->i), kdgu_next(re->s, &re->i);
		return a;
	} else if (!kdgu_chrcmp(re->s, re->i, '\\')) {
		a = kdgu_chrget(re->s, re->i), kdgu_next(re->s, &re->i);
		return a;
	}

	/* Skip over the `\`. */
	kdgu_next(re->s, &re->i);
	if (!kdgu_chrbound(re->s, re->i)) return NULL;
	a = kdgu_new(re->s->fmt, NULL, 0);

	switch (kdgu_decode(re->s, re->i)) {
	case 's': kdgu_append(a, &KDGU(WHITESPACE)); break;
	case 'w': kdgu_append(a, &KDGU(WORD));  break;
	case 'a': kdgu_chrappend(a, '\a'); break;
	case 'b': kdgu_chrappend(a, '\b'); break;
	case 'f': kdgu_chrappend(a, '\f'); break;
	case 'n': kdgu_chrappend(a, '\n'); break;
	case 't': kdgu_chrappend(a, '\t'); break;
	case 'r': kdgu_chrappend(a, '\r'); break;
	case 'e': kdgu_chrappend(a, 7);    break;
	case 'd': kdgu_append(a, &KDGU(DIGIT)); break;
	case 'h': kdgu_append(a, &KDGU(" \t")); break;

	case 'U':
	case 'u':
	case 'x':
		unicodepoint(re, a);
		break;

	case '0':
		kdgu_next(re->s, &re->i);
		kdgu_chrappend(a, parse_oct_num(re, 0));
		kdgu_prev(re->s, &re->i);
		break;

		/* Fuck all this for now. */
#if 0
	case 'D': {
		a = malloc(256);

		for (int j = 1; j < '0'; j++)       a[i++] = j;
		for (int j = '9' + 1; j < 256; j++) a[i++] = j;

		a[i] = 0;
	} break;

	case 'H': {
		a = malloc(256);
		for (int j = 1; j < 256; j++)
			if (!strchr("\t ", j))
				a[i++] = j;
		a[i] = 0;
	} break;

	case 'N': {
		a = malloc(256);
		for (int j = 1; j < 256; j++)
			if (j != '\n')
				a[i++] = j;
		a[i] = 0;
	} break;

	case 'S': {
		a = malloc(256);
		for (int j = 1; j < 256; j++)
			if (!strchr(WHITESPACE, j))
				a[i++] = j;
		a[i] = 0;
	} break;
#endif

	case 'o':
		kdgu_next(re->s, &re->i);
		int loc = re->i;

		if (!kdgu_chrcmp(re->s, re->i, '{') && !re->err) {
			error(re, KTRE_ERROR_SYNTAX_ERROR,
			      loc, "incomplete token");

			return NULL;
		}

		kdgu_next(re->s, &re->i);
		kdgu_chrappend(a, parse_oct_num(re, 0));

		if (!kdgu_chrcmp(re->s, re->i, '}') && !re->err) {
			error(re, KTRE_ERROR_SYNTAX_ERROR,
			      loc, "unmatched '{'");

			return NULL;
		}
		break;

	default:
		kdgu_chrappend(a, kdgu_decode(re->s, re->i));
		break;
	}

	kdgu_next(re->s, &re->i);

	return a;
}

static struct node *
parse_character_class(ktre *re)
{
	struct node *left = new_node(re);
	kdgu *class       = NULL;

	if (kdgu_chrcmp(re->s, re->i, '^')) {
		left->type = NODE_NOT;
		kdgu_next(re->s, &re->i);
	} else {
		left->type = NODE_CLASS;
	}

	while (re->i < re->s->len && !kdgu_chrcmp(re->s, re->i, ']')) {
		kdgu *a    = NULL;
		kdgu *b    = NULL;
		bool range = false;

		a = parse_character_class_character(re);

		if (!a) {
			free_node(re, left);
			return NULL;
		}

		range = (kdgu_chrcmp(re->s, re->i, '-')
		         && !kdgu_chrcmp(re->s, re->i + 1, ']')
		         && kdgu_len(a) == 1);

		if (!range || kdgu_len(a) != 1) {
			if (!class) class = kdgu_copy(a);
			else kdgu_append(class, a);
			kdgu_free(a);
			continue;
		}

		kdgu_next(re->s, &re->i);
		b = parse_character_class_character(re);

		if (!b) {
			kdgu_free(a), free_node(re, left);
			return NULL;
		}

		/*
		 * The entry represented more than one code point so
		 * it can't be a range.
		 */
		if (kdgu_len(b) != 1 || !kdgu_chrbound(b, 0)) {
			if (!class) class = kdgu_copy(a);
			else kdgu_append(class, a);
			kdgu_append(class, b);
			kdgu_free(a), kdgu_free(b);
			continue;
		}

		if (!class) class = kdgu_new(re->s->fmt, NULL, 0);
		for (uint32_t i = kdgu_decode(a, 0), n = kdgu_decode(b, 0); i <= n; i++)
			kdgu_chrappend(class, i);

		kdgu_free(a), kdgu_free(b);
	}

	if (!kdgu_chrcmp(re->s, re->i, ']')) {
		error(re, KTRE_ERROR_SYNTAX_ERROR, left->loc,
		      "unterminated character class");
		kdgu_free(class), free_node(re, left);
		return NULL;
	}

	if (!class) {
		error(re, KTRE_ERROR_SYNTAX_ERROR, left->loc,
		      "empty character class");
		free_node(re, left);
		return NULL;
	}

	left->class = class;

	/* Skip over the `]`. */
	kdgu_next(re->s, &re->i);
	return left;
}

static struct node *
parse_g(ktre *re)
{
	struct node *left = new_node(re);
	kdgu_next(re->s, &re->i);

	bool bracketed = kdgu_chrcmp(re->s, re->i, '{');
	bool neg = false;
	bool pos = false;
	int n;

	if (bracketed) {
		kdgu_next(re->s, &re->i);

		if (kdgu_chrcmp(re->s, re->i, '+')) pos = true;
		if (kdgu_chrcmp(re->s, re->i, '-')) neg = true;

		if (pos || neg) kdgu_next(re->s, &re->i);
		n = parse_dec_num(re, 0);

		if (!kdgu_chrcmp(re->s, re->i, '}') && !re->err) {
			free_node(re, left);
			error(re, KTRE_ERROR_SYNTAX_ERROR,
			      left->loc, "incomplete token");

			return NULL;
		}
	} else {
		if (kdgu_chrcmp(re->s, re->i, '+')) pos = true;
		if (kdgu_chrcmp(re->s, re->i, '-')) neg = true;

		if (pos || neg) kdgu_next(re->s, &re->i);
		n = parse_dec_num(re, 0);

		kdgu_prev(re->s, &re->i);
	}

	if (pos) n = re->gp + n;
	if (neg) n = re->gp - n;

	left->type = NODE_BACKREF;
	left->c = n;

	return left;
}

static struct node *
parse_k(ktre *re)
{
	struct node *left = new_node(re);

	bool bracketed = kdgu_chrcmp(re->s, re->i, '<');
	kdgu_next(re->s, &re->i);

	/*
	 * TODO: Don't use a and b, count the number of graphemes
	 * between them or take the substring and count the length of
	 * that.
	 */
	unsigned a = re->i;
	while (re->i < re->s->len
	       && strchr(WORD, kdgu_decode(re->s, re->i))
	       && kdgu_chrbound(re->s, re->i))
		kdgu_next(re->s, &re->i);
	unsigned b = re->i;

	if ((bracketed && !kdgu_chrcmp(re->s, re->i, '>'))
	    || (!bracketed && !kdgu_chrcmp(re->s, re->i, '\''))
	    || a == b) {
		error(re, KTRE_ERROR_SYNTAX_ERROR,
		      re->i, "expected '>' or '");
		free_node(re, left);
		return NULL;
	}

	left->type = NODE_BACKREF;
	left->c = -1;

	for (int i = 0; i < re->gp; i++) {
		if (!re->group[i].name) continue;
		if (kdgu_ncmp(re->group[i].name, re->s, re->i, 0, b - a, false)
		    && b - a == re->group[i].name->len) {
			left->c = i;
			break;
		}
	}

	if (left->c < 0) {
		error(re, KTRE_ERROR_SYNTAX_ERROR,
		      re->i, "name references a group that does not exist");
		free_node(re, left);
		return NULL;
	}

	return left;
}

static struct {
	char *small, *big, *name;
	enum category cat;
} categories[] = {
	{"Ll", "Lowercase_Letter", "Lowercase Letter", CATEGORY_LL},
	{"Lu", "Uppercase_Letter", "Uppercase Letter", CATEGORY_LU},
	{"Lt", "Titlecase_Letter", "Titlecase Letter", CATEGORY_LT},
	{"Lm", "Modifier_Letter", "Modifier Letter", CATEGORY_LM},
	{"Lo", "Other_Letter", "Other Letter", CATEGORY_LO},
	{"Mn", "Non_Spacing_Mark", "Non Spacing Mark", CATEGORY_MN},
	{"Mc", "Spacing_Combining_Mark", "Spacing Combining Mark", CATEGORY_MC},
	{"Me", "Enclosing_Mark", "Enclosing Mark", CATEGORY_ME},
	{"Zs", "Space_Separator", "Space Separator", CATEGORY_ZS},
	{"Zl", "Line_Separator", "Line Separator", CATEGORY_ZL},
	{"Zp", "Paragraph_Separator", "Paragraph Separator", CATEGORY_ZP},
	{"Sm", "Math_Symbol", "Math Symbol", CATEGORY_SM},
	{"Sc", "Currency_Symbol", "Currency Symbol", CATEGORY_SC},
	{"Sk", "Modifier_Symbol", "Modifier Symbol", CATEGORY_SK},
	{"So", "Other_Symbol", "Other Symbol", CATEGORY_SO},
	{"Nd", "Decimal_Digit_Number", "Decimal Digit Number", CATEGORY_ND},
	{"Nl", "Letter_Number", "Letter Number", CATEGORY_NL},
	{"No", "Other_Number", "Other Number", CATEGORY_NO},
	{"Pd", "Dash_Punctuation", "Dash Punctuation", CATEGORY_PD},
	{"Ps", "Open_Punctuation", "Open Punctuation", CATEGORY_PS},
	{"Pe", "Close_Punctuation", "Close Punctuation", CATEGORY_PE},
	{"Pi", "Initial_Punctuation", "Initial Punctuation", CATEGORY_PI},
	{"Pf", "Final_Punctuation", "Final Punctuation", CATEGORY_PF},
	{"Pc", "Connector_Punctuation", "Connector Punctuation", CATEGORY_PC},
	{"Po", "Other_Punctuation", "Other Punctuation", CATEGORY_PO},
	{"Cc", "Control", "Control", CATEGORY_CC},
	{"Cf", "Format", "Format", CATEGORY_CF},
	{"Co", "Private_Use", "Private Use", CATEGORY_CO},
	{"Cs", "Surrogate", "Surrogate", CATEGORY_CS},
	{"Cn", "Unassigned", "Unassigned", CATEGORY_CN},
};

static struct {
	char *c;
	int num;
	enum category *categories;
} shorthand_categories[] = {
	{"L", 5, (enum category []){CATEGORY_LL,
				    CATEGORY_LU,
				    CATEGORY_LT,
				    CATEGORY_LM,
				    CATEGORY_LO}},
	{"M", 3, (enum category []){CATEGORY_MN,
				    CATEGORY_MC,
				    CATEGORY_ME}},
	{"Z", 3, (enum category []){CATEGORY_ZS,
				    CATEGORY_ZL,
				    CATEGORY_ZP}},
	{"S", 4, (enum category []){CATEGORY_SM,
				    CATEGORY_SC,
				    CATEGORY_SK,
				    CATEGORY_SO}},
	{"N", 3, (enum category []){CATEGORY_ND,
				    CATEGORY_NL,
				    CATEGORY_NO}},
	{"P", 7, (enum category []){CATEGORY_PD,
				    CATEGORY_PS,
				    CATEGORY_PE,
				    CATEGORY_PI,
				    CATEGORY_PF,
				    CATEGORY_PC,
				    CATEGORY_PO}},
	{"C", 5, (enum category []){CATEGORY_CC,
				    CATEGORY_CF,
				    CATEGORY_CO,
				    CATEGORY_CS,
				    CATEGORY_CN}}
};

static struct node *
parse_p(ktre *re)
{
	struct node *left = new_node(re);
	if (!left) return NULL;
	left->c = -1;
	bool bracketed = kdgu_chrcmp(re->s, re->i, '{');

	if (!bracketed) {
		uint32_t c = kdgu_decode(re->s, re->i);
		left->type = NODE_CATEGORIES;

		if (!kdgu_chrbound(re->s, re->i)) {
			free_node(re, left);
			error(re, KTRE_ERROR_SYNTAX_ERROR,
			      re->i, "expected a category");
			return NULL;
		}

		for (unsigned i = 0;
		     i < sizeof shorthand_categories / sizeof *shorthand_categories;
		     i++)
			if (c == (unsigned char)*shorthand_categories[i].c)
				return left->c = i, left;

		free_node(re, left);
		error(re, KTRE_ERROR_SYNTAX_ERROR,
		      re->i, "expected a category");
		return NULL;
	}

	/*
	 * TODO:
	 *
	 * \p{Uppercase=True}
	 * \p{Bidi_Class: Left}
	 * \p{name=control-0007}
	 */

	left->type = NODE_CATEGORY;
	kdgu_next(re->s, &re->i);
	unsigned idx = re->i;
	while (idx < re->s->len && !kdgu_chrcmp(re->s, idx, '}'))
		kdgu_next(re->s, &idx);
	kdgu *substr = kdgu_substr(re->s, re->i, idx);

	if (kdgu_ncmp(&KDGU("name="), re->s, 0, re->i, 5, false)) {
		int loc = re->i;
		re->i = idx, left->type = NODE_STR, idx = 0;
		for (int i = 0; i < 5; i++) kdgu_next(substr, &idx);
		kdgu *name = kdgu_substr(substr, idx, substr->len);
		uint32_t c = kdgu_getcode(name);

		if (c == UINT32_MAX) {
			kdgu_free(substr), kdgu_free(name);
			error(re, KTRE_ERROR_SYNTAX_ERROR,
			      loc + idx, "unknown character name");
			return NULL;
		}

		kdgu *str = kdgu_new(re->s->fmt, NULL, 0);
		kdgu_chrappend(str, c);
		kdgu_free(name), kdgu_free(substr);
		left->class = str;
		return left;
	}

	for (unsigned i = 0;
	     i < sizeof categories / sizeof *categories;
	     i++) {
		if (kdgu_fuzzy(substr, &KDGU(categories[i].small))
		    || kdgu_fuzzy(substr, &KDGU(categories[i].big))) {
			left->c = categories[i].cat;
			re->i = idx;
			break;
		}
	}

	if (left->c < 0)
		for (unsigned i = 0;
		     i < sizeof shorthand_categories / sizeof *shorthand_categories;
		     i++) {
			if (kdgu_fuzzy(substr, &KDGU(shorthand_categories[i].c))) {
				left->type = NODE_CATEGORIES;
				left->c = i;
				re->i = idx;
			}
		}

	kdgu_free(substr);

	if (left->c < 0) {
		free_node(re, left);
		error(re, KTRE_ERROR_SYNTAX_ERROR,
		      re->i, "no such category");
		return NULL;
	}

	if (kdgu_decode(re->s, re->i) != '}') {
		free_node(re, left);
		error(re, KTRE_ERROR_SYNTAX_ERROR,
		      re->i, "expected '}'");
		return NULL;
	}

	return left;
}

static struct node *
parse_N(ktre *re)
{
	struct node *left = new_node(re);
	if (!left) return NULL;
	left->type = NODE_STR;

	/* TODO: Make things that use decode use chrcmp instead. */
	if (!kdgu_chrcmp(re->s, re->i, '{')) {
		free_node(re, left);
		error(re, KTRE_ERROR_SYNTAX_ERROR,
		      re->i, "expected '{'");
		return NULL;
	}

	kdgu_next(re->s, &re->i);
	unsigned idx = re->i;
	while (idx < re->s->len && !kdgu_chrcmp(re->s, idx, '}'))
		kdgu_next(re->s, &idx);

	if (!kdgu_chrcmp(re->s, idx, '}')) {
		free_node(re, left);
		error(re, KTRE_ERROR_SYNTAX_ERROR,
		      idx, "expected '}'");
		return NULL;
	}

	kdgu *substr = kdgu_substr(re->s, re->i, idx);
	uint32_t c = kdgu_getcode(substr);
	kdgu_free(substr);

	if (c == UINT32_MAX) {
		free_node(re, left);
		error(re, KTRE_ERROR_SYNTAX_ERROR,
		      re->i, "unknown character name");
		return NULL;
	}

	re->i = idx;
	left->class = kdgu_new(re->s->fmt, NULL, 0);
	kdgu_chrappend(left->class, c);

	return left;
}

static struct node *
parse_primary(ktre *re)
{
	struct node *left = new_node(re);
	if (!left) return NULL;

	int loc = re->i;

	if (kdgu_chrcmp(re->s, re->i, ')'))
		return free_node(re, left), NULL;

 again:
	if (re->literal) {
		if (kdgu_chrcmp(re->s, re->i, '\\') && kdgu_chrcmp(re->s, 1, 'E')) {
			re->literal = false;
			kdgu_next(re->s, &re->i), kdgu_next(re->s, &re->i);
			goto again;
		}

		left->type = NODE_STR;
		left->class = kdgu_new(re->s->fmt, NULL, 0);
		kdgu_chrappend(left->class, kdgu_decode(re->s, re->i));
		kdgu_next(re->s, &re->i);
		return left;
	}

	if (!kdgu_chrcmp(re->s, re->i, '\\')) {
		switch (kdgu_decode(re->s, re->i)) {
		case '[':
			free_node(re, left);
			kdgu_next(re->s, &re->i);
			left = parse_character_class(re);
			break;

		case '(':
			free_node(re, left);
			left = parse_group(re);
			break;

		case '.':
			kdgu_next(re->s, &re->i);
			left->type = NODE_ANY;
			break;

		case '^':
			kdgu_next(re->s, &re->i);
			left->type = NODE_BOL;
			break;

		case '$':
			kdgu_next(re->s, &re->i);
			left->type = NODE_EOL;
			break;

		case '#':
			if (re->popt & KTRE_EXTENDED) {
				while (kdgu_decode(re->s, re->i) && !kdgu_chrcmp(re->s, re->i, '\n'))
					kdgu_next(re->s, &re->i);
			} else {
				left->type = NODE_STR;
				left->class = kdgu_new(re->s->fmt, NULL, 0);
				kdgu_chrappend(left->class, kdgu_decode(re->s, re->i));
				kdgu_next(re->s, &re->i);
			}
			break;

		default:
			if (re->popt & KTRE_EXTENDED
			    && strchr(WHITESPACE, kdgu_decode(re->s, re->i))) {
				while (re->i < re->s->len
				       && strchr(WHITESPACE, kdgu_decode(re->s, re->i)))
					kdgu_next(re->s, &re->i);

				if (kdgu_decode(re->s, re->i)) goto again;
			} else {
				left->type = NODE_STR;
				left->class = kdgu_new(re->s->fmt, NULL, 0);
				kdgu_chrappend(left->class, kdgu_decode(re->s, re->i));
				kdgu_next(re->s, &re->i);
			}
		}

		if (left) left->loc = loc;
		return left;
	}

	left->type = NODE_CLASS;
	kdgu *a = kdgu_new(re->s->fmt, NULL, 0);
	kdgu_next(re->s, &re->i);
	uint32_t c = kdgu_decode(re->s, re->i);

	/* TODO: Join strings and single-character classes. */

	switch (c) {
	case 'a': kdgu_chrappend(a, '\a');	break;
	case 'f': kdgu_chrappend(a, '\f');	break;
	case 'n': kdgu_chrappend(a, '\n');	break;
	case 't': kdgu_chrappend(a, '\t');	break;
	case 'r': kdgu_chrappend(a, '\r');	break;
	case 'e': kdgu_chrappend(a, 7);		break;
	case 's': left->type = NODE_SPACE;	break;
	case 'K': left->type = NODE_SET_START;	break;
	case 'b': left->type = NODE_WB;		break;
	case 'B': left->type = NODE_NWB;	break;
	case 'A': left->type = NODE_BOS;	break;
	case 'Z': left->type = NODE_EOS;	break;
	case 'd': left->type = NODE_DIGIT;	break;
	case 'w': left->type = NODE_WORD;	break;
	case 'S': left->type = NODE_NSPACE;     break;
	case 'D': left->type = NODE_NDIGIT;     break;
	case 'W': left->type = NODE_NSPACE;     break;
	case 'h':
		kdgu_chrappend(a, '\t');
		kdgu_chrappend(a, ' ');
		break;

	case 'u':
	case 'U':
	case 'x': {
		left->type = NODE_STR;
		unicodepoint(re, a);
		if (re->err) return kdgu_free(a), free_node(re, left), NULL;
	} break;

	case '-': /* backreferences */
	case '+': case '1': case '2': case '3': case '4':
	case '5': case '6': case '7': case '8': case '9': {
		bool neg = (kdgu_chrcmp(re->s, re->i, '-'));
		bool pos = (kdgu_chrcmp(re->s, re->i, '+'));
		if (neg || pos) kdgu_next(re->s, &re->i);

		int n = parse_dec_num(re, 0);

		if (neg) n = re->gp - n;
		if (pos) n = re->gp + n;

		left->type = NODE_BACKREF;
		left->c = n;
		kdgu_prev(re->s, &re->i);
	} break;

	case 'o':
		kdgu_next(re->s, &re->i);
		loc = re->i;

		if (!kdgu_chrcmp(re->s, re->i, '{') && !re->err) {
			kdgu_free(a), free_node(re, left);
			error(re, KTRE_ERROR_SYNTAX_ERROR,
			      loc, "expected '{'");

			return NULL;
		}

		kdgu_next(re->s, &re->i);
		kdgu_chrappend(a, parse_oct_num(re, 0));

		if (!kdgu_chrcmp(re->s, re->i, '}') && !re->err) {
			kdgu_free(a), free_node(re, left);
			error(re, KTRE_ERROR_SYNTAX_ERROR,
			      loc, "unmatched '{'");

			return NULL;
		}
		break;

	case 'Q':
		re->literal = true;
		kdgu_next(re->s, &re->i);
		goto again;

	case 'E':
		re->literal = false;
		kdgu_next(re->s, &re->i);
		goto again;

#if 0
	case 'H': {
		a = malloc(256);
		int i = 0;

		for (int j = 1; j < 256; j++)
			if (!strchr("\t ", j))
				a[i++] = j;

		a[i] = 0;
	} break;

	case 'N': {
		a = malloc(256);
		int i = 0;

		for (int j = 1; j < 256; j++)
			if (j != '\n')
				a[i++] = j;

		a[i] = 0;
	} break;
#endif

	case 'g':
		free_node(re, left), kdgu_next(re->s, &re->i);
		left = parse_g(re);
		break;

	case 'k':
		free_node(re, left), kdgu_next(re->s, &re->i);
		left = parse_k(re);
		break;

	case 'P':
	case 'p':
		free_node(re, left), kdgu_next(re->s, &re->i);
		left = parse_p(re);
		break;

	case 'N':
		free_node(re, left), kdgu_next(re->s, &re->i);
		left = parse_N(re);
		break;

	default:
		kdgu_chrappend(a, kdgu_decode(re->s, re->i));
		break;
	}

	kdgu_next(re->s, &re->i);
	if (!a->len) kdgu_free(a), a = NULL;
	if (left && !left->class) left->class = a;
	if (left) left->loc = loc;

	return left;
}

static struct node *
factor(ktre *re)
{
	if (re->i >= re->s->len) return NULL;

	struct node *left = parse_primary(re);

	while (re->i < re->s->len
	       && (kdgu_chrcmp(re->s, re->i, '*')
		   || kdgu_chrcmp(re->s, re->i, '+')
		   || kdgu_chrcmp(re->s, re->i, '?')
		   || kdgu_chrcmp(re->s, re->i, '{'))) {
		struct node *n = new_node(re);

		switch (kdgu_decode(re->s, re->i)) {
		case '*': n->type = NODE_ASTERISK; kdgu_next(re->s, &re->i); break;
		case '?': n->type = NODE_QUESTION; kdgu_next(re->s, &re->i); break;
		case '+': n->type = NODE_PLUS;     kdgu_next(re->s, &re->i); break;
		case '{': { /* counted repetition */
			n->type = NODE_REP; n->c = -1; n->d = 0;

			kdgu_next(re->s, &re->i);
			n->c = parse_dec_num(re, 0);

			if (kdgu_chrcmp(re->s, re->i, ',')) {
				kdgu_next(re->s, &re->i);
				n->d = isdigit(kdgu_decode(re->s, re->i)) ? parse_dec_num(re, 0) : -1;
			}

			if (!kdgu_chrcmp(re->s, re->i, '}')) {
				error(re, KTRE_ERROR_SYNTAX_ERROR,
				      re->i - 1,
				      "unmatched '{'");
				free_node(re, n);
				free_node(re, left);
				return NULL;
			}

			kdgu_next(re->s, &re->i);
		} break;
		}

		n->a = left;
		left = n;
	}

	if (left) left->loc = re->i - 1;
	return left;
}

static struct node *
term(ktre *re)
{
	struct node *left = new_node(re);
	left->type = NODE_NONE;

	while (re->i < re->s->len
	       && !kdgu_chrcmp(re->s, re->i, '|')
	       && !kdgu_chrcmp(re->s, re->i, ')')) {
		struct node *right = factor(re);

		if (re->err) {
			free_node(re, left);
			free_node(re, right);
			return NULL;
		}

		if (left->type == NODE_NONE) {
			free_node(re, left);
			left = right;
			continue;
		}

		if (left->type == NODE_STR && right->type == NODE_STR) {
			kdgu_append(left->class, right->class);
			free_node(re, right);
		} else if (left->type == NODE_SEQUENCE && left->b->type == NODE_STR && right->type == NODE_STR) {
			kdgu_append(left->b->class, right->class);
			free_node(re, right);
		} else {
			struct node *tmp = new_node(re);

			if (!tmp) {
				free_node(re, left);
				free_node(re, right);
				return NULL;
			}

			tmp->loc = re->i;
			tmp->a = left;
			tmp->b = right;
			tmp->type = NODE_SEQUENCE;
			left = tmp;
		}
	}

	return left;
}

static struct node *
parse(ktre *re)
{
	struct node *n = term(re);

	if (re->i < re->s->len && kdgu_chrcmp(re->s, re->i, '|')) {
		kdgu_next(re->s, &re->i);

		struct node *m = new_node(re);
		m->type = NODE_OR;
		m->a = n;
		m->b = parse(re);

		if (re->err) {
			free_node(re, m->b);
			m->b = NULL;
			return m;
		}

		return m;
	}

	return n;
}

static void
print_node(ktre *re, struct node *n)
{
	if ((re->opt & KTRE_DEBUG) == 0) return;
	static int depth = 0;
	static int l = 0, arm[2048] = { 0 };

	if (depth > 100) return;
	depth++, DBG("\n"), arm[l] = 0;

	for (int i = 0; i < l - 1; i++)
		arm[i] ?    DBG("|   ") : DBG("    ");
	if (l) arm[l - 1] ? DBG("|-- ") : DBG("`-- ");
	if (!n) { DBG("(null)"); return; }

#define N0(...)					\
	do {					\
		DBG(__VA_ARGS__);		\
		DBG(" %d", n->loc);		\
	} while(0)

#define N1(...)                                 \
	do {                                    \
		DBG(__VA_ARGS__);               \
		DBG(" %d", n->loc);             \
		l++; print_node(re, n->a); l--; \
	} while(0)

#define N2(...)					\
	do {					\
		DBG(__VA_ARGS__);		\
		DBG(" %d", n->loc);		\
		l++;				\
		arm[l - 1] = 1;			\
		print_node(re, n->a);		\
		arm[l - 1] = 0;			\
		print_node(re, n->b);		\
		l--;				\
	} while(0)

	switch (n->type) {
	case NODE_ANY:       N0("(any)");                                     break;
	case NODE_MANY:      N0("(m_any)");                                   break;
	case NODE_DIGIT:     N0("(digit)");                                   break;
	case NODE_WORD:      N0("(word)");                                    break;
	case NODE_SPACE:     N0("(space)");                                   break;
	case NODE_NONE:      N0("(none)");                                    break;
	case NODE_WB:        N0("(word boundary)");                           break;
	case NODE_NWB:       N0("(negated word boundary)");                   break;
	case NODE_BACKREF:   N0("(backreference to %d)", n->c);               break;
	case NODE_CLASS:     DBG("(class '"); dbgf(re, n->class, 0); N0("')");       break;
	case NODE_STR:       DBG("(string '"); dbgf(re, n->class, 0); N0("')");      break;
	case NODE_NOT:       DBG("(not '"); dbgf(re, n->class, 0); N0("')");         break;
	case NODE_BOL:       N0("(bol)");                                     break;
	case NODE_EOL:       N0("(eol)");                                     break;
	case NODE_BOS:       N0("(bos)");                                     break;
	case NODE_EOS:       N0("(eos)");                                     break;
	case NODE_RECURSE:   N0("(recurse)");                                 break;
	case NODE_SET_START: N0("(set_start)");                               break;
	case NODE_SETOPT:    N0("(setopt %d)", n->c);                         break;
	case NODE_CALL:      N0("(call %d)", n->c);                           break;
	case NODE_SEQUENCE:  N2 ("(sequence)");                               break;
	case NODE_OR:        N2 ("(or)");                                     break;
	case NODE_REP:       N1 ("(counted repetition %d - %d)", n->c, n->d); break;
	case NODE_ASTERISK:  N1 ("(asterisk)");                               break;
	case NODE_PLUS:      N1 ("(plus)");                                   break;
	case NODE_QUESTION:  N1 ("(question)");                               break;
	case NODE_ATOM:      N1 ("(atom)");                                   break;
	case NODE_PLA:       N1 ("(lookahead)");                              break;
	case NODE_NLA:       N1 ("(negative lookahead)");                     break;
	case NODE_PLB:       N1 ("(lookbehind)");                             break;
	case NODE_NLB:       N1 ("(negative lookbehind)");                    break;
	case NODE_GROUP:
		if (re->group[n->gi].name) {
			dbgf(re, re->group[n->gi].name, 0);
			DBG(" %d", n->loc);
			l++; print_node(re, n->a); l--;
		} else N1("(group %d)", n->gi);
		break;

	case NODE_CATEGORY: {
		char *name = NULL;
		for (unsigned i = 0;
		     !name && i < sizeof categories / sizeof *categories;
		     i++)
			if (n->c == (int)categories[i].cat)
				name = categories[i].name;
		N0("(category %d '%s')", n->c, name);
	} break;

	case NODE_CATEGORIES:
		for (int i = 0; i < shorthand_categories[n->c].num; i++) {
			char *name = NULL;

			for (unsigned j = 0;
			     !name && j < sizeof categories / sizeof *categories;
			     j++)
				if (shorthand_categories[n->c].categories[i] == categories[j].cat)
					name = categories[j].name;

			if (i == 0) {
				DBG("(categories '%s'", name);
				continue;
			}

			DBG("\n");
			for (int j = 0; j < l - 1; j++)
				arm[j] ? DBG("|   ") : DBG("    ");
			DBG("                '%s'", name);
		}
		DBG(")");
		break;

	default:
		DBG("\nunimplemented printer for node of type %d\n", n->type);
		assert(false);
	}

	depth--;
}

static void
print_instruction(ktre *re, struct instr instr)
{
	switch (instr.op) {
	case INSTR_CLASS: DBG("CLASS   '"); dbgf(re, instr.class, 0); DBG("'"); break;
	case INSTR_STR:   DBG("STR     '"); dbgf(re, instr.class, 0); DBG("'"); break;
	case INSTR_NOT:   DBG("NOT     '"); dbgf(re, instr.class, 0); DBG("'"); break;
	case INSTR_TSTR:  DBG("TSTR    '"); dbgf(re, instr.class, 0); DBG("'"); break;
	case INSTR_BRANCH:     DBG("BRANCH   %d, %d", instr.a, instr.b); break;
	case INSTR_SAVE:       DBG("SAVE     %d",  instr.a);             break;
	case INSTR_JMP:        DBG("JMP      %d",  instr.a);             break;
	case INSTR_SETOPT:     DBG("SETOPT   %d",  instr.a);             break;
	case INSTR_BACKREF:    DBG("BACKREF  %d",  instr.a);             break;
	case INSTR_CALL:       DBG("CALL     %d",  instr.a);             break;
	case INSTR_PROG:       DBG("PROG     %d",  instr.a);             break;
	case INSTR_SET_START:  DBG("SET_START");                         break;
	case INSTR_TRY:        DBG("TRY");                               break;
	case INSTR_CATCH:      DBG("CATCH");                             break;
	case INSTR_ANY:        DBG("ANY");                               break;
	case INSTR_MANY:       DBG("MANY");                              break;
	case INSTR_DIGIT:      DBG("DIGIT");                             break;
	case INSTR_WORD:       DBG("WORD");                              break;
	case INSTR_SPACE:      DBG("SPACE");                             break;
	case INSTR_BOL:        DBG("BOL");                               break;
	case INSTR_EOL:        DBG("EOL");                               break;
	case INSTR_BOS:        DBG("BOS");                               break;
	case INSTR_EOS:        DBG("EOS");                               break;
	case INSTR_RET:        DBG("RET");                               break;
	case INSTR_WB:         DBG("WB");                                break;
	case INSTR_NWB:        DBG("NWB");                               break;
	case INSTR_MATCH:      DBG("MATCH");                             break;
	case INSTR_PLA:        DBG("PLA");                               break;
	case INSTR_PLA_WIN:    DBG("PLA_WIN");                           break;
	case INSTR_NLA:        DBG("NLA      %d",  instr.a);             break;
	case INSTR_NLA_FAIL:   DBG("NLA_FAIL");                          break;
	case INSTR_PLB:        DBG("PLB");                               break;
	case INSTR_PLB_WIN:    DBG("PLB_WIN");                           break;
	case INSTR_NLB:        DBG("NLB      %d",  instr.a);             break;
	case INSTR_NLB_FAIL:   DBG("NLB_FAIL");                          break;
	case INSTR_CATEGORY:   DBG("CATEGORY %d", instr.c);              break;
	case INSTR_CATEGORIES: DBG("CATEGORIES %d", instr.c);            break;
	default:
		DBG("\nunimplemented instruction printer %d\n", instr.op);
		assert(false);
	}
}

static bool
is_iteratable(struct node *n)
{
	return n->type != NODE_SETOPT;
}

#define PATCH_A(loc, _a) if (re->c) re->c[loc].a = _a
#define PATCH_B(loc, _b) if (re->c) re->c[loc].b = _b
#define PATCH_C(loc, _c) if (re->c) re->c[loc].c = _c

static void
compile(ktre *re, struct node *n, bool rev)
{
	int a = -1, b = -1;

	if (!n) return;

	if ((n->type == NODE_ASTERISK
	     || n->type == NODE_PLUS
	     || n->type == NODE_QUESTION)
	    && !is_iteratable(n->a)) {
		error(re, KTRE_ERROR_SYNTAX_ERROR,
		      n->loc,
		      "iteration on non-iteratable value");
		return;
	}

	switch (n->type) {
	case NODE_ASTERISK:
		a = re->ip;
		emit_ab(re, INSTR_BRANCH, re->ip + 1, -1, n->loc);
		emit_c(re, INSTR_PROG, re->num_prog++, n->loc);
		compile(re, n->a, rev);
		emit_ab(re, INSTR_BRANCH, a + 1, re->ip + 1, n->loc);
		PATCH_B(a, re->ip);
		break;

	case NODE_QUESTION: {
		switch (n->a->type) {
		case NODE_ASTERISK:
			a = re->ip;
			emit_ab(re, INSTR_BRANCH, -1, re->ip + 1, n->loc);
			emit_c(re, INSTR_PROG, re->num_prog++, n->loc);
			compile(re, n->a->a, rev);
			emit_ab(re, INSTR_BRANCH, re->ip + 1, a + 1, n->loc);
			PATCH_A(a, re->ip);
			break;
		case NODE_PLUS:
			a = re->ip;
			emit_c(re, INSTR_PROG, re->num_prog++, n->loc);
			compile(re, n->a->a, rev);
			emit_ab(re, INSTR_BRANCH, re->ip + 1, a, n->loc);
			break;
		case NODE_QUESTION:
			a = re->ip;
			emit_ab(re, INSTR_BRANCH, -1, re->ip + 1, n->loc);
			emit_c(re, INSTR_PROG, re->num_prog++, n->loc);
			compile(re, n->a->a, rev);
			PATCH_A(a, re->ip);
			break;
		default:
			a = re->ip;
			emit_ab(re, INSTR_BRANCH, re->ip + 1, -1, n->loc);
			compile(re, n->a, rev);
			PATCH_B(a, re->ip);
		}
	} break;

	case NODE_GROUP:
		if (re->group[n->gi].is_called && !re->group[n->gi].is_compiled) {
			emit_c(re, INSTR_CALL, re->ip + 3, n->loc);
			emit_c(re, INSTR_SAVE, n->gi * 2 + 1, n->loc);
			a = re->ip;
			emit_c(re, INSTR_JMP, -1, n->loc);
			emit_c(re, INSTR_SAVE, n->gi * 2, n->loc);

			re->group[n->gi].address = re->ip - 1;
			re->num_groups++;

			compile(re, n->a, rev);

			emit(re, INSTR_RET, n->loc);
			PATCH_C(a, re->ip);

			re->group[n->gi].is_compiled = true;
		} else if (re->group[n->gi].is_compiled) {
			emit_c(re, INSTR_SAVE, n->gi * 2, n->loc);
			compile(re, n->a, rev);
			emit_c(re, INSTR_SAVE, n->gi * 2 + 1, n->loc);
		} else {
			emit_c(re, INSTR_SAVE, n->gi * 2, n->loc);

			re->num_groups++;
			re->group[n->gi].address = re->ip - 1;

			compile(re, n->a, rev);
			emit_c(re, INSTR_SAVE, n->gi * 2 + 1, n->loc);
			re->group[n->gi].is_compiled = true;
		}
		break;

	case NODE_CALL:
		emit_c(re, INSTR_CALL, re->group[n->c].address + 1, n->loc);
		break;

	case NODE_PLUS:
		switch (n->a->type) {
		case NODE_ASTERISK:
		case NODE_PLUS:
		case NODE_QUESTION:
		case NODE_REP:
			emit(re, INSTR_TRY, n->loc);
			emit_c(re, INSTR_PROG, re->num_prog++, n->loc);
			compile(re, n->a, rev);
			emit(re, INSTR_CATCH, n->loc);
			break;
		default:
			a = re->ip;
			emit_c(re, INSTR_PROG, re->num_prog++, n->loc);
			compile(re, n->a, rev);
			emit_ab(re, INSTR_BRANCH, a, re->ip + 1, n->loc);
		}
		break;

	case NODE_OR:
		a = re->ip;
		emit_ab(re, INSTR_BRANCH, re->ip + 1, -1, n->loc);
		compile(re, n->a, rev);
		b = re->ip;
		emit_c(re, INSTR_JMP, -1, n->loc);
		PATCH_B(a, re->ip);
		compile(re, n->b, rev);
		PATCH_C(b, re->ip);
		break;

	case NODE_SEQUENCE:
		if (rev) {
			compile(re, n->b, rev);
			compile(re, n->a, rev);
		} else {
			compile(re, n->a, rev);
			compile(re, n->b, rev);
		}
		break;

	case NODE_RECURSE:
		emit_c(re, INSTR_CALL, re->group[0].address + 1, n->loc);
		break;

	case NODE_BACKREF:
		if (n->c <= 0 || n->c >= re->num_groups) {
			error(re, KTRE_ERROR_SYNTAX_ERROR,
			      n->loc,
			      "backreference number references a group "
			      "that does not yet exist");
			return;
		}

		if (!re->group[n->c].is_compiled) {
			error(re, KTRE_ERROR_SYNTAX_ERROR,
			      n->loc,
			      "backreferences may not reference the "
			      "group they occur in");
			return;
		}

		emit_c(re, INSTR_BACKREF, n->c, n->loc);
		break;

	case NODE_REP:
		a = 0;
		for (int i = 0; i < n->c; i++) {
			a = re->ip;
			if (n->a->type == NODE_GROUP && !re->group[n->a->gi].is_compiled) {
				compile(re, n->a, rev);
			} else if (n->a->type == NODE_GROUP) {
				emit_c(re, INSTR_CALL, re->group[n->a->gi].address + 1, n->loc);
			} else compile(re, n->a, rev);
		}

		if (n->d == -1) {
			if (n->a->type == NODE_GROUP) {
				/* Emit the bytecode for a Kleene star. */
				emit_ab(re, INSTR_BRANCH, re->ip + 1, re->ip + 2, n->loc);
				emit_c(re, INSTR_CALL, re->group[n->a->gi].address + 1, n->loc);
				emit_ab(re, INSTR_BRANCH, re->ip - 1, re->ip + 1, n->loc);
			} else {
				emit_ab(re, INSTR_BRANCH, a, re->ip + 1, n->loc);
			}

			break;
		}

		for (int i = 0; i < n->d - n->c; i++) {
			a = re->ip;
			emit_ab(re, INSTR_BRANCH, re->ip + 1, -1, n->loc);

			if (n->a->type == NODE_GROUP)
				emit_c(re, INSTR_CALL, re->group[n->a->gi].address + 1, n->loc);
			else compile(re, n->a, rev);

			PATCH_B(a, re->ip);
		}
		break;

#define GROUPY(A,B,C)				\
		emit(re, INSTR_##A, n->loc);	\
		compile(re, n->a, B);		\
		emit(re, INSTR_##C, n->loc)	\

	case NODE_ATOM: GROUPY(TRY,rev,CATCH); break;
	case NODE_PLA: GROUPY(PLA,false,PLA_WIN); break;
	case NODE_PLB: GROUPY(PLB,true,PLB_WIN); break;
	case NODE_NLA:
		a = re->ip;
		GROUPY(NLA,false,NLA_FAIL);
		PATCH_C(a, re->ip);
		break;
	case NODE_NLB:
		a = re->ip;
		GROUPY(NLB,true,NLB_FAIL);
		PATCH_C(a, re->ip);
		break;

	case NODE_CLASS:      emit_class(re, INSTR_CLASS,  n->class, n->loc); break;
	case NODE_STR:        emit_class(re, INSTR_STR,    n->class, n->loc); break;
	case NODE_NOT:        emit_class(re, INSTR_NOT,    n->class, n->loc); break;
	case NODE_CATEGORY:   emit_c    (re, INSTR_CATEGORY, n->c,   n->loc); break;
	case NODE_CATEGORIES: emit_c    (re, INSTR_CATEGORIES, n->c, n->loc); break;
	case NODE_SETOPT:     emit_c    (re, INSTR_SETOPT, n->c,     n->loc); break;
	case NODE_BOL:        emit      (re, INSTR_BOL,              n->loc); break;
	case NODE_EOL:        emit      (re, INSTR_EOL,              n->loc); break;
	case NODE_BOS:        emit      (re, INSTR_BOS,              n->loc); break;
	case NODE_EOS:        emit      (re, INSTR_EOS,              n->loc); break;
	case NODE_ANY:        emit      (re, INSTR_ANY,              n->loc); break;
	case NODE_SET_START:  emit      (re, INSTR_SET_START,        n->loc); break;
	case NODE_WB:         emit      (re, INSTR_WB,               n->loc); break;
	case NODE_NWB:        emit      (re, INSTR_NWB,              n->loc); break;
	case NODE_DIGIT:      emit      (re, INSTR_DIGIT,            n->loc); break;
	case NODE_WORD:       emit      (re, INSTR_WORD,             n->loc); break;
	case NODE_SPACE:      emit      (re, INSTR_SPACE,            n->loc); break;
	case NODE_NDIGIT:     emit      (re, INSTR_NDIGIT,           n->loc); break;
	case NODE_NWORD:      emit      (re, INSTR_NWORD,            n->loc); break;
	case NODE_NSPACE:     emit      (re, INSTR_NSPACE,           n->loc); break;
	case NODE_NONE:                                                      break;

	default:
		DBG("\nunimplemented compiler for node of type %d\n", n->type);
		assert(false);
		break;
	}
}

static void
print_compile_error(ktre *re)
{
	if ((re->opt & KTRE_DEBUG) == 0) return;
	DBG("\nfailed to compile with error code %d: %s\n",
	    re->err, re->err_str ? re->err_str : "no error message");
	DBG("\t"), kdgu_print(re->s, stderr), DBG("\n\t");
	for (int i = 0; i < re->loc; i++)
		DBG(kdgu_decode(re->s, i) == '\t' ? "\t" : " ");
	DBG("^\n");
}

static void
print_finish(ktre *re,
	     const kdgu *subject,
	     const kdgu *regex,
	     bool ret,
	     int **vec,
	     kdgu *replaced)
{
	if ((re->opt & KTRE_DEBUG) == 0) return;
	if (!ret && !re->err) { DBG("\nno matches.\n"); return; }
	if (re->err) {
		DBG("\nfailed at runtime with error code %d: %s\n",
		    re->err, re->err_str
		    ? re->err_str
		    : "no error message");
		DBG("\t"), dbgf(re, regex, 0), DBG("\n\t");
		for (int i = 0; i < re->loc; i++) DBG(" ");
		DBG("^\n");
		return;
	}

	for (unsigned i = 0; i < re->num_matches; i++) {
		DBG("\nmatch %d: `", i + 1);
		kdgu *substr = kdgu_substr(subject, vec[i][0], vec[i][0] + vec[i][1]);
		kdgu_print(substr, stderr), kdgu_free(substr);
		DBG("`");

		for (int j = 1; j < re->num_groups; j++) {
			if (vec[i][j * 2 + 1] && !kdgu_chrsize(subject, vec[i][j * 2]))
				continue;
			DBG("\ngroup %d: `", j);
			substr = kdgu_substr(subject,
					     vec[i][j * 2],
					     vec[i][j * 2] + vec[i][j * 2 + 1]);
			kdgu_print(substr, stderr), kdgu_free(substr);
			DBG("`");
		}
	}

	if (replaced) DBG("\nreplace: `"), kdgu_print(replaced, stderr), fputc('`', stderr);
	DBG("\n");
}

ktre *
ktre_compile(const kdgu *pat, int opt)
{
	if (!pat) return NULL;
	ktre *re = malloc(sizeof *re);
	memset(re, 0, sizeof *re);

	if (opt & KTRE_GLOBAL) opt |= KTRE_UNANCHORED;

	re->err_str = "no error";
	re->max_tp  = -1;
	re->popt    = opt;
	re->opt     = opt;
	re->s       = pat;
	re->i       = 0;
	re->n       = new_node(re);

	if (opt & KTRE_DEBUG) {
		DBG("regexpr: "), kdgu_print(pat, stderr);
		if (opt) DBG("\noptions:");
		for (size_t i = 0; i < sizeof opt; i++) {
			switch (opt & 1 << i) {
			case KTRE_INSENSITIVE: DBG("\n\tINSENSITIVE"); break;
			case KTRE_UNANCHORED : DBG("\n\tUNANCHORED "); break;
			case KTRE_EXTENDED   : DBG("\n\tEXTENDED   "); break;
			case KTRE_GLOBAL     : DBG("\n\tGLOBAL     "); break;
			case KTRE_MULTILINE  : DBG("\n\tMULTILINE  "); break;
			case KTRE_CONTINUE   : DBG("\n\tCONTINUE   "); break;
			case KTRE_DEBUG      : DBG("\n\tDEBUG      "); break;
			case KTRE_ECMA       : DBG("\n\tECMA       "); break;
			}
		}
	}

	if ((opt & KTRE_CONTINUE) && (opt & KTRE_GLOBAL)) {
		error(re, KTRE_ERROR_INVALID_OPTIONS, 0,
		      "invalid option configuration: /c and /g may not be used together");

		re->n = NULL;
		print_compile_error(re);
		return re;
	}

	if (!re->n) {
		re->n = NULL;
		print_compile_error(re);
		return re;
	}

	re->n->loc  = 0;
	re->n->type = NODE_GROUP;
	re->n->gi   = add_group(re);

	if (re->n->gi < 0) {
		re->n = NULL;
		print_compile_error(re);
		return re;
	}

	re->group[0].is_compiled = false;
	re->group[0].is_called   = false;
	re->n->a                 = NULL;
	struct node *n           = parse(re);
	re->n->a                 = n;

	if (re->err) {
		free_node(re, re->n);
		re->n = NULL;
		print_compile_error(re);
		return re;
	}

	if (re->i < re->s->len) {
		error(re, KTRE_ERROR_SYNTAX_ERROR,
		      re->i,
		      "unmatched righthand delimiter");

		print_compile_error(re);
		return re;
	}

	print_node(re, re->n);

	if (re->opt & KTRE_UNANCHORED) {
		/*
		 * To implement unanchored matching (the default for
		 * most engines) we could have the VM just try to
		 * match the regex at every position in the string,
		 * but that's inefficient. Instead, we'll just bake
		 * the unanchored matching right into the bytecode by
		 * manually emitting the instructions for `.*?`.
		 */
		emit_ab(re, INSTR_BRANCH, 3, 1, 0);
		emit(re, INSTR_MANY, 0);
		emit_ab(re, INSTR_BRANCH, 3, 1, 0);
	}

	compile(re, re->n, false);
	re->num_groups = re->gp;
	if (re->err) return print_compile_error(re), re;
	emit(re, INSTR_MATCH, re->i);

	if ((opt & KTRE_DEBUG) == 0) return re;

	for (int i = 0; i < re->ip; i++) {
		for (int j = 0; j < re->num_groups; j++)
			if (re->group[j].address == i)
				DBG("\ngroup %d:", j);
		DBG("\n%3d: [%3d] ", i, re->c[i].loc);
		print_instruction(re, re->c[i]);
	}

	return re;
}

ktre *
ktre_copy(ktre *re)
{
	ktre *ret = malloc(sizeof *ret);
	memset(ret, 0, sizeof *ret);
	ret->c = re->c;
	re->copied = true;
	return ret;
}

#define TP (re->tp)
#define THREAD (re->t)

#define MAKE_THREAD_VARIABLE(f,p)					\
	do {								\
		if (!THREAD[TP].f) {					\
			THREAD[TP].f = malloc(    (p + 1) * sizeof *THREAD[TP].f); \
			memset(THREAD[TP].f, -1,  (p + 1) * sizeof *THREAD[TP].f); \
		} else if (THREAD[TP].p < p) {				\
			THREAD[TP].f = realloc(THREAD[TP].f,		\
					       (p + 1) * sizeof *THREAD[TP].f);	\
		}							\
									\
		THREAD[TP].p = p;					\
									\
		if (TP > 0) {						\
			memcpy(THREAD[TP].f,				\
			       THREAD[TP - 1].f,			\
			       THREAD[TP - 1].p > p			\
			       ? p * sizeof *THREAD[0].f		\
			       : THREAD[TP - 1].p * sizeof *THREAD[0].f); \
		}							\
	} while (0)


#define MAKE_STATIC_THREAD_VARIABLE(f,s)				\
	do {								\
		if (!THREAD[TP].f) {					\
			THREAD[TP].f = malloc(    (s) * sizeof *THREAD[TP].f); \
			memset(THREAD[TP].f, -1,  (s) * sizeof *THREAD[TP].f); \
		}							\
									\
		if (TP > 0) {						\
			memcpy(THREAD[TP].f,				\
			       THREAD[TP - 1].f,			\
			       (s) * sizeof *THREAD[0].f);		\
		}							\
	} while (0)

static void
new_thread(ktre *re,
	   int sp,
	   unsigned ip,
	   unsigned opt,
	   unsigned fp,
	   unsigned la,
	   unsigned ep)
{
	++TP;

	if (TP >= re->thread_alloc) {
		if (re->thread_alloc * 2 >= KTRE_MAX_THREAD) {
			re->thread_alloc = KTRE_MAX_THREAD;

			/*
			 * Account for the case where we're just about
			 * to bump up against the thread limit.
			 */
			TP = (TP >= KTRE_MAX_THREAD) ? KTRE_MAX_THREAD - 1 : TP;
		} else re->thread_alloc *= 2;

		re->t = realloc(re->t, re->thread_alloc * sizeof *THREAD);
		memset(&THREAD[TP], 0, (re->thread_alloc - TP) * sizeof *THREAD);
	}

	MAKE_STATIC_THREAD_VARIABLE(vec, re->num_groups * 2);
	MAKE_STATIC_THREAD_VARIABLE(prog, re->num_prog);
	MAKE_THREAD_VARIABLE(frame, fp);
	MAKE_THREAD_VARIABLE(las, la);
	MAKE_THREAD_VARIABLE(exception, ep);

	THREAD[TP].ip  = ip;
	THREAD[TP].sp  = sp;
	THREAD[TP].opt = opt;

	re->max_tp = (TP > re->max_tp) ? TP : re->max_tp;
}

#define FAIL do { --TP; return true; } while (0)
#define PREV (kdgu_prev(subject, &THREAD[TP].sp) || --THREAD[TP].sp)
#define NEXT (kdgu_next(subject, &THREAD[TP].sp) || ++THREAD[TP].sp)

static inline bool
execute_instr(ktre *re,
	      unsigned ip,
	      int sp,
	      unsigned fp,
	      unsigned la,
	      unsigned ep,
	      unsigned opt,
	      unsigned loc,
	      bool rev,
	      unsigned num_steps,
	      const kdgu *subject,
	      int ***vec)
{
	if (re->opt & KTRE_DEBUG) {
		DBG("\n| %4d | %4d | %4d | %4d | %4d | ", ip, sp, TP, fp, num_steps);
		dbgf(re, subject, sp >= 0 ? sp : 0);
	}

	if (THREAD[TP].die) {
		THREAD[TP].die = false;
		FAIL;
	}

	if (sp > (int)subject->len) FAIL;
	uint32_t c = kdgu_decode(subject, sp);

	switch (re->c[ip].op) {
	case INSTR_JMP: THREAD[TP].ip = re->c[ip].c; break;
	case INSTR_BACKREF:
		THREAD[TP].ip++;
		if (kdgu_ncmp(subject,
			      subject,
			      rev ? sp + 1 : sp,
			      THREAD[TP].vec[re->c[ip].c * 2] + (rev ? THREAD[TP].vec[re->c[ip].c * 2 + 1] : 0),
			      rev ? -THREAD[TP].vec[re->c[ip].c * 2 + 1] : THREAD[TP].vec[re->c[ip].c * 2 + 1],
			      (opt & KTRE_INSENSITIVE) != 0))
			THREAD[TP].sp += rev
				? -THREAD[TP].vec[re->c[ip].c * 2 + 1]
				: THREAD[TP].vec[re->c[ip].c * 2 + 1];
		else FAIL;
		break;
	case INSTR_CLASS:
		THREAD[TP].ip++;
		if (kdgu_contains(re->c[ip].class, c))
			kdgu_next(subject, &THREAD[TP].sp);
		else if (opt & KTRE_INSENSITIVE
			 && kdgu_contains(re->c[ip].class, lc(c)))
			kdgu_next(subject, &THREAD[TP].sp);
		else FAIL;
		break;
	case INSTR_STR: case INSTR_TSTR:
		THREAD[TP].ip++;
		if (kdgu_ncmp(subject,
			      re->c[ip].class,
			      sp,
			      rev ? re->c[ip].class->len - 1 : 0,
			      rev ? -re->c[ip].class->len : re->c[ip].class->len,
			      re->opt & KTRE_INSENSITIVE))
			THREAD[TP].sp += rev
				? -(int)re->c[ip].class->len
				: (int)re->c[ip].class->len;
		else FAIL;
		break;
	case INSTR_NOT:
		THREAD[TP].ip++;
		if (kdgu_contains(re->c[ip].class, c)) FAIL;
		kdgu_next(subject, &THREAD[TP].sp);
		break;
	case INSTR_BOL:
		if (!(sp > 0 && kdgu_chrcmp(subject, sp - 1, '\n')) && sp != 0) FAIL;
		THREAD[TP].ip++;
		break;
	case INSTR_EOL:
		if (sp < 0 || (kdgu_chrcmp(subject, sp, '\n')
			       && sp != (int)subject->len))
			FAIL;
		THREAD[TP].ip++;
		break;
	case INSTR_BOS:
		if (sp) FAIL;
		THREAD[TP].ip++;
		break;
	case INSTR_EOS:
		if (sp != (int)subject->len) FAIL;
		THREAD[TP].ip++;
		break;
	case INSTR_WB:
		THREAD[TP].ip++;
		if (sp == 0 && strchr(WORD, c))
			return true;
		if (!!strchr(WORD, c) != !!strchr(WORD, kdgu_decode(subject, sp - 1)))
			return true;
		FAIL;
		break;
	case INSTR_NWB:
		THREAD[TP].ip++;
		if (sp == 0 && !strchr(WORD, c)) return true;
		if (!!strchr(WORD, c) == !!strchr(WORD, kdgu_decode(subject, sp - 1)))
			return true;
		FAIL;
		break;
	case INSTR_ANY:
		THREAD[TP].ip++;
		if (opt & KTRE_MULTILINE ? false : c == '\n') FAIL;
		rev ? PREV : NEXT;
		break;
	case INSTR_MANY:
		THREAD[TP].ip++;
		rev ? PREV : NEXT;
		break;
	case INSTR_BRANCH:
		THREAD[TP].ip = re->c[ip].b;
		new_thread(re, sp, re->c[ip].a, opt, fp, la, ep);
		break;
	case INSTR_MATCH: {
		unsigned n = 0;

		for (unsigned i = 0; i < re->num_matches; i++)
			if (re->vec[i][0] == sp)
				n++;

		if (n) FAIL;
		if ((opt & KTRE_UNANCHORED) == 0 && !(sp >= 0 && sp == (int)subject->len))
			FAIL;

		re->vec = realloc(re->vec, (re->num_matches + 1) * sizeof *re->vec);
		re->cont = sp;

		if (!re->vec) {
			error(re, KTRE_ERROR_OUT_OF_MEMORY, loc, "out of memory");
			return false;
		}

		re->vec[re->num_matches] = malloc(re->num_groups * 2 * sizeof *re->vec);
		if (!re->vec[re->num_matches]) {
			error(re, KTRE_ERROR_OUT_OF_MEMORY, loc, "out of memory");
			return false;
		}

		memcpy(re->vec[re->num_matches++],
		       THREAD[TP].vec,
		       re->num_groups * 2 * sizeof **re->vec);

		if (vec) *vec = re->vec;
		if (!(opt & KTRE_GLOBAL)) return false;

		TP = 0;
		THREAD[TP].ip = 0;
		THREAD[TP].sp = sp;

		if (THREAD[TP].sp > subject->len) return false;
	} break;
	case INSTR_SAVE:
		THREAD[TP].ip++;
		THREAD[TP].vec[re->c[ip].c] = re->c[ip].c % 2 == 0
			? sp
			: sp - THREAD[TP].vec[re->c[ip].c - 1];
		break;
	case INSTR_SETOPT:
		THREAD[TP].ip++;
		THREAD[TP].opt = re->c[ip].c;
		break;
	case INSTR_SET_START:
		THREAD[TP].ip++;
		THREAD[TP].vec[0] = sp;
		break;
	case INSTR_CALL:
		THREAD[TP].ip = re->c[ip].c;
		THREAD[TP].frame = realloc(THREAD[TP].frame, (fp + 1) * sizeof *THREAD[TP].frame);
		THREAD[TP].frame[THREAD[TP].fp++] = ip + 1;
		break;
	case INSTR_RET:
		THREAD[TP].ip = THREAD[TP].frame[--THREAD[TP].fp];
		break;
	case INSTR_PROG:
		THREAD[TP].ip++;
		if (THREAD[TP].prog[re->c[ip].c] == sp) FAIL;
		THREAD[TP].prog[re->c[ip].c] = sp;
		break;
	case INSTR_DIGIT:
		THREAD[TP].ip++;
		if (!is_digit(re, c)) FAIL;
		rev ? PREV : NEXT;
		break;
	case INSTR_WORD:
		THREAD[TP].ip++;
		if (!is_word(re, c)) FAIL;
		rev ? PREV : NEXT;
		break;
	case INSTR_SPACE:
		THREAD[TP].ip++;
		if (!is_space(re, c)) FAIL;
		rev ? PREV : NEXT;
		break;
	case INSTR_NDIGIT:
		THREAD[TP].ip++;
		if (is_digit(re, c)) FAIL;
		rev ? PREV : NEXT;
		break;
	case INSTR_NWORD:
		THREAD[TP].ip++;
		if (is_word(re, c)) FAIL;
		rev ? PREV : NEXT;
		break;
	case INSTR_NSPACE:
		THREAD[TP].ip++;
		if (is_space(re, c)) FAIL;
		rev ? PREV : NEXT;
		break;
	case INSTR_TRY:
		THREAD[TP].ip++;
		THREAD[TP].exception = realloc(THREAD[TP].exception,
					       (ep + 1) * sizeof *THREAD[TP].exception);
		THREAD[TP].exception[THREAD[TP].ep++] = TP;
		break;
	case INSTR_CATCH:
		TP = THREAD[TP].exception[ep - 1];
		THREAD[TP].ip = ip + 1;
		THREAD[TP].sp = sp;
		break;
	case INSTR_PLB:
		THREAD[TP].die = true;
		new_thread(re, sp - 1, ip + 1, opt, fp, la, ep + 1);
		THREAD[TP].exception[ep] = TP - 1;
		THREAD[TP].rev = true;
		break;
	case INSTR_PLB_WIN:
		TP = THREAD[TP].exception[--THREAD[TP].ep];
		THREAD[TP].rev = false;
		THREAD[TP].die = false;
		THREAD[TP].ip = ip + 1;
		break;
	case INSTR_NLB:
		THREAD[TP].ip = re->c[ip].c;
		new_thread(re, sp - 1, ip + 1, opt, fp, la, ep + 1);
		THREAD[TP].exception[ep] = TP - 1;
		THREAD[TP].rev = true;
		break;
	case INSTR_NLB_FAIL:
		TP = THREAD[TP].exception[--THREAD[TP].ep] - 1;
		break;
	case INSTR_PLA:
		THREAD[TP].die = true;
		new_thread(re, sp, ip + 1, opt, fp, la, ep + 1);
		THREAD[TP].exception[ep] = TP - 1;
		break;
	case INSTR_PLA_WIN:
		TP = THREAD[TP].exception[--THREAD[TP].ep];
		THREAD[TP].die = false;
		THREAD[TP].ip = ip + 1;
		break;
	case INSTR_NLA:
		THREAD[TP].ip = re->c[ip].a;
		new_thread(re, sp, ip + 1, opt, fp, la, ep + 1);
		THREAD[TP].exception[ep] = TP - 1;
		break;
	case INSTR_NLA_FAIL:
		TP = THREAD[TP].exception[--THREAD[TP].ep] - 1;
		break;
	case INSTR_CATEGORY:
		THREAD[TP].ip++;
		if (codepoint(c)->category != re->c[ip].c) FAIL;
		rev ? kdgu_dec(subject, &THREAD[TP].sp) || --THREAD[TP].sp
		    : kdgu_inc(subject, &THREAD[TP].sp) || ++THREAD[TP].sp;
		break;
	case INSTR_CATEGORIES:
		THREAD[TP].ip++;
		rev ? kdgu_dec(subject, &THREAD[TP].sp) || --THREAD[TP].sp
		    : kdgu_inc(subject, &THREAD[TP].sp) || ++THREAD[TP].sp;
		enum category cat = codepoint(c)->category;
		for (int i = 0; i < shorthand_categories[re->c[ip].c].num; i++)
			if (shorthand_categories[re->c[ip].c].categories[i] == cat)
				return true;
		FAIL;
		break;
	default:
		DBG("\nunimplemented instruction %d\n", re->c[ip].op);
		assert(false);
	}

	if (TP >= KTRE_MAX_THREAD - 1) {
		error(re, KTRE_ERROR_STACK_OVERFLOW, loc, "regex exceeded the maximum number of executable threads");
		return false;
	}

	if (fp >= KTRE_MAX_CALL_DEPTH - 1) {
		error(re, KTRE_ERROR_CALL_OVERFLOW, loc, "regex exceeded the maximum depth for subroutine calls");
		return false;
	}

	return true;
}

static bool
run(ktre *re, const kdgu *subject, int ***vec)
{
	*vec = NULL;
	re->num_matches = 0;
	TP = -1;

	if (!re->thread_alloc) {
		re->thread_alloc = 25;
		re->t = malloc(re->thread_alloc * sizeof *THREAD);
		if (re->err) return false;
		memset(re->t, 0, re->thread_alloc * sizeof *THREAD);
	}

	if (re->opt & KTRE_CONTINUE && re->cont >= (int)subject->len)
		return false;

	/* Push the initial thread. */
	new_thread(re, re->opt & KTRE_CONTINUE ? re->cont : 0, 0, re->opt, 0, 0, 0);

	unsigned num_steps = 0;
	DBG("\n|   ip |   sp |   tp |   fp | step |");

	while (TP >= 0
	       && execute_instr(re,
				THREAD[TP].ip,
				THREAD[TP].sp,
				THREAD[TP].fp,
				THREAD[TP].la,
				THREAD[TP].ep,
				THREAD[TP].opt,
				re->c[THREAD[TP].ip].loc,
				THREAD[TP].rev,
				num_steps++,
				subject,
				vec));

	return !!re->num_matches;
}

void
ktre_free(ktre *re)
{
	if (re->copied) return;
	free_node(re, re->n);
	if (re->err) free(re->err_str);

	if (re->c) {
		for (int i = 0; i < re->ip; i++)
			if (re->c[i].op == INSTR_TSTR)
				kdgu_free(re->c[i].class);

		free(re->c);
	}

	for (int i = 0; i <= re->max_tp; i++) {
		free(THREAD[i].vec);
		free(THREAD[i].prog);
		free(THREAD[i].frame);
		free(THREAD[i].las);
		free(THREAD[i].exception);
	}

	if (re->vec) {
		for (unsigned i = 0; i < re->num_matches; i++)
			free((re->vec)[i]);

		free(re->vec);
	}

	for (int i = 0; i < re->gp; i++)
		if (re->group[i].name)
			kdgu_free(re->group[i].name);

	free(re->group);
	free(re->t);
	free(re);

	return;
}

_Bool
ktre_exec(ktre *re, const kdgu *subject, int ***vec)
{
	if (re->opt & KTRE_DEBUG) {
		DBG("\nsubject: ");
		dbgf(re, subject, 0);
	}

	if (re->err) {
		if (re->err_str) free(re->err_str);
		re->err = KTRE_ERROR_NO_ERROR;
	}

	int **v = NULL;
	_Bool ret = false;

	if (vec) ret = run(re, subject, vec);
	else     ret = run(re, subject, &v);

	if (vec) print_finish(re, subject, re->s, ret, *vec, NULL);
	else     print_finish(re, subject, re->s, ret, v, NULL);

	return ret;
}

_Bool
ktre_match(const kdgu *subject, const kdgu *pat, int opt, int ***vec)
{
	ktre *re = ktre_compile(pat, opt);

	if (re->err) {
		ktre_free(re);
		return false;
	}

	int **v = NULL;
	bool ret = run(re, subject, vec ? vec : &v);
	print_finish(re, subject, pat, ret, vec ? *vec : v, NULL);
	ktre_free(re);
	return ret;
}

kdgu *
ktre_replace(const kdgu *subject,
	     const kdgu *pat,
	     const kdgu *replacement,
	     const kdgu *indicator,
	     int opt)
{
	ktre *re = ktre_compile(pat, opt);

	if (!re->err) {
		kdgu *ret = ktre_filter(re, subject, replacement, indicator);
		ktre_free(re);
		return ret;
	}

	ktre_free(re);
	return NULL;
}

static void
smartcopy(kdgu *dest,
	  const kdgu *src,
	  unsigned j,
	  unsigned n,
          bool u, bool uch, bool l, bool lch)
{
	for (unsigned i = j; i < j + n; kdgu_next(src, &i)) {
		kdgu *chr = kdgu_chrget(src, i);

		if (i == j && uch) {
			kdgu_uc(chr), kdgu_append(dest, chr), kdgu_free(chr);
			continue;
		}

		if (i == j && lch) {
			kdgu_lc(chr), kdgu_append(dest, chr), kdgu_free(chr);
			continue;
		}

		if (u) kdgu_uc(chr);
		if (l) kdgu_lc(chr);
		kdgu_append(dest, chr);
		kdgu_free(chr);
	}
}

kdgu *
ktre_filter(ktre *re,
	    const kdgu *subject,
	    const kdgu *replacement,
	    const kdgu *indicator)
{
	DBG("\nsubject: "), dbgf(re, subject, 0);

	int **vec = NULL;
	if (!run(re, subject, &vec) || re->err)
		return print_finish(re, subject, re->s, false, vec, NULL), NULL;

	kdgu *ret = NULL;

	for (unsigned i = 0; i < re->num_matches; i++) {
		bool u   = false, l   = false;
		bool uch = false, lch = false;

		if (i > 0) {
			kdgu *substr = kdgu_substr(subject,
						   vec[i - 1][0] + vec[i - 1][1],
						   vec[i][0]);
			kdgu_append(ret, substr), kdgu_free(substr);
		} else
			ret = kdgu_new(subject->fmt, subject->s, vec[i][0]);

		kdgu *match = NULL;

		for (unsigned r = 0; r < replacement->len; kdgu_next(replacement, &r)) {
			if (kdgu_ncmp(replacement, indicator, r, 0, indicator->len, false)) {
				unsigned t = r;
				for (unsigned z = 0; z < kdgu_len(indicator); kdgu_next(indicator, &z))
					kdgu_next(replacement, &t);

				/* TODO: Make this parse full numbers. */
				int n = kdgu_decode(replacement, t) - '0';
				if (n < 0 || n >= re->num_groups) goto skip_capture;
				r = t;

				/* Ignore uninitialized groups. */
				if (vec[i][n * 2] < 0 || vec[i][n * 2 + 1] < 0)
					continue;

				if (!match)
					match = kdgu_new(subject->fmt, NULL, 0);

				smartcopy(match,
					  subject,
					  vec[i][n * 2],
				          vec[i][n * 2 + 1],
					  u, uch, l, lch);

				uch = lch = false;
				continue;
			}

		skip_capture:
			if (!match) match = kdgu_new(subject->fmt, NULL, 0);
			if (!kdgu_chrcmp(replacement, r, '\\')) {
				if (uch || u) kdgu_chrappend(match, uc(kdgu_decode(replacement, r)));
				else if (lch || l) kdgu_chrappend(match, lc(kdgu_decode(replacement, r)));
				else kdgu_chrappend(match, kdgu_decode(replacement, r));
				lch = uch = false;
				continue;
			}

			kdgu_next(replacement, &r);

			switch (kdgu_decode(replacement, r)) {
			case 'U': u =     true;  break;
			case 'L': l =     true;  break;
			case 'E': l = u = false; break;
			case 'l': lch =   true;  break;
			case 'u': uch =   true;  break;

			case '0': case '1': case '2': case '3':
			case '4': case '5': case '6': case '7':
			case '8': case '9': {
				int n = kdgu_decode(replacement, r) - '0';
				kdgu_chrappend(match, n);
			} break;
			}
		}

		if (match) {
			kdgu_append(ret, match);
			kdgu_free(match);
		}
	}

	int end = vec[re->num_matches - 1][0]
		+ vec[re->num_matches - 1][1];
	kdgu *substr = kdgu_substr(subject, end, subject->len);
	kdgu_append(ret, substr);
	print_finish(re, subject, re->s, ret, vec, ret);

	return ret;
}

kdgu **
ktre_split(ktre *re, const kdgu *subject, int *len)
{
	DBG("\nsubject: "), dbgf(re, subject, 0);

	*len = 0;
	int **vec = NULL;

	if (!run(re, subject, &vec) || re->err) {
		print_finish(re, subject, re->s, false, vec, NULL);
		return NULL;
	}

	kdgu **r = NULL;
	unsigned j = 0;

	for (unsigned i = 0; i < re->num_matches; i++) {
		if (vec[i][0] == 0 || vec[i][0] == (int)subject->len) continue;
		r = realloc(r, (*len + 1) * sizeof *r);
		r[*len] = kdgu_new(subject->fmt, subject->s + j, vec[i][0] - j);
		j = vec[i][0] + vec[i][1];
		(*len)++;
	}

	if (subject->len >= j) {
		r = realloc(r, (*len + 1) * sizeof *r);
		r[*len] = kdgu_new(subject->fmt, subject->s + j, subject->len - j);
		(*len)++;
	}

	return r;
}

int **
ktre_getvec(const ktre *re)
{
	int **vec = malloc(re->num_matches * sizeof *re->vec);

	for (unsigned i = 0; i < re->num_matches; i++) {
		vec[i] = malloc(re->num_groups * 2 * sizeof **re->vec);
		memcpy(vec[i], re->vec[i], re->num_groups * 2 * sizeof **re->vec);
	}

	return vec;
}
