#include <stdarg.h>
#include <limits.h>
#include <ctype.h>
#include <assert.h>

#include "ktre.h"

#define SPACE  " \t\n\r\f\v"
#define DIGIT  "0123456789"
#define WORD   "_0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"

/* TODO: https://www.regular-expressions.info/refcharclass.html */

#define DBG(...) ((re->opt & KTRE_DEBUG) ? fprintf(stderr, __VA_ARGS__) : 0)

static void print_node(const ktre *re, struct node *n);
static void error(ktre *re, enum ktre_error err, int loc, const char *fmt, ...);

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

		if (c < 32 || c == 0x7F) {
			fprintf(stderr, "\\U%04"PRIX32, c);
			continue;
		}

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
		INSTR_NCLASS,
		INSTR_TSTR,
		INSTR_STR,
		INSTR_ALT,
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
		INSTR_RANGE,

		/* Unicode properties. */

		INSTR_CATEGORY,
		INSTR_SCRIPT
	} op;

	union {
		struct {
			int32_t a, b;
		};
		uint32_t c;
		kdgu *str;
		struct {        /* Alternation. */
			unsigned num;
			kdgu **list;
		};
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
emit_ab(ktre *re, int instr, int32_t a, int32_t b, int loc)
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
emit_str(ktre *re, int instr, kdgu *str, int loc)
{
	grow_code(re, 1);
	if (!re->c) return;

	re->c[re->ip].op = instr;
	re->c[re->ip].str = str;
	re->c[re->ip].loc = loc;

	re->ip++;
}

static void
emit_alt(ktre *re, int instr, kdgu **list, unsigned num, int loc)
{
	grow_code(re, 1);
	if (!re->c) return;

	re->c[re->ip].op = instr;
	re->c[re->ip].list = list;
	re->c[re->ip].num = num;
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
		NODE_NOT,     /* negated character class            */
		NODE_AND,
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

		NODE_RANGE,
		NODE_DIGIT,
		NODE_SPACE,
		NODE_WORD,

		NODE_NDIGIT,
		NODE_NSPACE,
		NODE_NWORD,

		NODE_CLASS,
		NODE_NCLASS,
		NODE_CATEGORY,
		NODE_SCRIPT,

		NODE_ALT
	} type;

	struct node *a, *b;

	union {
		int32_t c;
		struct {
			int32_t x, y;
		};
		kdgu *str;
		struct {        /* Alternation. */
			unsigned num;
			kdgu **list;
		};
	};

	int gi; /* Group index. */
	int loc;
};

static void
free_node(struct node *n)
{
	/* Sometimes parse errors can produce NULL nodes. */
	if (!n) return;

	switch (n->type) {
	case NODE_SEQUENCE: case NODE_OR:
		free_node(n->a), free_node(n->b);
		break;
	case NODE_QUESTION: case NODE_REP:   case NODE_ASTERISK:
	case NODE_PLUS:     case NODE_GROUP: case NODE_ATOM:
	case NODE_PLA:      case NODE_NLA:   case NODE_PLB:
	case NODE_NLB:      case NODE_NOT:
		free_node(n->a);
		break;
	case NODE_STR: case NODE_CLASS: case NODE_NCLASS:
		kdgu_free(n->str);
		break;
	case NODE_ALT:
		for (unsigned i = 0; i < n->num; i++)
			kdgu_free(n->list[i]);
		free(n->list);
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

static struct node *primary(ktre *re);
static struct node *parse(ktre *re);
static struct node *term(ktre *re);

static bool
is_word(ktre *re, uint32_t c) {
	if (re->opt & KTRE_ECMA) return !!strchr(WORD, c);
	enum category cat = codepoint(c)->category;
	return cat & CATEGORY_LL
	    || cat & CATEGORY_LU
	    || cat & CATEGORY_LT
	    || cat & CATEGORY_LO
	    || cat & CATEGORY_LM
	    || cat & CATEGORY_ND
	    || cat & CATEGORY_PC;
}

static bool
is_digit(ktre *re, uint32_t c)
{
	if (re->opt & KTRE_ECMA) return !!strchr(DIGIT, c);
	return codepoint(c)->category & CATEGORY_ND;
}

static bool
is_space(ktre *re, uint32_t c)
{
	if (re->opt & KTRE_ECMA) return !!strchr(SPACE, c);
	enum category cat = codepoint(c)->category;
	return strchr(SPACE, c)
		|| c == 0x85
		|| cat & CATEGORY_ZL
		|| cat & CATEGORY_ZP
		|| cat & CATEGORY_ZS;
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
new_node(const ktre *re)
{
	struct node *n = malloc(sizeof *n);
	if (!n) return NULL;
	memset(n, 0, sizeof *n);
	n->loc = re->i;
	return n;
}

static struct node *
copy_node(const ktre *re, struct node *n)
{
	if (!n) return NULL;

	struct node *r = new_node(re);
	if (!r) return NULL;
	memcpy(r, n, sizeof *n);

	switch (n->type) {
	case NODE_SEQUENCE: case NODE_OR:
		r->a = copy_node(re, n->a);
		r->b = copy_node(re, n->b);
		break;
	case NODE_QUESTION: case NODE_REP:   case NODE_ASTERISK:
	case NODE_PLUS:     case NODE_GROUP: case NODE_ATOM:
	case NODE_PLA:      case NODE_NLA:   case NODE_PLB:
	case NODE_NLB:      case NODE_NOT:
		r->a = copy_node(re, n->a);
		break;
	case NODE_STR: case NODE_CLASS: case NODE_NCLASS:
		r->str = kdgu_copy(n->str);
		break;
	case NODE_ALT:
		r->list = malloc(n->num * sizeof *n->list);
		for (unsigned i = 0; i < n->num; i++)
			r->list[i] = kdgu_copy(n->list[i]);
		break;
	default: break;
	}

	return r;
}

static struct node *
parse_mode_modifiers(ktre *re)
{
	struct node *left = new_node(re);
	if (!left) return NULL;
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
		case 'S': bit = KTRE_STRETCHY;    break;

		case '-':
			neg = true;
			kdgu_next(re->s, &re->i);
			continue;

		default:
			error(re, KTRE_ERROR_SYNTAX_ERROR,
			      re->i,
			      "invalid mode modifier");

			free_node(left);
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

		if (!tmp1) return NULL;
		if (!tmp2) return NULL;
		if (!tmp3) return NULL;

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
		if (!tmp) return free_node(left), NULL;

		if (left) {
			tmp->type = NODE_OR;
			tmp->a = left;
			tmp->b = term(re);
		} else {
			free_node(tmp);
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

		free_node(left);
		return NULL;
	}

	return left;
}

static struct node *
parse_special_group(ktre *re)
{
	struct node *left = new_node(re);
	if (!left) return NULL;
	uint32_t c = kdgu_decode(re->s, re->i);
	kdgu_next(re->s, &re->i);

	switch (c) {
	case '<': {
		if (kdgu_chrcmp(re->s, re->i, '=')
		    || kdgu_chrcmp(re->s, re->i, '!')) {
			left->type = kdgu_chrcmp(re->s, re->i, '=') ? NODE_PLB : NODE_NLB;
			kdgu_next(re->s, &re->i);
			left->a = parse(re);
			break;
		}

		unsigned a = re->i;
		while (re->i < re->s->len
		       && is_word(re, kdgu_decode(re->s, re->i)))
			kdgu_next(re->s, &re->i);
		unsigned b = re->i;

		if (!kdgu_chrcmp(re->s, re->i, '>')) {
			error(re, KTRE_ERROR_SYNTAX_ERROR,
			      re->i, "expected '>'");
			free_node(left);
			return NULL;
		}

		kdgu_next(re->s, &re->i);
		left->type = NODE_GROUP;
		left->gi = add_group(re);

		if (left->gi < 0) {
			free_node(left);
			return NULL;
		}

		re->group[left->gi].is_called = false;
		re->group[left->gi].name = kdgu_substr(re->s, a, b);

		left->a = parse(re);
	} break;

	case 'P': {
		if (kdgu_chrcmp(re->s, re->i, '=')) {
			kdgu_next(re->s, &re->i);

			unsigned a = re->i;
			while (re->i < re->s->len
			       && is_word(re, kdgu_decode(re->s, re->i))
			       && kdgu_chrbound(re->s, re->i))
				kdgu_next(re->s, &re->i);
			unsigned b = re->i;

			kdgu *substr = kdgu_substr(re->s, a, b);
			left->type = NODE_BACKREF;
			left->c = -1;

			for (int i = 0; i < re->gp && left->c == -1; i++)
				if (kdgu_cmp(re->group[i].name, substr))
					left->c = i;

			kdgu_free(substr);

			if (left->c < 0) {
				free_node(left);
				error(re, KTRE_ERROR_SYNTAX_ERROR,
				      re->i,
				      "name references a group that does not exist");
				return NULL;
			}

			break;
		}

		if (!kdgu_chrcmp(re->s, re->i, '<')) {
			error(re, KTRE_ERROR_SYNTAX_ERROR,
			      re->i, "expected '<' or '='");
			free_node(left);
			return NULL;
		}

		kdgu_next(re->s, &re->i);

		unsigned a = re->i;
		while (re->i < re->s->len
		       && is_word(re, kdgu_decode(re->s, re->i))
		       && kdgu_chrbound(re->s, re->i))
			kdgu_next(re->s, &re->i);
		unsigned b = re->i;

		if (!kdgu_chrcmp(re->s, re->i, '>')) {
			free_node(left);
			error(re, KTRE_ERROR_SYNTAX_ERROR,
			      re->i,
			      "expected '>'");
			return NULL;
		}

		kdgu_next(re->s, &re->i);
		left->type = NODE_GROUP;
		left->gi = add_group(re);

		if (left->gi < 0)
			return free_node(left), NULL;

		re->group[left->gi].is_called = false;
		re->group[left->gi].name = kdgu_substr(re->s, a, b);
		left->a = parse(re);
	} break;

	case ':':
		free_node(left), left = parse(re);
		break;

	case '|':
		free_node(left), left = parse_branch_reset(re);
		break;

	case '#':
		left->type = NODE_NONE;
		while (re->i < re->s->len
		       && !kdgu_chrcmp(re->s, re->i, ')'))
			kdgu_next(re->s, &re->i);
		break;

	case '\'': {
		unsigned a = re->i;
		while (re->i < re->s->len
		       && is_word(re, kdgu_decode(re->s, re->i))
		       && kdgu_chrbound(re->s, re->i))
			kdgu_next(re->s, &re->i);
		unsigned b = re->i;

		if (!kdgu_chrcmp(re->s, re->i, '\'')) {
			error(re, KTRE_ERROR_SYNTAX_ERROR,
			      re->i, "expected '\''");
			free_node(left);
			return NULL;
		}

		kdgu_next(re->s, &re->i);
		left->type = NODE_GROUP;
		left->gi = add_group(re);

		if (left->gi < 0)
			return free_node(left), NULL;

		re->group[left->gi].is_called = false;
		re->group[left->gi].name = kdgu_substr(re->s, a, b);

		left->a = parse(re);
	} break;

	case '>':
		left->type = NODE_ATOM;
		left->a = parse(re);
		break;

	case '=':
		left->type = NODE_PLA;
		left->a = parse(re);
		break;

	case '!':
		left->type = NODE_NLA;
		left->a = parse(re);
		break;

	case '0': case '1': case '2': case '3': case '4':
	case '5': case '6': case '7': case '8': case '9':
		left->type = NODE_CALL;
		kdgu_prev(re->s, &re->i);
		left->c = kdgu_decimal(re->s, &re->i);

		if (left->c >= 0 && left->c < re->gp)
			re->group[left->c].is_called = true;
		break;

	default:
		free_node(left);
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
	if (!left) return NULL;
	kdgu_next(re->s, &re->i);

	if (kdgu_ncmp(re->s, &KDGU("?R"), re->i, 0, 2, false)) {
		left->type = NODE_RECURSE;
		kdgu_next(re->s, &re->i), kdgu_next(re->s, &re->i);

		re->group[0].is_called = true;
	} else if (kdgu_chrcmp(re->s, re->i, '?')) {
		kdgu_next(re->s, &re->i);

		free_node(left);
		left = parse_special_group(re);
	} else {
		left->type = NODE_GROUP;
		left->gi = add_group(re);

		if (left->gi < 0)
			return free_node(left), NULL;

		re->group[left->gi].is_called = false;
		left->a = parse(re);
	}

	if (!left) return NULL;

	if (!kdgu_chrcmp(re->s, re->i, ')') && !re->err) {
		error(re, KTRE_ERROR_SYNTAX_ERROR, left->loc,
		      "unmatched '('");
		return free_node(left), NULL;
	}

	kdgu_next(re->s, &re->i);
	return left;
}

/* Parses something like \u{dead beef}, \x28, \U1234, etc. */

static void
unicodepoint(ktre *re, kdgu *a)
{
	kdgu_next(re->s, &re->i);
	bool bracketed = kdgu_chrcmp(re->s, re->i, '{');

	if (bracketed) {
		kdgu_next(re->s, &re->i);

		do {
			while (is_space(re, kdgu_decode(re->s, re->i)))
				kdgu_next(re->s, &re->i);
			if (kdgu_chrcmp(re->s, re->i, '}'))
				break;
			kdgu_chrappend(a, kdgu_hexadecimal(re->s, &re->i));
		} while (is_space(re, kdgu_decode(re->s, re->i)));

		if (!kdgu_chrcmp(re->s, re->i, '}') && !re->err)
			error(re, KTRE_ERROR_SYNTAX_ERROR,
			      re->i, "incomplete token");
	} else {
		kdgu_chrappend(a, kdgu_hexadecimal(re->s, &re->i));
	}

	kdgu_prev(re->s, &re->i);
}

static struct node *parse_property_class(struct ktre *re, const kdgu *k);

/* It ain't a hack if it works. */
static struct node *
quickparse(const struct ktre *re, const kdgu *k)
{
	struct ktre *r = ktre_compile(k, ((re->popt & ~KTRE_DEBUG) & ~KTRE_STRETCHY) | KTRE_DUMB);
	struct node *n = copy_node(re, r->n->a);
	ktre_free(r);
	return n;
}

static struct node *
parse_posix_character_class(struct ktre *re, struct node *left)
{
	struct node *right = NULL;
	kdgu_next(re->s, &re->i), kdgu_next(re->s, &re->i);

	bool neg = kdgu_chrcmp(re->s, re->i, '^');
	if (neg) kdgu_next(re->s, &re->i);

	unsigned end = re->i;

	while (end < re->s->len
	       && !kdgu_ncmp(&KDGU(":]"), re->s, 0, end, 2, false))
		kdgu_next(re->s, &end);

	if (!kdgu_ncmp(&KDGU(":]"), re->s, 0, end, 2, false)) {
		free_node(left);
		error(re, KTRE_ERROR_SYNTAX_ERROR,
		      re->i, "expected ':]'");
		return NULL;
	}

	kdgu *substr = kdgu_substr(re->s, re->i, end);

#define POSIX_CLASS(X,Y)	  \
	if (!right && kdgu_fuzzy(&KDGU(X), substr)) \
		right = quickparse(re, &KDGU(Y))
	POSIX_CLASS("alnum",  "[A-Za-z0-9]");
	POSIX_CLASS("alpha",  "[A-Za-z]");
	POSIX_CLASS("ascii",  "[\\x00-\\x7F]");
	POSIX_CLASS("blank",  "[ \\t]");
	POSIX_CLASS("cntrl",  "[\\x00-\\x1F\\x7F]");
	POSIX_CLASS("digit",  "[0-9]");
	POSIX_CLASS("graph",  "[^[:cntrl:]]");
	POSIX_CLASS("lower",  "[a-z]");
	POSIX_CLASS("print",  "[[:graph:] ]");
	POSIX_CLASS("punct",  "[-!\"#$%&'()*+,./:;<=>?@[]");
	POSIX_CLASS("space",  "[ \\t\\n\\r\\f\\v]");
	POSIX_CLASS("upper",  "[A-Z]");
	POSIX_CLASS("word",   "[A-Za-z0-9_]");
	POSIX_CLASS("xdigit", "[0-9A-Fa-f]");
	POSIX_CLASS("d",      "[0-9]");
	POSIX_CLASS("s",      "[ \\t\\n\\r\\f\\v]");
	POSIX_CLASS("w",      "[A-Za-z0-9_]");
	POSIX_CLASS("l",      "[a-z]");
	POSIX_CLASS("u",      "[A-Z]");
	POSIX_CLASS("h",      "[ \\t]");
	POSIX_CLASS("v",      "[\\xa-\\xd\x85\\x2028\\x2029]");
	if (!right) right = parse_property_class(re, substr);
	kdgu_free(substr);

	if (neg) {
		struct node *tmp = new_node(re);

		if (!tmp) {
			free_node(left);
			return NULL;
		}

		tmp->type = NODE_NOT;
		tmp->a = right;
		right = tmp;
	}

	re->i = end;
	kdgu_next(re->s, &re->i), kdgu_next(re->s, &re->i);

	return right;
}

static struct node *
parse_character_class_character(struct ktre *re, struct node *left)
{
	struct node *right = NULL;

	if (kdgu_ncmp(&KDGU("[:"), re->s, 0, re->i, 2, false)) {
		right = parse_posix_character_class(re, left);
	} else if (kdgu_chrcmp(re->s, re->i, '\\')) {
		right = primary(re);
	} else {
		bool lit = re->literal;
		re->literal = true;
		right = primary(re);
		re->literal = lit;
	}

	if (!right) return NULL;

	if (kdgu_chrcmp(re->s, re->i + kdgu_chrsize(re->s, re->i), ']')) {
		;
	} else if (kdgu_chrcmp(re->s, re->i, '-')) {
		/* Skip over the `-`. */
		kdgu_next(re->s, &re->i);
		struct node *end = NULL;

		if (kdgu_chrcmp(re->s, re->i, '\\')) {
			end = primary(re);
		} else {
			bool lit = re->literal;
			re->literal = true;
			end = primary(re);
			re->literal = lit;
		}

		if (right->type == NODE_STR
		    && end->type == NODE_STR
		    && kdgu_len(right->str) == 1
		    && kdgu_len(end->str) == 1
		    && kdgu_chrbound(right->str, 0)
		    && kdgu_chrbound(end->str, 0)) {
			struct node *tmp = new_node(re);

			if (!tmp) {
				free_node(left), free_node(right);
				return NULL;
			}

			tmp->type = NODE_RANGE;
			tmp->x = kdgu_decode(right->str, 0);
			tmp->y = kdgu_decode(end->str, 0);

			free_node(right), free_node(end);
			right = tmp;
		} else {
			/* TODO: Character class subtraction. */
			exit(0);
		}
	} else if (kdgu_ncmp(&KDGU("&&"), re->s, 0, re->i, 2, false)) {
		kdgu_next(re->s, &re->i), kdgu_next(re->s, &re->i);
		struct node *end = NULL;

		if (kdgu_ncmp(&KDGU("[:"), re->s, 0, re->i, 2, false)) {
			end = parse_posix_character_class(re, left);
		} else {
			bool lit = re->literal;
			re->literal = true;
			end = primary(re);
			re->literal = lit;
		}

		if (!end) {
			free_node(left), free_node(right);
			return NULL;
		}

		struct node *tmp = new_node(re);

		if (!tmp) {
			free_node(left), free_node(end), free_node(right);
			return NULL;
		}

		tmp->type = NODE_AND;
		tmp->a = right;
		tmp->b = end;
		right = tmp;
	}

	if (!left) left = right;
	else {
		struct node *tmp = new_node(re);

		if (!tmp) {
			free_node(left), free_node(right);
			return NULL;
		}

		tmp->type = NODE_OR;
		tmp->a = left;
		tmp->b = right;
		left = tmp;
	}

	return left;
}

static struct node *
parse_character_class(ktre *re)
{
	bool neg = kdgu_chrcmp(re->s, re->i, '^');
	if (neg) kdgu_next(re->s, &re->i);
	struct node *left = NULL;

	while (re->i < re->s->len
	       && !kdgu_chrcmp(re->s, re->i, ']'))
		left = parse_character_class_character(re, left);

	if (!kdgu_chrcmp(re->s, re->i, ']')) {
		free_node(left);
		error(re, KTRE_ERROR_SYNTAX_ERROR,
		      re->i, "expected ']'");
		return NULL;
	}

	if (neg) {
		struct node *tmp = new_node(re);

		if (!tmp) {
			free_node(left);
			return NULL;
		}

		tmp->type = NODE_NOT;
		tmp->a = left;
		left = tmp;
	}

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
		n = kdgu_decimal(re->s, &re->i);

		if (!kdgu_chrcmp(re->s, re->i, '}') && !re->err) {
			free_node(left);
			error(re, KTRE_ERROR_SYNTAX_ERROR,
			      left->loc, "incomplete token");
			return NULL;
		}
	} else {
		if (kdgu_chrcmp(re->s, re->i, '+')) pos = true;
		if (kdgu_chrcmp(re->s, re->i, '-')) neg = true;

		if (pos || neg) kdgu_next(re->s, &re->i);
		n = kdgu_decimal(re->s, &re->i);

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

	unsigned a = re->i;
	while (re->i < re->s->len
	       && is_word(re, kdgu_decode(re->s, re->i))
	       && kdgu_chrbound(re->s, re->i))
		kdgu_next(re->s, &re->i);
	unsigned b = re->i;

	if ((bracketed && !kdgu_chrcmp(re->s, re->i, '>'))
	    || (!bracketed && !kdgu_chrcmp(re->s, re->i, '\''))
	    || a == b) {
		error(re, KTRE_ERROR_SYNTAX_ERROR,
		      re->i, "expected a matching right-hand delimiter for named backreference");
		free_node(left);
		return NULL;
	}

	left->type = NODE_BACKREF;
	left->c = -1;
	kdgu *substr = kdgu_substr(re->s, a, b);

	for (int i = 0; i < re->gp; i++) {
		if (!re->group[i].name) continue;
		if (kdgu_cmp(re->group[i].name, substr)
		    && b - a == re->group[i].name->len) {
			left->c = i;
			break;
		}
	}

	kdgu_free(substr);

	if (left->c < 0) {
		error(re, KTRE_ERROR_SYNTAX_ERROR,
		      re->i, "name references a group that does not exist");
		free_node(left);
		return NULL;
	}

	return left;
}

static struct node *
parse_property_class(struct ktre *re, const kdgu *k)
{
	struct node *left = new_node(re);
	left->type = NODE_CATEGORY;
	unsigned a = 0, b = 0;

	if (!kdgu_contains(k, ':')
	    && !kdgu_contains(k, '=')
	    && !kdgu_contains(k, 0x2260)) {
		do {
			if (kdgu_chrcmp(k, b, '|'))
				kdgu_next(k, &b), a = b;

			while (b < k->len
			       && !kdgu_chrcmp(k, b, '|')
			       && !kdgu_chrcmp(k, b, '}'))
				kdgu_next(k, &b);

			kdgu *substr = kdgu_substr(k, a, b);
			left->c |= kdgu_getcat(substr);
			kdgu_free(substr);

			if (left->c == (int32_t)UINT32_MAX) {
				free_node(left);
				error(re, KTRE_ERROR_SYNTAX_ERROR,
				      re->i - a, "no such category");
				return NULL;
			}

			a = b;
		} while (kdgu_chrcmp(k, b, '|'));

		return left;
	}

	while (b < k->len
	       && !kdgu_chrcmp(k, b, '=')
	       && !kdgu_chrcmp(k, b, 0x2260)
	       && !kdgu_chrcmp(k, b, ':'))
		kdgu_next(k, &b);

	kdgu *property = kdgu_substr(k, a, b);
	uint32_t op = kdgu_decode(k, b);
	kdgu_next(k, &b), a = b;
	int loc = re->i - b;
	kdgu *value = kdgu_substr(k, b, k->len);

	if (kdgu_fuzzy(&KDGU("name"), property)) {
		uint32_t c = kdgu_getcode(value);

		if (c == UINT32_MAX) {
			kdgu_free(property), kdgu_free(value);
			free_node(left);
			error(re, KTRE_ERROR_SYNTAX_ERROR,
			      loc, "unknown character name");
			return NULL;
		}

		left->type = NODE_STR;
		left->str = kdgu_new(k->fmt, NULL, 0);
		kdgu_chrappend(left->str, c);
	} else if (kdgu_fuzzy(&KDGU("sc"), property)
		   || kdgu_fuzzy(&KDGU("script"), property)) {
		left->c = kdgu_getscript(value);

		if (left->c == -1) {
			kdgu_free(property), kdgu_free(value);
			free_node(left);
			error(re, KTRE_ERROR_SYNTAX_ERROR,
			      loc, "unknown character name");
			return NULL;
		}

		left->type = NODE_SCRIPT;
	} else {
		kdgu_free(property), kdgu_free(value);
		free_node(left);
		error(re, KTRE_ERROR_SYNTAX_ERROR,
		      loc, "unknown property name");
		return NULL;
	}

	if (op == 0x2260) {
		struct node *tmp = new_node(re);
		if (!tmp) return NULL;
		tmp->type = NODE_NOT;
		tmp->a = left;
		left = tmp;
	}

	return left;
}

static struct node *
parse_p(ktre *re)
{
	kdgu *substr = NULL;
	unsigned idx = re->i;

	if (kdgu_chrcmp(re->s, re->i, '{')) {
		kdgu_next(re->s, &re->i);

		while (idx < re->s->len && !kdgu_chrcmp(re->s, idx, '}'))
			kdgu_next(re->s, &idx);

		if (!kdgu_chrcmp(re->s, idx, '}')) {
			error(re, KTRE_ERROR_SYNTAX_ERROR,
			      idx, "expected '}'");
			return NULL;
		}
	} else {
		kdgu_next(re->s, &idx);
	}

	substr = kdgu_substr(re->s, re->i, idx);
	if (kdgu_chrcmp(re->s, re->i, '{')) kdgu_next(re->s, &idx);
	re->i = idx;
	struct node *left = parse_property_class(re, substr);
	kdgu_free(substr);

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
		free_node(left);
		error(re, KTRE_ERROR_SYNTAX_ERROR,
		      re->i, "expected '{'");
		return NULL;
	}

	kdgu_next(re->s, &re->i);
	unsigned idx = re->i;
	while (idx < re->s->len && !kdgu_chrcmp(re->s, idx, '}'))
		kdgu_next(re->s, &idx);

	if (!kdgu_chrcmp(re->s, idx, '}')) {
		free_node(left);
		error(re, KTRE_ERROR_SYNTAX_ERROR,
		      idx, "expected '}'");
		return NULL;
	}

	kdgu *substr = kdgu_substr(re->s, re->i, idx);
	uint32_t c = kdgu_getcode(substr);
	kdgu_free(substr);

	if (c == UINT32_MAX) {
		free_node(left);
		error(re, KTRE_ERROR_SYNTAX_ERROR,
		      re->i, "unknown character name");
		return NULL;
	}

	re->i = idx;
	left->str = kdgu_new(re->s->fmt, NULL, 0);
	kdgu_chrappend(left->str, c);

	return left;
}

static struct node *
primary(ktre *re)
{
	struct node *left = new_node(re);
	if (!left) return NULL;

	int loc = re->i;

 again:
	if (re->literal) {
		if (kdgu_chrcmp(re->s, re->i, '\\') && kdgu_chrcmp(re->s, 1, 'E')) {
			re->literal = false;
			kdgu_next(re->s, &re->i), kdgu_next(re->s, &re->i);
			goto again;
		}

		left->type = NODE_STR;
		left->str = kdgu_new(re->s->fmt, NULL, 0);
		kdgu_chrappend(left->str, kdgu_decode(re->s, re->i));
		kdgu_next(re->s, &re->i);
		return left;
	}

	if (kdgu_chrcmp(re->s, re->i, ')')
	    || kdgu_chrcmp(re->s, re->i, ']'))
		return free_node(left), NULL;

	if (!kdgu_chrcmp(re->s, re->i, '\\')) {
		switch (kdgu_decode(re->s, re->i)) {
		case '[':
			free_node(left);
			kdgu_next(re->s, &re->i);
			left = parse_character_class(re);
			break;

		case '(':
			free_node(left);
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
				while (re->i < re->s->len && !kdgu_chrcmp(re->s, re->i, '\n'))
					kdgu_next(re->s, &re->i);
			} else {
				left->type = NODE_STR;
				left->str = kdgu_new(re->s->fmt, NULL, 0);
				kdgu_chrappend(left->str, kdgu_decode(re->s, re->i));
				kdgu_next(re->s, &re->i);
			}
			break;

		default:
			if (re->popt & KTRE_EXTENDED
			    && is_space(re, kdgu_decode(re->s, re->i))) {
				while (re->i < re->s->len
				       && strchr(SPACE, kdgu_decode(re->s, re->i)))
					kdgu_next(re->s, &re->i);

				if (kdgu_decode(re->s, re->i)) goto again;
			} else {
				if ((re->popt & KTRE_STRETCHY)
				    && kdgu_chrcmp(re->s, re->i, ' ')) {
					left = quickparse(re, &KDGU("\\s+"));
				} else {
					left->type = NODE_STR;
					left->str = kdgu_getchr(re->s, re->i);
				}

				kdgu_next(re->s, &re->i);
			}
		}

		if (left) left->loc = loc;
		return left;
	}

	left->type = NODE_STR;
	kdgu *a = kdgu_new(re->s->fmt, NULL, 0);
	kdgu_next(re->s, &re->i);
	uint32_t c = kdgu_decode(re->s, re->i);

	/* TODO: Join strings and single-character classes. */
	/* TODO: h */

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

	case 'u':
	case 'U':
	case 'x':
		unicodepoint(re, a);
		if (re->err) return kdgu_free(a), free_node(left), NULL;
		break;

	case '-': /* backreferences */
	case '+': case '1': case '2': case '3': case '4':
	case '5': case '6': case '7': case '8': case '9': {
		bool neg = (kdgu_chrcmp(re->s, re->i, '-'));
		bool pos = (kdgu_chrcmp(re->s, re->i, '+'));
		if (neg || pos) kdgu_next(re->s, &re->i);

		int n = kdgu_decimal(re->s, &re->i);

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
			kdgu_free(a), free_node(left);
			error(re, KTRE_ERROR_SYNTAX_ERROR,
			      loc, "expected '{'");

			return NULL;
		}

		kdgu_next(re->s, &re->i);
		kdgu_chrappend(a, kdgu_octal(re->s, &re->i));

		if (!kdgu_chrcmp(re->s, re->i, '}') && !re->err) {
			kdgu_free(a), free_node(left);
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

#if 0 /* TODO */
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
		free_node(left), kdgu_next(re->s, &re->i);
		left = parse_g(re);
		break;

	case 'k':
		free_node(left), kdgu_next(re->s, &re->i);
		left = parse_k(re);
		break;

	case 'P':
	case 'p':
		free_node(left), kdgu_next(re->s, &re->i);
		left = parse_p(re);
		break;

	case 'N':
		free_node(left), kdgu_next(re->s, &re->i);
		left = parse_N(re);
		break;

	default:
		kdgu_chrappend(a, kdgu_decode(re->s, re->i));
		break;
	}

	kdgu_next(re->s, &re->i);
	if (!a->len) kdgu_free(a), a = NULL;
	if (left && !left->str) left->str = a;
	if (left) left->loc = loc;

	return left;
}

static struct node *
factor(ktre *re)
{
	if (re->i >= re->s->len) return NULL;

	struct node *left = primary(re);

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
			n->type = NODE_REP; n->x = -1; n->y = 0;

			kdgu_next(re->s, &re->i);
			n->x = kdgu_decimal(re->s, &re->i);

			if (kdgu_chrcmp(re->s, re->i, ',')) {
				kdgu_next(re->s, &re->i);
				n->y = isdigit(kdgu_decode(re->s, re->i)) ? kdgu_decimal(re->s, &re->i) : -1;
			}

			if (!kdgu_chrcmp(re->s, re->i, '}')) {
				error(re, KTRE_ERROR_SYNTAX_ERROR,
				      re->i - 1,
				      "unmatched '{'");
				free_node(n);
				free_node(left);
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
	if (!left) return NULL;
	left->type = NODE_NONE;

	while (re->i < re->s->len
	       && !kdgu_chrcmp(re->s, re->i, '|')
	       && !kdgu_chrcmp(re->s, re->i, ')')) {
		struct node *right = factor(re);
		if (!right) return NULL;

		if (re->err) {
			free_node(left);
			free_node(right);
			return NULL;
		}

		if (left->type == NODE_NONE) {
			free_node(left);
			left = right;
			continue;
		}

		if (left->type == NODE_STR && right->type == NODE_STR) {
			kdgu_append(left->str, right->str);
			free_node(right);
		} else if (left->type == NODE_SEQUENCE && left->b->type == NODE_STR && right->type == NODE_STR) {
			kdgu_append(left->b->str, right->str);
			free_node(right);
		} else {
			struct node *tmp = new_node(re);

			if (!tmp) {
				free_node(left);
				free_node(right);
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
			free_node(m->b);
			m->b = NULL;
			return m;
		}

		return m;
	}

	return n;
}

static void
print_node(const ktre *re, struct node *n)
{
	if ((re->opt & KTRE_DEBUG) == 0) return;
	static int depth = 0;
	static int l = 0, arm[2048] = {0};

	if (depth > 100) return;
	depth++, (depth != 1 && DBG("\n")), arm[l] = 0;

	for (int i = 0; i < l - 1; i++)
		arm[i] ?    DBG("│   ") : DBG("    ");
	if (l) arm[l - 1] ? DBG("├───") : DBG("╰───");
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
	case NODE_MANY:      N0("(multiline any)");                           break;
	case NODE_DIGIT:     N0("(digit)");                                   break;
	case NODE_WORD:      N0("(word)");                                    break;
	case NODE_SPACE:     N0("(space)");                                   break;
	case NODE_NSPACE:    N0("(non space)");                               break;
	case NODE_NONE:      N0("(none)");                                    break;
	case NODE_WB:        N0("(word boundary)");                           break;
	case NODE_NWB:       N0("(negated word boundary)");                   break;
	case NODE_BACKREF:   N0("(backreference to %d)", n->c);               break;
	case NODE_CLASS:     DBG("(class '");  dbgf(re, n->str, 0); N0("')"); break;
	case NODE_NCLASS:    DBG("(nclass '"); dbgf(re, n->str, 0); N0("')"); break;
	case NODE_STR:       DBG("(string '"); dbgf(re, n->str, 0); N0("')"); break;
	case NODE_NOT:       N1("(not)");                                     break;
	case NODE_BOL:       N0("(bol)");                                     break;
	case NODE_EOL:       N0("(eol)");                                     break;
	case NODE_BOS:       N0("(bos)");                                     break;
	case NODE_EOS:       N0("(eos)");                                     break;
	case NODE_RECURSE:   N0("(recurse)");                                 break;
	case NODE_SET_START: N0("(set_start)");                               break;
	case NODE_SETOPT:    N0("(setopt %d)", n->c);                         break;
	case NODE_CALL:      N0("(call %d)", n->c);                           break;
	case NODE_SEQUENCE:  N2("(sequence)");                                break;
	case NODE_AND:       N2("(and)");                                     break;
	case NODE_OR:        N2("(or)");                                      break;
	case NODE_REP:       N1("(counted repetition %d - %d)", n->x, n->y);  break;
	case NODE_ASTERISK:  N1("(asterisk)");                                break;
	case NODE_PLUS:      N1("(plus)");                                    break;
	case NODE_QUESTION:  N1("(question)");                                break;
	case NODE_ATOM:      N1("(atom)");                                    break;
	case NODE_PLA:       N1("(lookahead)");                               break;
	case NODE_NLA:       N1("(negative lookahead)");                      break;
	case NODE_PLB:       N1("(lookbehind)");                              break;
	case NODE_NLB:       N1("(negative lookbehind)");                     break;
	case NODE_GROUP:
		if (re->group[n->gi].name) {
			DBG("(group '");
			dbgf(re, re->group[n->gi].name, 0);
			DBG("') %d", n->loc);
			l++; print_node(re, n->a); l--;
		} else N1("(group %d)", n->gi);
		break;

	case NODE_CATEGORY: {
		bool first = true;
		for (int i = 0; i < 30; i++) {
			if (!(n->c & (1 << i))) continue;
			char *name = kdgu_getcatname(n->c & (1 << i));

			if (first) {
				DBG("(category '%s'", name);
				first = false;
				continue;
			}

			DBG("\n");
			for (int j = 0; j < l; j++)
				arm[j] ? DBG("|   ") : DBG("    ");
			DBG("          '%s'", name);
		}
		DBG(")");
	} break;

	case NODE_ALT: {
		bool first = true;
		for (unsigned i = 0; i < n->num; i++) {
			kdgu *str = n->list[i];

			if (first) {
				DBG("(alternation '");
				kdgu_print(str, stderr);
				DBG("'");
				first = false;
				continue;
			}

			DBG("\n");
			for (int j = 0; j < l; j++)
				arm[j] ? DBG("|   ") : DBG("    ");
			DBG("             '");
			kdgu_print(str, stderr);
			DBG("'");
		}
		DBG(")");
	} break;

	case NODE_RANGE:
		N0("(range U+%04"PRIX32" - U+%04"PRIX32")", n->x, n->y);
		break;

	case NODE_SCRIPT:
		N0("(script %s)", kdgu_getscriptname(n->c));
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
	case INSTR_CLASS:  DBG("CLASS   '"); dbgf(re, instr.str, 0); DBG("'"); break;
	case INSTR_NCLASS: DBG("NCLASS  '"); dbgf(re, instr.str, 0); DBG("'"); break;
	case INSTR_STR:    DBG("STR     '"); dbgf(re, instr.str, 0); DBG("'"); break;
	case INSTR_NOT:    DBG("NOT     '"); dbgf(re, instr.str, 0); DBG("'"); break;
	case INSTR_TSTR:   DBG("TSTR    '"); dbgf(re, instr.str, 0); DBG("'"); break;
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
	case INSTR_NSPACE:     DBG("NSPACE");                            break;
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
	case INSTR_CATEGORY:   DBG("CATEGORY %u",  instr.c);             break;

	case INSTR_SCRIPT:
		DBG("SCRIPT   %u (%s)", instr.c, kdgu_getscriptname(instr.c));
		break;

	case INSTR_RANGE:
		DBG("RANGE    U+%04"PRIX32", U+%04"PRIX32, instr.a, instr.b);
		break;

	case INSTR_ALT: {
		DBG("ALT     '");

		for (unsigned i = 0; i < instr.num; i++) {
			kdgu_print(instr.list[i], stderr);
			if (i < instr.num - 1) DBG("|");
		}

		DBG("'");
	} break;

	default:
		DBG("\nunimplemented instruction printer %d\n", instr.op);
		assert(false);
	}
}

static void
print_instructions(ktre *re)
{
	char jmp[re->ip][50];
	unsigned size = 0;

	for (int i = 0; i < re->ip; i++)
		for (unsigned j = 0; j < 50; j++)
			jmp[i][j] = 0;

	/*
	 * Go through the instructions and fill out a branch marking
	 * the target of each jump instruction.
	 */
	for (int i = 0; i < re->ip; i++) {
		if (re->c[i].op != INSTR_JMP) continue;
		int depth = -1;

		for (unsigned j = 0; j < size && depth == -1; j++){
			bool ok = true;
			for (unsigned k = i; k <= re->c[i].c; k++)
				if (jmp[k][j] != 0) {
					ok = false;
					break;
				}

			if (ok) {
				depth = j;
				break;
			}
		}

		if (depth == -1) size++, depth = size - 1;

		for (unsigned k = i + 1; k < re->c[i].c; k++)
			jmp[k][depth] = 3;

		if (jmp[i][depth] == 6)
			jmp[i][depth] = 5;
		else
			jmp[i][depth] = jmp[i][depth] == 2 ? 7 : 1;
		jmp[re->c[i].c][depth] = 2;

		for (int k = 0; k < depth; k++) {
			if (jmp[i][k] == 2 || jmp[i][k] == 6)
				jmp[i][k] = 6;
			else if (jmp[i][k] == 3 || jmp[i][k] == 5)
				jmp[i][k] = 5;
			else
				jmp[i][k] = 4;
		}

		for (int k = 0; k < depth; k++) {
			if (jmp[re->c[i].c][k] == 2 || jmp[re->c[i].c][k] == 6)
				jmp[re->c[i].c][k] = 6;
			else if (jmp[re->c[i].c][k] == 3 || jmp[re->c[i].c][k] == 5)
				jmp[re->c[i].c][k] = 5;
			else
				jmp[re->c[i].c][k] = 4;
		}
	}

	for (int i = 0; i < re->ip; i++) {
		DBG("\n%3d. [%4d] ", i, re->c[i].loc);

		if (re->c[i].op == INSTR_SAVE && re->c[i].a % 2 == 0)
			DBG("<%2d> ", re->c[i].a / 2);
		else if (re->c[i].op == INSTR_SAVE && re->c[i].a % 2 != 0)
			DBG("</%d> ", (re->c[i].a - 1) / 2);
		else
			DBG("     ");

		for (int j = size - 1; j >= 0; j--) {
			switch (jmp[i][j]) {
			case 0: DBG(" "); break;
			case 1: DBG("┌"); break;
			case 2: DBG("└"); break;
			case 3: DBG("│"); break;
			case 4: DBG("─"); break;
			case 5: DBG("┼"); break;
			case 6: DBG("┴"); break;
			case 7: DBG("├"); break;
			}
		}

		print_instruction(re, re->c[i]);
	}

	DBG("\n");
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
		for (int i = 0; i < n->x; i++) {
			a = re->ip;
			if (n->a->type == NODE_GROUP && !re->group[n->a->gi].is_compiled) {
				compile(re, n->a, rev);
			} else if (n->a->type == NODE_GROUP) {
				emit_c(re, INSTR_CALL, re->group[n->a->gi].address + 1, n->loc);
			} else compile(re, n->a, rev);
		}

		if (n->y == -1) {
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

		for (int i = 0; i < n->y - n->x; i++) {
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

	case NODE_NOT:
		a = re->ip;
		GROUPY(NLA,false,NLA_FAIL);
		PATCH_C(a, re->ip);
		emit(re, INSTR_MANY, n->loc);
		break;

	case NODE_AND:
		emit(re, INSTR_PLA, n->loc);
		emit(re, INSTR_PLA, n->loc);
		compile(re, n->a, rev);
		emit(re, INSTR_PLA_WIN, n->loc);
		emit(re, INSTR_PLA, n->loc);
		compile(re, n->b, rev);
		emit(re, INSTR_PLA_WIN, n->loc);
		emit(re, INSTR_PLA_WIN, n->loc);
		emit(re, INSTR_MANY, n->loc);
		break;

	case NODE_ALT:  emit_alt(re, INSTR_ALT, n->list, n->num, n->loc); break;
	case NODE_STR:        emit_str(re, INSTR_STR,    n->str, n->loc); break;
	case NODE_CLASS:      emit_str(re, INSTR_CLASS,  n->str, n->loc); break;
	case NODE_NCLASS:     emit_str(re, INSTR_NCLASS, n->str, n->loc); break;
	case NODE_CATEGORY:   emit_c    (re, INSTR_CATEGORY, n->c,   n->loc); break;
	case NODE_SCRIPT:     emit_c    (re, INSTR_SCRIPT,   n->c,   n->loc); break;
	case NODE_SETOPT:     emit_c    (re, INSTR_SETOPT,   n->c,   n->loc); break;
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
	case NODE_NONE:                                                       break;
	case NODE_RANGE: emit_ab(re, INSTR_RANGE, n->x, n->y, n->loc); break;

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
	for (unsigned i = 0; i < (unsigned)re->loc; kdgu_next(re->s, &i))
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

	if (replaced) {
		DBG("\nreplace: `");
		kdgu_print(replaced, stderr);
		fputc('`', stderr);
	}

	DBG("\n");
}

static struct node *
optimize_node(const struct ktre *re, struct node *n)
{
	if (!n) return n;

	switch (n->type) {
	case NODE_SEQUENCE: case NODE_OR:
		n->a = optimize_node(re, n->a);
		n->b = optimize_node(re, n->b);
		break;
	case NODE_QUESTION: case NODE_REP:   case NODE_ASTERISK:
	case NODE_PLUS:     case NODE_GROUP: case NODE_ATOM:
	case NODE_PLA:      case NODE_NLA:   case NODE_PLB:
	case NODE_NLB:      case NODE_NOT:
		n->a = optimize_node(re, n->a);
		break;
	default:;
	}

	if (n->type == NODE_RANGE && n->y - n->x < 0x5C) {
		struct node *tmp = new_node(re);
		if (!tmp) return n;
		tmp->type = NODE_CLASS;
		tmp->str = kdgu_new(re->s->fmt, NULL, 0);
		for (int32_t i = n->x; i <= n->y; i++)
			kdgu_chrappend(tmp->str, i);
		free_node(n);
		return tmp;
	}

	if (n->type == NODE_STR && kdgu_len(n->str) == 1)
		n->type = NODE_CLASS;

	if (n->type == NODE_OR
	    && n->a->type == NODE_CLASS
	    && n->b->type == NODE_CLASS) {
		struct node *tmp = new_node(re);
		if (!tmp) return n;
		tmp->type = NODE_CLASS;
		tmp->str = kdgu_copy(n->a->str);
		kdgu_setappend(tmp->str, n->b->str);
		free_node(n);
		return tmp;
	}

	if (n->type == NODE_OR
	    && n->b->type == NODE_CLASS
	    && n->a->type == NODE_OR
	    && n->a->b->type == NODE_CLASS) {
		struct node *tmp = n->a;
		kdgu_setappend(tmp->b->str, n->b->str);
		return tmp;
	}

	if (n->type == NODE_NOT
	    && n->a->type == NODE_CLASS) {
		n->a->type = NODE_NCLASS;
		struct node *tmp = copy_node(re, n->a);
		return free_node(n), tmp;
	}

	if (n->type == NODE_AND
	    && n->a->type == NODE_CLASS
	    && n->b->type == NODE_CLASS) {
		struct node *tmp = new_node(re);
		if (!tmp) return n;
		tmp->type = NODE_CLASS;
		tmp->str = kdgu_new(re->s->fmt, NULL, 0);

		for (unsigned i = 0;
		     i < n->b->str->len;
		     kdgu_inc(n->b->str, &i)) {
			uint32_t c = kdgu_decode(n->b->str, i);
			if (kdgu_contains(n->a->str, c))
				kdgu_chrappend(tmp->str, c);
		}

		free_node(n);
		return tmp;
	}

	if (n->type == NODE_OR
	    && (n->a->type == NODE_STR || (n->a->type == NODE_CLASS && kdgu_len(n->a->str) == 1))
	    && (n->b->type == NODE_STR || (n->b->type == NODE_CLASS && kdgu_len(n->b->str) == 1))) {
		struct node *tmp = new_node(re);
		if (!tmp) return n;

		tmp->type = NODE_ALT;
		tmp->list = malloc(2 * sizeof *tmp->list);
		tmp->list[0] = kdgu_copy(n->a->str);
		tmp->list[1] = kdgu_copy(n->b->str);
		tmp->num = 2;

		free_node(n);
		return tmp;
	}

	if (n->type == NODE_OR
	    && (n->a->type == NODE_STR || (n->a->type == NODE_CLASS && kdgu_len(n->a->str) == 1))
	    && n->b->type == NODE_ALT) {
		struct node *tmp = copy_node(re, n->b);
		if (!tmp) return n;

		tmp->list = realloc(tmp->list, ++tmp->num * sizeof *tmp->list);
		tmp->list[tmp->num - 1] = kdgu_copy(n->a->str);

		free_node(n);
		return tmp;
	}

	if (n->type == NODE_OR
	    && n->a->type == NODE_STR
	    && n->b->type == NODE_OR
	    && n->b->a->type == NODE_STR) {
		struct node *tmp = new_node(re);
		if (!tmp) return n;

		tmp->type = NODE_OR;
		tmp->b = copy_node(re, n->b->b);
		tmp->a = new_node(re);
		tmp->a->type = NODE_ALT;

		tmp->a->list = malloc(2 * sizeof *tmp->list);
		tmp->a->list[0] = kdgu_copy(n->a->str);
		tmp->a->list[1] = kdgu_copy(n->b->a->str);
		tmp->a->num = 2;

		free_node(n);

		return tmp;
	}

	return n;
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
		for (size_t i = 0; i < CHAR_BIT * sizeof opt; i++) {
			switch (opt & (1 << i)) {
			case KTRE_INSENSITIVE: DBG("\n\tINSENSITIVE"); break;
			case KTRE_UNANCHORED : DBG("\n\tUNANCHORED");  break;
			case KTRE_EXTENDED   : DBG("\n\tEXTENDED");    break;
			case KTRE_GLOBAL     : DBG("\n\tGLOBAL");      break;
			case KTRE_MULTILINE  : DBG("\n\tMULTILINE");   break;
			case KTRE_CONTINUE   : DBG("\n\tCONTINUE");    break;
			case KTRE_DEBUG      : DBG("\n\tDEBUG");       break;
			case KTRE_ECMA       : DBG("\n\tECMA");        break;
			}
		}
		DBG("\n");
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
		free_node(re->n);
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

	if ((re->opt & KTRE_DUMB) == 0) {
		print_node(re, re->n);
		DBG("\noptimal:\n");
		re->n = optimize_node(re, re->n);
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

	if ((re->opt & KTRE_DUMB) == 0) {
		for (int i = 0; i < re->ip; i++) {
			if (re->c[i].op != INSTR_JMP) continue;
			int k = re->c[i].c;
			while (re->c[k].op == INSTR_JMP) k = re->c[k].c;
			re->c[i].c = k;
		}
	}

	if (opt & KTRE_DEBUG) print_instructions(re);

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
		if (kdgu_contains(re->c[ip].str, c))
			rev ? PREV : NEXT;
		else if (opt & KTRE_INSENSITIVE
			 && kdgu_contains(re->c[ip].str, lc(c)))
			rev ? PREV : NEXT;
		else FAIL;
		break;
	case INSTR_NCLASS:
		THREAD[TP].ip++;
		if (!kdgu_contains(re->c[ip].str, c))
			rev ? PREV : NEXT;
		else if (opt & KTRE_INSENSITIVE
			 && !kdgu_contains(re->c[ip].str, lc(c)))
			rev ? PREV : NEXT;
		else FAIL;
		break;
	case INSTR_STR: case INSTR_TSTR: {
		THREAD[TP].ip++;
		kdgu *str = re->c[ip].str;
		unsigned len = kdgu_len(str);
		if (kdgu_ncmp(subject,
			      re->c[ip].str,
			      sp,
			      rev ? len - 1 : 0,
			      rev ? -len : len,
			      opt & KTRE_INSENSITIVE))
			kdgu_move(subject, &THREAD[TP].sp, rev ? -(int)len : (int)len);
		else FAIL;
	} break;
	case INSTR_ALT:
		THREAD[TP].ip++;

		for (unsigned i = 0; i < re->c[ip].num; i++) {
			kdgu *str = re->c[ip].list[i];
			unsigned len = kdgu_len(str);

			if (kdgu_ncmp(subject,
				      str,
				      sp,
				      rev ? len - 1 : 0,
				      rev ? -len : len,
				      opt & KTRE_INSENSITIVE)) {
				kdgu_move(subject, &THREAD[TP].sp, rev ? -(int)len : (int)len);
				return true;
			}
		}

		FAIL;
		break;
	case INSTR_NOT:
		THREAD[TP].ip++;
		if (kdgu_contains(re->c[ip].str, c)) FAIL;
		kdgu_next(subject, &THREAD[TP].sp);
		break;
	case INSTR_BOL: {
		unsigned idx = sp;
		kdgu_dec(subject, &idx);
		if (!(sp > 0 && kdgu_chrcmp(subject, idx, '\n')) && sp != 0) FAIL;
		THREAD[TP].ip++;
	} break;
	case INSTR_EOL:
		if (!kdgu_chrcmp(subject, sp, '\n') && sp != (int)subject->len)
			FAIL;
		if (kdgu_chrcmp(subject, sp, '\n'))
			rev ? PREV : NEXT;
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
		if (sp == 0 && is_word(re, c))
			return true;
		if (is_word(re, c) != is_word(re, kdgu_decode(subject, sp - 1)))
			return true;
		FAIL;
		break;
	case INSTR_NWB:
		THREAD[TP].ip++;
		if (sp == 0 && !is_word(re, c)) return true;
		if (is_word(re, c) == is_word(re, kdgu_decode(subject, sp - 1)))
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
		rev ? kdgu_dec(subject, &THREAD[TP].sp) || --THREAD[TP].sp
		    : kdgu_inc(subject, &THREAD[TP].sp) || ++THREAD[TP].sp;
		if (codepoint(c)->category & re->c[ip].c) return true;
		FAIL;
		break;
	case INSTR_SCRIPT:
		THREAD[TP].ip++;
		rev ? kdgu_dec(subject, &THREAD[TP].sp) || --THREAD[TP].sp
		    : kdgu_inc(subject, &THREAD[TP].sp) || ++THREAD[TP].sp;
		if (codepoint(c)->script == re->c[ip].c) return true;
		FAIL;
		break;
	case INSTR_RANGE:
		THREAD[TP].ip++;
		rev ? kdgu_dec(subject, &THREAD[TP].sp) || --THREAD[TP].sp
		    : kdgu_inc(subject, &THREAD[TP].sp) || ++THREAD[TP].sp;
		if (c >= (uint32_t)re->c[ip].a
		    && c <= (uint32_t)re->c[ip].b)
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
	free_node(re->n);
	if (re->err) free(re->err_str);

	if (re->c) {
		for (int i = 0; i < re->ip; i++)
			if (re->c[i].op == INSTR_TSTR)
				kdgu_free(re->c[i].str);

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
		DBG("subject: ");
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
	if (vec) *vec = ktre_getvec(re);
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
		kdgu *chr = kdgu_getchr(src, i);

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
	int **vec = NULL;
	DBG("\nsubject: "), dbgf(re, subject, 0);
	*len = 0;

	kdgu **r = NULL;
	unsigned j = 0;

	if (!run(re, subject, &vec) || re->err) {
		print_finish(re, subject, re->s, false, vec, NULL);
		*len = 1;
		r = malloc(sizeof *r);
		*r = kdgu_copy(subject);
		return r;
	}

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

kdgu *
ktre_getgroup(int **const vec, int match, int group, const kdgu *subject)
{
	return kdgu_substr(subject, vec[match][group * 2], vec[match][group * 2] + vec[match][group * 2 + 1]);
}
