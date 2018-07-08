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

static void print_node(struct ktre *re, struct node *n);
#define DBG(...) ((re->opt & KTRE_DEBUG) ? fprintf(stderr, __VA_ARGS__) : 0)
static void
dbgf(const char *str)
{
	for (int i = 0; i < (int)strlen(str); i++) {
		if ((!strchr(WHITESPACE, str[i]) && str[i] != '\\')
		    || str[i] == ' ') {
			fputc(str[i], stderr);
			continue;
		}

		fputc('\\', stderr);
		switch (str[i]) {
		case '\t': fputc('t', stderr); break;
		case '\r': fputc('r', stderr); break;
		case '\n': fputc('n', stderr); break;
		case '\v': fputc('v', stderr); break;
		case '\f': fputc('f', stderr); break;
		case '\\': fputc('\\',stderr); break;
		default: fputc(str[i], stderr);
		}
	}
}

static void error(struct ktre *re, enum ktre_error err, int loc, char *fmt, ...);

static int
add_group(struct ktre *re)
{
	if (re->gp >= KTRE_MAX_GROUPS) {
		error(re,
		      KTRE_ERROR_TOO_MANY_GROUPS,
		      re->sp - re->pat,
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
		INSTR_CHAR,
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
		INSTR_RET
	} op;

	union {
		struct {
			int a, b;
		};
		int c;
		char *class;
	};

	unsigned len;
	int loc;
};

static void
grow_code(struct ktre *re, int n)
{
	if (!re->info.instr_alloc) {
		re->info.instr_alloc = 25;
		re->c = malloc(re->info.instr_alloc * sizeof *re->c);
	}
	if (!re->c) return;
	if (re->ip + n >= re->info.instr_alloc) {
		if (re->ip + n >= re->info.instr_alloc * 2) {
			re->info.instr_alloc = re->ip + n;
		} else {
			re->info.instr_alloc *= 2;
		}

		re->c = realloc(re->c, re->info.instr_alloc * sizeof *re->c);
	}
}

static void
emit_ab(struct ktre *re, int instr, int a, int b, int loc)
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
emit_c(struct ktre *re, int instr, int c, int loc)
{
	grow_code(re, 1);
	if (!re->c) return;

	re->c[re->ip].op = instr;
	re->c[re->ip].c = c;
	re->c[re->ip].loc = loc;

	re->ip++;
}

static void
emit_class(struct ktre *re, int instr, char *class, int loc)
{
	grow_code(re, 1);
	if (!re->c) return;

	re->c[re->ip].op = instr;
	re->c[re->ip].class = class;
	re->c[re->ip].loc = loc;
	re->c[re->ip].len = strlen(class);

	re->ip++;
}

static void
emit(struct ktre *re, int instr, int loc)
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
		NODE_CHAR,
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
		NODE_WORD
	} type;

	struct node *a, *b;
	int gi; /* group index */
	int c, d;
	char *class;

	int loc;
};

static void
free_node(struct ktre *re, struct node *n)
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
		free(n->class);
		break;
	default: break;
	}

	free(n);
}

static void
next_char(struct ktre *re)
{
	if (*re->sp) re->sp++;
}

static void
error(struct ktre *re, enum ktre_error err, int loc, char *fmt, ...)
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

static struct node *parse(struct ktre *re);
static struct node *term(struct ktre *re);

static inline char
lc(char c)
{
	return c >= 'A' && c <= 'Z' ? (c - 'A') + 'a' : c;
}

static inline char
uc(char c)
{
	return c >= 'a' && c <= 'z' ? (c - 'a') + 'A' : c;
}

static void
append_char(char **class, char c)
{
	size_t len = *class ? strlen(*class) : 0;
	*class = realloc(*class, len + 2);
	if (!*class) return;
	(*class)[len] = c;
	(*class)[len + 1] = 0;
}

static void
append_str(char **class, const char *c)
{
	size_t len = *class ? strlen(*class) : 0;
	*class = realloc(*class, len + strlen(c) + 1);
	if (!*class) return;
	strcpy(*class + len, c);
}

static char *
strclone(struct ktre *re, const char *str)
{
	char *ret = malloc(strlen(str) + 1);
	if (!ret) return NULL;
	re->info.parser_alloc += strlen(str) + 1;
	strcpy(ret, str);
	return ret;
}

static struct node *
new_node(struct ktre *re)
{
	struct node *n = malloc(sizeof *n);
	if (!n) return NULL;
	re->info.parser_alloc += sizeof *n;
	memset(n, 0, sizeof *n);
	n->loc = re->sp - re->pat;
	return n;
}

static int
dec_num(const char **s)
{
	int n = -1;

	while (**s && isdigit(**s)) {
		if (n < 0) n = 0;
		n = n * 10 + (**s - '0');
		(*s)++;
	}

	return n;
}

static int
hex_num(const char **s)
{
	int n = -1;

	while (**s
	       && ((lc(**s) >= 'a' && lc(**s) <= 'f')
	           || (**s  >= '0' &&    **s <= '9'))) {
		if (n < 0) n = 0;
		char c = lc(**s);
		n *= 16;
		n += (c >= 'a' && c <= 'f') ? c - 'a' + 10 : c - '0';
		(*s)++;
	}

	return n;
}

static int
oct_num(const char **s)
{
	int n = -1;

	while (**s >= '0' && **s <= '7') {
		if (n < 0) n = 0;
		n *= 8;
		n += **s - '0';
		(*s)++;
	}

	return n;
}

static int
parse_dec_num(struct ktre *re)
{
	int n = dec_num(&re->sp);
	if (n < 0 && !re->err)
		error(re, KTRE_ERROR_SYNTAX_ERROR,
		      re->sp - re->pat, "expected a number");
	return n;
}

static int
parse_hex_num(struct ktre *re)
{
	int n = hex_num(&re->sp);
	if (n < 0 && !re->err)
		error(re, KTRE_ERROR_SYNTAX_ERROR,
		      re->sp - re->pat, "expected a number");
	return n;
}

static int
parse_oct_num(struct ktre *re)
{
	int n = oct_num(&re->sp);
	if (n < 0 && !re->err)
		error(re, KTRE_ERROR_SYNTAX_ERROR,
		      re->sp - re->pat, "expected a number");
	return n;
}

static struct node *
parse_mode_modifiers(struct ktre *re)
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

	while (*re->sp && *re->sp != ')' && *re->sp != ':') {
		bool off = false;
		int  bit = 0;

		switch (*re->sp) {
		case 'c': off = true;
		case 'i': bit = KTRE_INSENSITIVE; break;
		case 't': off = true;
		case 'x': bit = KTRE_EXTENDED;    break;
		case 'm': bit = KTRE_MULTILINE;   break;

		case '-':
			neg = true;
			next_char(re);
			continue;

		default:
			error(re, KTRE_ERROR_SYNTAX_ERROR,
			      re->sp - re->pat,
			      "invalid mode modifier");

			free_node(re, left);
			return NULL;
		}

		if (off || neg) opt &= ~bit;
		else            opt |=  bit;

		re->sp++;
	}

	re->popt = opt;
	left->c = opt;

	if (*re->sp == ':') {
		next_char(re);

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
parse_branch_reset(struct ktre *re)
{
	struct node *left = NULL;

	int bottom = re->gp;
	int top = -1;

	do {
		if (*re->sp == '|') re->sp++;
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
	} while (*re->sp == '|');

	re->gp = top;

	if (*re->sp != ')') {
		error(re, KTRE_ERROR_SYNTAX_ERROR,
		      re->sp - re->pat,
		      "expected ')'");

		free_node(re, left);
		return NULL;
	}

	return left;
}

static struct node *
parse_named_group(struct ktre *re)
{
	struct node *left = new_node(re);

	if (strchr(WORD, *re->sp)) {
		const char *a = re->sp;
		while (*re->sp && strchr(WORD, *re->sp))
			re->sp++;
		const char *b = re->sp;

		if (*re->sp != '>') {
			error(re, KTRE_ERROR_SYNTAX_ERROR,
			      re->sp - re->pat,
			      "expected '>'");

			free_node(re, left);
			return NULL;
		}

		next_char(re);
		left->type = NODE_GROUP;
		left->gi = add_group(re);

		if (left->gi < 0) {
			free_node(re, left);
			return NULL;
		}

		re->group[left->gi].is_called = false;
		re->group[left->gi].name = malloc(b - a + 1);
		strncpy(re->group[left->gi].name, a, b - a);
		re->group[left->gi].name[b - a] = 0;

		left->a = parse(re);
		return left;
	}

	if      (*re->sp == '=') left->type = NODE_PLB;
	else if (*re->sp == '!') left->type = NODE_NLB;
	else {
		error(re, KTRE_ERROR_SYNTAX_ERROR,
		      left->loc, "invalid group syntax");
		free_node(re, left);
		return NULL;
	}

	next_char(re);
	left->a = parse(re);

	return left;
}

/* Parses a backreference to a named group. */
static struct node *
parse_named_backreference(struct ktre *re)
{
	struct node *left = new_node(re);

	if (*re->sp == '=') {
		next_char(re);

		const char *a = re->sp;
		while (*re->sp && strchr(WORD, *re->sp))
			re->sp++;
		const char *b = re->sp;

		if (*re->sp != ')') {
			error(re, KTRE_ERROR_SYNTAX_ERROR,
			      re->sp - re->pat, "expected ')'");
			free_node(re, left);
			return NULL;
		}

		left->type = NODE_BACKREF;
		left->c = -1;

		for (int i = 0; i < re->gp; i++) {
			if (!re->group[i].name) continue;
			if (!strncmp(re->group[i].name, a, b - a)
			    && b - a == (int)strlen(re->group[i].name)) {
				left->c = i;
				break;
			}
		}

		if (left->c < 0) {
			error(re, KTRE_ERROR_SYNTAX_ERROR,
			      re->sp - re->pat,
			      "name references a group that does not exist");

			free_node(re, left);
			return NULL;
		}

		return left;
	}

	if (*re->sp != '<') {
		error(re, KTRE_ERROR_SYNTAX_ERROR,
		      re->sp - re->pat, "expected '<'");
		free_node(re, left);
		return NULL;
	}

	next_char(re);
	const char *a = re->sp;
	while (*re->sp && strchr(WORD, *re->sp)) re->sp++;
	const char *b = re->sp;

	if (*re->sp != '>') {
		error(re, KTRE_ERROR_SYNTAX_ERROR,
		      re->sp - re->pat, "expected '>'");
		free_node(re, left);
		return NULL;
	}

	next_char(re);
	left->type = NODE_GROUP;
	left->gi = add_group(re);

	if (left->gi < 0) {
		free_node(re, left);
		return NULL;
	}

	re->group[left->gi].is_called = false;
	re->group[left->gi].name = malloc(b - a + 1);
	strncpy(re->group[left->gi].name, a, b - a);
	re->group[left->gi].name[b - a] = 0;

	left->a = parse(re);

	return left;
}

static struct node *
parse_special_group(struct ktre *re)
{
	struct node *left = NULL;
	re->sp++;

	switch (re->sp[-1]) {
	case ':': left = parse(re); break;
	case '|': left = parse_branch_reset(re); break;
	case 'P': return parse_named_backreference(re);
	case '<': return parse_named_group(re);

	case '#':
		left = new_node(re);
		left->type = NODE_NONE;
		while (*re->sp && *re->sp != ')') re->sp++;
		break;

	case '\'': {
		left = new_node(re);
		const char *a = re->sp;
		while (*re->sp && strchr(WORD, *re->sp))
			re->sp++;
		const char *b = re->sp;

		if (*re->sp != '\'') {
			error(re, KTRE_ERROR_SYNTAX_ERROR,
			      re->sp - re->pat, "expected '\''");
			free_node(re, left);
			return NULL;
		}

		next_char(re);
		left->type = NODE_GROUP;
		left->gi = add_group(re);

		if (left->gi < 0) {
			free_node(re, left);
			return NULL;
		}

		re->group[left->gi].is_called = false;
		re->group[left->gi].name = malloc(b - a + 1);
		strncpy(re->group[left->gi].name, a, b - a);
		re->group[left->gi].name[b - a] = 0;

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
		re->sp--;
		left->c = parse_dec_num(re);

		if (left->c >= 0 && left->c < re->gp)
			re->group[left->c].is_called = true;
		break;

	default:
		free_node(re, left);
		re->sp--;
		left = parse_mode_modifiers(re);
		break;
	}

	return left;
}

static struct node *
parse_group(struct ktre *re)
{
	struct node *left = new_node(re);
	next_char(re);

	if (!strncmp(re->sp, "?R", 2)) {
		left->type = NODE_RECURSE;
		re->sp += 2;

		re->group[0].is_called = true;
	} else if (*re->sp == '?') {
		next_char(re);

		free_node(re, left);
		left = parse_special_group(re);
	} else {
		left->type = NODE_GROUP;
		left->gi = add_group(re);

		if (left->gi < 0) {
			free_node(re, left);
			return NULL;
		}

		re->group[left->gi].is_called = false;
		left->a = parse(re);
	}

	if (*re->sp != ')' && !re->err) {
		error(re, KTRE_ERROR_SYNTAX_ERROR, left->loc,
		      "unmatched '('");
		free_node(re, left);

		return NULL;
	}

	next_char(re);
	return left;
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
	{ "[:cntrl:]", "\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b"
	  "\x0c\x0d\x0e\x0f\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19"
	  "\x1a\x1b\x1c\x1d\x1e\x1f\x7F"},
	{ "[:graph:]", "\x21\x22\x23\x24\x25\x26\x27\x28\x29\x2a\x2b"
	  "\x2c\x2d\x2e\x2f\x30\x31\x32\x33\x34\x35\x36\x37\x38\x39\x3a"
	  "\x3b\x3c\x3d\x3e\x3f\x40\x41\x42\x43\x44\x45\x46\x47\x48\x49"
	  "\x4a\x4b\x4c\x4d\x4e\x4f\x50\x51\x52\x53\x54\x55\x56\x57\x58"
	  "\x59\x5a\x5b\x5c\x5d\x5e\x5f\x60\x61\x62\x63\x64\x65\x66\x67"
	  "\x68\x69\x6a\x6b\x6c\x6d\x6e\x6f\x70\x71\x72\x73\x74\x75\x76"
	  "\x77\x78\x79\x7a\x7b\x7c\x7d\x7e"},
	{ "[:print:]", "\x20\x21\x22\x23\x24\x25\x26\x27\x28\x29\x2a\x2b"
	  "\x2c\x2d\x2e\x2f\x30\x31\x32\x33\x34\x35\x36\x37\x38\x39\x3a"
	  "\x3b\x3c\x3d\x3e\x3f\x40\x41\x42\x43\x44\x45\x46\x47\x48\x49"
	  "\x4a\x4b\x4c\x4d\x4e\x4f\x50\x51\x52\x53\x54\x55\x56\x57\x58"
	  "\x59\x5a\x5b\x5c\x5d\x5e\x5f\x60\x61\x62\x63\x64\x65\x66\x67"
	  "\x68\x69\x6a\x6b\x6c\x6d\x6e\x6f\x70\x71\x72\x73\x74\x75\x76"
	  "\x77\x78\x79\x7a\x7b\x7c\x7d\x7e"}
};

static char *
parse_character_class_character(struct ktre *re)
{
	char *a = NULL;

	if (*re->sp == '[') {
		for (size_t i = 0;
		     i < sizeof pclasses / sizeof *pclasses;
		     i++) {
			if (!strncmp(re->sp, pclasses[i].name, strlen(pclasses[i].name))) {
				append_str(&a, pclasses[i].class);
				re->sp += strlen(pclasses[i].name);
				return a;
			}
		}

		append_char(&a, *re->sp);
		next_char(re);
		return a;
	} else if (*re->sp != '\\') {
		append_char(&a, *re->sp);
		next_char(re);
		return a;
	}

	/* Skip over the `\`. */
	next_char(re);
	int i = 0;

	switch (*re->sp) {
	case 's': append_str(&a, WHITESPACE); break;
	case 'w': append_str(&a, WORD);  break;
	case 'a': append_char(&a, '\a'); break;
	case 'b': append_char(&a, '\b'); break;
	case 'f': append_char(&a, '\f'); break;
	case 'n': append_char(&a, '\n'); break;
	case 't': append_char(&a, '\t'); break;
	case 'r': append_char(&a, '\r'); break;
	case 'd': append_str(&a, DIGIT); break;
	case 'h': append_str(&a, " \t"); break;
	case 'e': append_char(&a, 7); break;

	case '0':
		next_char(re);
		append_char(&a, parse_oct_num(re));
		re->sp--;
		break;

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

	case 'x': {
		next_char(re);

		int loc = re->sp - re->pat;
		bool bracketed = *re->sp == '{';

		if (!bracketed) {
			append_char(&a, parse_hex_num(re));
			re->sp--;
			break;
		}

		next_char(re);
		append_char(&a, parse_hex_num(re));

		if (*re->sp != '}' && !re->err) {
			free(a);

			error(re, KTRE_ERROR_SYNTAX_ERROR,
			      loc, "incomplete token");

			return NULL;
		}
	} break;

	case 'o':
		next_char(re);
		int loc = re->sp - re->pat;

		if (*re->sp != '{' && !re->err) {
			error(re, KTRE_ERROR_SYNTAX_ERROR,
			      loc, "incomplete token");

			return NULL;
		}

		next_char(re);
		append_char(&a, parse_oct_num(re));

		if (*re->sp != '}' && !re->err) {
			error(re, KTRE_ERROR_SYNTAX_ERROR,
			      loc, "unmatched '{'");

			return NULL;
		}
		break;

	default:
		append_char(&a, *re->sp);
		break;
	}

	next_char(re);

	return a;
}

static struct node *
parse_character_class(struct ktre *re)
{
	struct node *left = new_node(re);
	char *class       = NULL;

	if (*re->sp == '^') {
		left->type = NODE_NOT;
		next_char(re);
	} else {
		left->type = NODE_CLASS;
	}

	while (*re->sp && *re->sp != ']') {
		char *a    = NULL;
		char *b    = NULL;
		bool range = false;

		a = parse_character_class_character(re);

		if (!a) {
			free_node(re, left);
			return NULL;
		}

		range = (*re->sp
		         && *re->sp == '-'
		         && re->sp[1] != ']'
		         && strlen(a) == 1);

		if (range) {
			next_char(re);
			b = parse_character_class_character(re);

			if (!b) {
				free(a);
				free_node(re, left);
				return NULL;
			}

			if (strlen(b) != 1) {
				append_str(&class, a);
				append_str(&class, b);
				free(a);
				free(b);
				continue;
			}

			for (int i = *a; i <= *b; i++)
				append_char(&class, i);

			free(b);
		} else {
			append_str(&class, a);
		}

		free(a);
	}

	if (*re->sp != ']') {
		error(re, KTRE_ERROR_SYNTAX_ERROR, left->loc,
		      "unterminated character class");
		free(class);
		free_node(re, left);
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
	next_char(re);
	return left;
}

static struct node *
parse_g(struct ktre *re)
{
	struct node *left = new_node(re);
	next_char(re);

	bool bracketed = *re->sp == '{';
	bool neg = false;
	bool pos = false;
	int n;

	if (bracketed) {
		next_char(re);

		if (*re->sp == '+') pos = true;
		if (*re->sp == '-') neg = true;

		if (pos || neg) next_char(re);
		n = parse_dec_num(re);

		if (*re->sp != '}' && !re->err) {
			free_node(re, left);
			error(re, KTRE_ERROR_SYNTAX_ERROR,
			      left->loc, "incomplete token");

			return NULL;
		}
	} else {
		if (*re->sp == '+') pos = true;
		if (*re->sp == '-') neg = true;

		if (pos || neg) next_char(re);
		n = parse_dec_num(re);

		re->sp--;
	}

	if (pos) n = re->gp + n;
	if (neg) n = re->gp - n;

	left->type = NODE_BACKREF;
	left->c = n;

	return left;
}

static struct node *
parse_k(struct ktre *re)
{
	struct node *left = new_node(re);

	bool bracketed = *re->sp == '<';
	next_char(re);

	const char *a = re->sp;
	while (*re->sp && strchr(WORD, *re->sp))
		re->sp++;
	const char *b = re->sp;

	if ((bracketed && *re->sp != '>')
	    || (!bracketed && *re->sp != '\'')
	    || a == b) {
		error(re, KTRE_ERROR_SYNTAX_ERROR,
		      re->sp - re->pat, "expected '>' or '");
		free_node(re, left);
		return NULL;
	}

	left->type = NODE_BACKREF;
	left->c = -1;

	for (int i = 0; i < re->gp; i++) {
		if (!re->group[i].name) continue;
		if (!strncmp(re->group[i].name, a, b - a)
		    && b - a == (int)strlen(re->group[i].name)) {
			left->c = i;
			break;
		}
	}

	if (left->c < 0) {
		error(re, KTRE_ERROR_SYNTAX_ERROR,
		      re->sp - re->pat, "name references a group that does not exist");
		free_node(re, left);
		return NULL;
	}

	return left;
}

static struct node *
parse_primary(struct ktre *re)
{
	struct node *left = new_node(re);
	if (!left) return NULL;

	int loc = re->sp - re->pat;

	if (*re->sp == ')') {
		free_node(re, left);
		return NULL;
	}

 again:
	if (re->literal) {
		if (*re->sp == '\\' && re->sp[1] == 'E') {
			re->literal = false;
			re->sp += 2;
			goto again;
		}

		left->type = NODE_CHAR;
		left->c = *re->sp;
		next_char(re);
		return left;
	}

	if (*re->sp != '\\') {
		switch (*re->sp) {
		case '[':
			free_node(re, left);
			next_char(re);
			left = parse_character_class(re);
			break;

		case '(':
			free_node(re, left);
			left = parse_group(re);
			break;

		case '.':
			next_char(re);
			left->type = NODE_ANY;
			break;

		case '^':
			next_char(re);
			left->type = NODE_BOL;
			break;

		case '$':
			next_char(re);
			left->type = NODE_EOL;
			break;

		case '#':
			if (re->popt & KTRE_EXTENDED) {
				while (*re->sp && *re->sp != '\n')
					next_char(re);
			} else {
				left->type = NODE_CHAR;
				left->c = *re->sp;
				next_char(re);
			}
			break;

		default:
			if (re->popt & KTRE_EXTENDED
			    && strchr(WHITESPACE, *re->sp)) {
				while (*re->sp
				       && strchr(WHITESPACE, *re->sp))
					next_char(re);

				if (*re->sp) goto again;
			} else {
				left->type = NODE_CHAR;
				left->c = *re->sp;
				next_char(re);
			}
		}

		if (left) left->loc = loc;
		return left;
	}

	left->type = NODE_CLASS;
	char *a = NULL;
	next_char(re);

	switch (*re->sp) {
	case 'a': append_char(&a, '\a');	break;
	case 'f': append_char(&a, '\f');	break;
	case 'n': append_char(&a, '\n');	break;
	case 't': append_char(&a, '\t');	break;
	case 'r': append_char(&a, '\r');	break;
	case 'h': append_str(&a, " \t");	break;
	case 'e': append_char(&a, 7);		break;
	case 's': left->type = NODE_SPACE;	break;
	case 'K': left->type = NODE_SET_START;	break;
	case 'b': left->type = NODE_WB;		break;
	case 'B': left->type = NODE_NWB;	break;
	case 'A': left->type = NODE_BOS;	break;
	case 'Z': left->type = NODE_EOS;	break;
	case 'd': left->type = NODE_DIGIT;	break;
	case 'w': left->type = NODE_WORD;	break;

	case 'x': {
		next_char(re);

		loc = re->sp - re->pat;
		bool bracketed = *re->sp == '{';

		if (bracketed) {
			next_char(re);
			append_char(&a, parse_hex_num(re));

			if (*re->sp != '}' && !re->err) {
				free(a);
				error(re, KTRE_ERROR_SYNTAX_ERROR,
				      loc, "incomplete token");
				return NULL;
			}
		} else {
			append_char(&a, parse_hex_num(re));
			re->sp--;
		}
	} break;

	case '-': /* backreferences */
	case '+': case '1': case '2': case '3': case '4':
	case '5': case '6': case '7': case '8': case '9': {
		bool neg = (*re->sp == '-');
		bool pos = (*re->sp == '+');
		if (neg || pos) next_char(re);

		int n = parse_dec_num(re);

		if (neg) n = re->gp - n;
		if (pos) n = re->gp + n;

		left->type = NODE_BACKREF;
		left->c = n;
		re->sp--;
	} break;

	case 'o':
		next_char(re);
		loc = re->sp - re->pat;

		if (*re->sp != '{' && !re->err) {
			free_node(re, left);
			error(re, KTRE_ERROR_SYNTAX_ERROR,
			      loc, "expected '{'");

			return NULL;
		}

		next_char(re);
		append_char(&a, parse_oct_num(re));

		if (*re->sp != '}' && !re->err) {
			free_node(re, left);
			error(re, KTRE_ERROR_SYNTAX_ERROR,
			      loc, "unmatched '{'");

			return NULL;
		}
		break;

	case 'S':
		left->type = NODE_NOT;
		left->class = strclone(re, WHITESPACE);
		break;

	case 'D':
		left->type = NODE_NOT;
		left->class = strclone(re, DIGIT);
		break;

	case 'W':
		left->type = NODE_NOT;
		left->class = strclone(re, WORD);
		break;

	case 'Q':
		re->literal = true;
		next_char(re);
		goto again;

	case 'E':
		re->literal = false;
		next_char(re);
		goto again;

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

	case 'g':
		free_node(re, left);
		next_char(re);
		left = parse_g(re);
		break;

	case 'k':
		free_node(re, left);
		next_char(re);
		left = parse_k(re);
		break;

	default:
		append_char(&a, *re->sp);
		break;
	}

	next_char(re);
	if (left && !left->class) left->class = a;
	if (left) left->loc = loc;
	return left;
}

static struct node *
factor(struct ktre *re)
{
	if (!*re->sp) return NULL;

	struct node *left = parse_primary(re);

	while (*re->sp
	       && (*re->sp == '*'
		   || *re->sp == '+'
		   || *re->sp == '?'
		   || *re->sp == '{')) {
		struct node *n = new_node(re);

		switch (*re->sp) {
		case '*': n->type = NODE_ASTERISK; next_char(re); break;
		case '?': n->type = NODE_QUESTION; next_char(re); break;
		case '+': n->type = NODE_PLUS;     next_char(re); break;
		case '{': { /* counted repetition */
			n->type = NODE_REP; n->c = -1; n->d = 0;

			next_char(re);
			n->c = parse_dec_num(re);

			if (*re->sp == ',') {
				next_char(re);
				n->d = isdigit(*re->sp) ? parse_dec_num(re) : -1;
			}

			if (*re->sp != '}') {
				error(re, KTRE_ERROR_SYNTAX_ERROR,
				      re->sp - re->pat - 1,
				      "unmatched '{'");
				free_node(re, n);
				free_node(re, left);
				return NULL;
			}

			next_char(re);
		} break;
		}

		n->a = left;
		left = n;
	}

	if (left) left->loc = re->sp - re->pat - 1;
	return left;
}

static struct node *
term(struct ktre *re)
{
	struct node *left = new_node(re);
	left->type = NODE_NONE;

	while (*re->sp && *re->sp != '|' && *re->sp != ')') {
		struct node *right = factor(re);

		if (re->err) {
			free_node(re, left);
			free_node(re, right);
			return NULL;
		}

		if (left->type == NODE_NONE) {
			free(left);
			left = right;
			continue;
		}

		if ((left->type == NODE_CHAR
		     || left->type == NODE_STR)
		    && right->type == NODE_CHAR) {
			if (left->type == NODE_CHAR) {
				char a = left->c;
				left->type = NODE_STR;
				left->class = malloc(3);
				re->info.parser_alloc += 3;

				if (re->popt & KTRE_INSENSITIVE) {
					left->class[0] = lc(a);
					left->class[1] = lc(right->c);
				} else {
					left->class[0] = a;
					left->class[1] = right->c;
				}

				left->class[2] = 0;
				free_node(re, right);
			} else {
				append_char(&left->class, re->popt & KTRE_INSENSITIVE ? lc(right->c) : right->c);
				free_node(re, right);
			}
		} else if (right->type == NODE_CHAR
			   && left->type == NODE_SEQUENCE
			   && (left->b->type == NODE_CHAR
			       || left->b->type == NODE_STR)) {
			if (left->b->type == NODE_CHAR) {
				char a = left->b->c;
				left->b->type = NODE_STR;
				left->b->class = malloc(3);
				re->info.parser_alloc += 3;

				if (re->popt & KTRE_INSENSITIVE) {
					left->b->class[0] = lc(a);
					left->b->class[1] = lc(right->c);
				} else {
					left->b->class[0] = a;
					left->b->class[1] = right->c;
				}

				left->b->class[2] = 0;
				free_node(re, right);
			} else {
				append_char(&left->class, re->popt & KTRE_INSENSITIVE ? lc(right->c) : right->c);
				free_node(re, right);
			}
		} else {
			struct node *tmp = new_node(re);

			if (!tmp) {
				free_node(re, left);
				free_node(re, right);
				return NULL;
			}

			tmp->loc = re->sp - re->pat;
			tmp->a = left;
			tmp->b = right;
			tmp->type = NODE_SEQUENCE;
			left = tmp;
		}
	}

	return left;
}

static struct node *
parse(struct ktre *re)
{
	struct node *n = term(re);

	if (*re->sp && *re->sp == '|') {
		next_char(re);

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
print_node(struct ktre *re, struct node *n)
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
	case NODE_CHAR:      N0("(char '%c')", n->c);                         break;
	case NODE_WB:        N0("(word boundary)");                           break;
	case NODE_NWB:       N0("(negated word boundary)");                   break;
	case NODE_BACKREF:   N0("(backreference to %d)", n->c);               break;
	case NODE_CLASS:     DBG("(class '"); dbgf(n->class); N0("')");       break;
	case NODE_STR:       DBG("(string '"); dbgf(n->class); N0("')");      break;
	case NODE_NOT:       DBG("(not '"); dbgf(n->class); N0("')");         break;
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
		if (re->group[n->gi].name) N1("(group '%s')", re->group[n->gi].name);
		else                       N1("(group %d)", n->gi);
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
	case INSTR_CLASS: DBG("CLASS   '"); dbgf(instr.class); DBG("'"); break;
	case INSTR_STR:   DBG("STR     '"); dbgf(instr.class); DBG("'"); break;
	case INSTR_NOT:   DBG("NOT     '"); dbgf(instr.class); DBG("'"); break;
	case INSTR_TSTR:  DBG("TSTR    '"); dbgf(instr.class); DBG("'"); break;
	case INSTR_BRANCH:    DBG("BRANCH   %d, %d", instr.a, instr.b);  break;
	case INSTR_CHAR:      DBG("CHAR    '%c'", instr.a);              break;
	case INSTR_SAVE:      DBG("SAVE     %d",  instr.a);              break;
	case INSTR_JMP:       DBG("JMP      %d",  instr.a);              break;
	case INSTR_SETOPT:    DBG("SETOPT   %d",  instr.a);              break;
	case INSTR_BACKREF:   DBG("BACKREF  %d",  instr.a);              break;
	case INSTR_CALL:      DBG("CALL     %d",  instr.a);              break;
	case INSTR_PROG:      DBG("PROG     %d",  instr.a);              break;
	case INSTR_SET_START: DBG("SET_START");                          break;
	case INSTR_TRY:       DBG("TRY");                                break;
	case INSTR_CATCH:     DBG("CATCH");                              break;
	case INSTR_ANY:       DBG("ANY");                                break;
	case INSTR_MANY:      DBG("MANY");                               break;
	case INSTR_DIGIT:     DBG("DIGIT");                              break;
	case INSTR_WORD:      DBG("WORD");                               break;
	case INSTR_SPACE:     DBG("SPACE");                              break;
	case INSTR_BOL:       DBG("BOL");                                break;
	case INSTR_EOL:       DBG("EOL");                                break;
	case INSTR_BOS:       DBG("BOS");                                break;
	case INSTR_EOS:       DBG("EOS");                                break;
	case INSTR_RET:       DBG("RET");                                break;
	case INSTR_WB:        DBG("WB");                                 break;
	case INSTR_NWB:       DBG("NWB");                                break;
	case INSTR_MATCH:     DBG("MATCH");                              break;
	case INSTR_PLA:       DBG("PLA");                                break;
	case INSTR_PLA_WIN:   DBG("PLA_WIN");                            break;
	case INSTR_NLA:       DBG("NLA      %d",  instr.a);              break;
	case INSTR_NLA_FAIL:  DBG("NLA_FAIL");                           break;
	case INSTR_PLB:       DBG("PLB");                                break;
	case INSTR_PLB_WIN:   DBG("PLB_WIN");                            break;
	case INSTR_NLB:       DBG("NLB      %d",  instr.a);              break;
	case INSTR_NLB_FAIL:  DBG("NLB_FAIL");                           break;
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
compile(struct ktre *re, struct node *n, bool rev)
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
			} else {
				if (n->a->type == NODE_CHAR) {
					char *str = malloc(n->c + 1);
					if (!str) return;
					re->info.parser_alloc += n->c + 1;

					for (int j = 0; j < n->c; j++) str[i] = n->a->c;
					str[n->c] = 0;
					emit_class(re, INSTR_TSTR, str, n->loc);
					break;
				} else compile(re, n->a, rev);
			}
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

	case NODE_CLASS:     emit_class(re, INSTR_CLASS,  n->class, n->loc); break;
	case NODE_STR:       emit_class(re, INSTR_STR,    n->class, n->loc); break;
	case NODE_NOT:       emit_class(re, INSTR_NOT,    n->class, n->loc); break;
	case NODE_SETOPT:    emit_c    (re, INSTR_SETOPT, n->c,     n->loc); break;
	case NODE_CHAR:      emit_c    (re, INSTR_CHAR,   n->c,     n->loc); break;
	case NODE_BOL:       emit      (re, INSTR_BOL,              n->loc); break;
	case NODE_EOL:       emit      (re, INSTR_EOL,              n->loc); break;
	case NODE_BOS:       emit      (re, INSTR_BOS,              n->loc); break;
	case NODE_EOS:       emit      (re, INSTR_EOS,              n->loc); break;
	case NODE_ANY:       emit      (re, INSTR_ANY,              n->loc); break;
	case NODE_SET_START: emit      (re, INSTR_SET_START,        n->loc); break;
	case NODE_WB:        emit      (re, INSTR_WB,               n->loc); break;
	case NODE_NWB:       emit      (re, INSTR_NWB,              n->loc); break;
	case NODE_DIGIT:     emit      (re, INSTR_DIGIT,            n->loc); break;
	case NODE_WORD:      emit      (re, INSTR_WORD,             n->loc); break;
	case NODE_SPACE:     emit      (re, INSTR_SPACE,            n->loc); break;
	case NODE_NONE:                                                      break;

	default:
		DBG("\nunimplemented compiler for node of type %d\n", n->type);
		assert(false);
		break;
	}
}

static void
print_compile_error(struct ktre *re)
{
	if ((re->opt & KTRE_DEBUG) == 0) return;
	DBG("\nfailed to compile with error code %d: %s\n",
	    re->err, re->err_str ? re->err_str : "no error message");
	DBG("\t%s\n\t", re->pat);
	for (int i = 0; i < re->loc; i++)
		DBG(re->pat[i] == '\t' ? "\t" : " ");
	DBG("^");
}

static void
print_finish(struct ktre *re,
	     const char *subject,
	     const char *regex,
	     bool ret,
	     int **vec,
	     const char *replaced)
{
	if ((re->opt & KTRE_DEBUG) == 0) return;
	if (!ret && !re->err) { DBG("\nno matches.\n"); return; }
	if (re->err) {
		DBG("\nfailed at runtime with error code %d: %s\n",
		    re->err, re->err_str
		    ? re->err_str
		    : "no error message");
		DBG("\t%s\n\t", regex);
		for (int i = 0; i < re->loc; i++) DBG(" ");
		DBG("^\n");
		return;
	}

	for (unsigned i = 0; i < re->num_matches; i++) {
		DBG("\nmatch %d: `%.*s`",
		    i + 1,
		    vec[i][1],
		    subject + vec[i][0]);

		for (int j = 1, n = (int)strlen(subject);
		     j < re->num_groups;
		     j++) {
			if (vec[i][j * 2 + 1] && vec[i][j * 2] == n)
				continue;
			DBG("\ngroup %d: `%.*s`",
			    j,
			    vec[i][j * 2 + 1],
			    subject + vec[i][j * 2]);
		}
	}

	if (replaced) DBG("\nreplace: `%s`", replaced);
	DBG("\n");
}

struct ktre *
ktre_compile(const char *pat, int opt)
{
	if (!pat) return NULL;

	struct ktre *re = malloc(sizeof *re);
	memset(re, 0, sizeof *re);

	if (opt & KTRE_GLOBAL) opt |= KTRE_UNANCHORED;

	re->err_str = "no error";
	re->max_tp  = -1;
	re->popt    = opt;
	re->pat     = pat;
	re->opt     = opt;
	re->sp      = pat;
	re->n       = new_node(re);

	if (opt & KTRE_DEBUG) {
		DBG("regexpr: %s", pat);
		if (opt) DBG("\noptions:");
		for (size_t i = 0; i < sizeof opt; i++) {
			switch (opt & 1 << i) {
			case KTRE_INSENSITIVE: DBG("\n\tINSENSITIVE"); break;
			case KTRE_UNANCHORED:  DBG("\n\tUNANCHORED");  break;
			case KTRE_EXTENDED:    DBG("\n\tEXTENDED");    break;
			case KTRE_GLOBAL:      DBG("\n\tGLOBAL");      break;
			case KTRE_CONTINUE:    DBG("\n\tCONTINUE");    break;
			default: continue;
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

	if (*re->sp) {
		error(re, KTRE_ERROR_SYNTAX_ERROR,
		      re->sp - re->pat,
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
	emit(re, INSTR_MATCH, re->sp - re->pat);

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

struct ktre *ktre_copy(struct ktre *re)
{
	struct ktre *ret = malloc(sizeof *ret);
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
			re->info.runtime_alloc += (p + 1) * sizeof *THREAD[TP].f; \
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
			re->info.runtime_alloc += (s) * sizeof *THREAD[TP].f; \
		}							\
									\
		if (TP > 0) {						\
			memcpy(THREAD[TP].f,				\
			       THREAD[TP - 1].f,			\
			       (s) * sizeof *THREAD[0].f);		\
		}							\
	} while (0)

static void
new_thread(struct ktre *re,
	   int sp,
	   int ip,
	   int opt,
	   int fp,
	   int la,
	   int ep)
{
	++TP;

	if (TP >= re->info.thread_alloc) {
		if (re->info.thread_alloc * 2 >= KTRE_MAX_THREAD) {
			re->info.thread_alloc = KTRE_MAX_THREAD;

			/*
			 * Account for the case where we're just about
			 * to bump up against the thread limit.
			 */
			TP = (TP >= KTRE_MAX_THREAD) ? KTRE_MAX_THREAD - 1 : TP;
		} else re->info.thread_alloc *= 2;

		re->t = realloc(re->t, re->info.thread_alloc * sizeof *THREAD);
		memset(&THREAD[TP], 0, (re->info.thread_alloc - TP) * sizeof *THREAD);
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
#define CHECKBOUNDS				\
	do {					\
		if (sp < 0 || sp >= slen) FAIL;	\
	} while (0)

static inline bool
execute_instr(ktre *re,
	      int ip,
	      int sp,
	      int fp,
	      int la,
	      int ep,
	      int opt,
	      int loc,
	      bool rev,
	      int slen,
	      int num_steps,
	      const char *subject,
	      int ***vec)
{
	if (re->opt & KTRE_DEBUG) {
		DBG("\n| %4d | %4d | %4d | %4d | %4d | ", ip, sp, TP, fp, num_steps);
		if (sp >= 0) dbgf(subject + sp);
		else dbgf(subject);
	}

	if (THREAD[TP].die) {
		THREAD[TP].die = false;
		FAIL;
	}

	switch (re->c[ip].op) {
	case INSTR_JMP: THREAD[TP].ip = re->c[ip].c; break;
	case INSTR_BACKREF:
		THREAD[TP].ip++;

		if (opt & KTRE_INSENSITIVE) {
			for (int i = 0; i < THREAD[TP].vec[re->c[ip].c * 2 + 1]; i++)
				if (rev
				    ? lc(subject[sp - i]) != lc(subject[THREAD[TP].vec[re->c[ip].c * 2] + sp - i])
				    : lc(subject[sp + i]) != lc(subject[THREAD[TP].vec[re->c[ip].c * 2] + i]))
					FAIL;

			THREAD[TP].sp += rev
				? -THREAD[TP].vec[re->c[ip].c * 2 + 1]
				: THREAD[TP].vec[re->c[ip].c * 2 + 1];
		} else {
			if (!strncmp(rev
				     ? subject + sp + 1 - THREAD[TP].vec[re->c[ip].c * 2 + 1]
				     : subject + sp,
				     &subject[THREAD[TP].vec[re->c[ip].c * 2]],
				     THREAD[TP].vec[re->c[ip].c * 2 + 1]))
				THREAD[TP].sp += THREAD[TP].vec[re->c[ip].c * 2 + 1];
			else FAIL;
		}
		break;
	case INSTR_CLASS:
		THREAD[TP].ip++;
		if (!subject[sp] || sp < 0) FAIL;
		if (strchr(re->c[ip].class, subject[sp])) THREAD[TP].sp++;
		else if (opt & KTRE_INSENSITIVE && strchr(re->c[ip].class, lc(subject[sp])))
			THREAD[TP].sp++;
		else FAIL;
		break;
	case INSTR_STR: case INSTR_TSTR:
		THREAD[TP].ip++;

		if (opt & KTRE_INSENSITIVE) {
			for (unsigned i = 0; i < re->c[ip].len; i++)
				if (rev
				    ? lc(subject[sp + 1 - re->c[ip].len + i]) != re->c[ip].class[i]
				    : lc(subject[sp + i]) != re->c[ip].class[i])
					FAIL;

			THREAD[TP].sp += rev ? -re->c[ip].len : re->c[ip].len;
		} else {
			if (rev
			    ? !strncmp(subject + sp + 1 - re->c[ip].len, re->c[ip].class, re->c[ip].len)
			    : !strncmp(subject + sp, re->c[ip].class, re->c[ip].len))
				THREAD[TP].sp += rev ? -re->c[ip].len : re->c[ip].len;
			else FAIL;
		}
		break;
	case INSTR_NOT:
		THREAD[TP].ip++;
		CHECKBOUNDS;
		if (strchr(re->c[ip].class, subject[sp])) FAIL;
		THREAD[TP].sp++;
		break;
	case INSTR_BOL:
		if (!(sp > 0 && subject[sp - 1] == '\n') && sp != 0) FAIL;
		THREAD[TP].ip++;
		break;
	case INSTR_EOL:
		if (sp < 0 || (subject[sp] != '\n' && sp != slen)) FAIL;
		THREAD[TP].ip++;
		break;
	case INSTR_BOS:
		if (sp) FAIL;
		THREAD[TP].ip++;
		break;
	case INSTR_EOS:
		if (sp != slen) FAIL;
		THREAD[TP].ip++;
		break;
	case INSTR_WB:
		THREAD[TP].ip++;
		CHECKBOUNDS;
		if (sp == 0 && strchr(WORD, subject[sp])) return true;
		if (!!strchr(WORD, subject[sp]) != !!strchr(WORD, subject[sp - 1]))
			return true;
		FAIL;
		break;
	case INSTR_NWB:
		THREAD[TP].ip++;
		CHECKBOUNDS;
		if (sp == 0 && !strchr(WORD, subject[sp])) return true;
		if (!!strchr(WORD, subject[sp]) == !!strchr(WORD, subject[sp - 1]))
			return true;
		FAIL;
		break;
	case INSTR_CHAR:
		THREAD[TP].ip++;
		CHECKBOUNDS;

		if (opt & KTRE_INSENSITIVE
		    ? lc(subject[sp]) != lc(re->c[ip].c)
		    : subject[sp] != re->c[ip].c)
			FAIL;
		rev ? THREAD[TP].sp-- : THREAD[TP].sp++;
		break;
	case INSTR_ANY:
		THREAD[TP].ip++;
		if (sp < 0 || !subject[sp]) FAIL;
		if (opt & KTRE_MULTILINE ? false : subject[sp] == '\n') FAIL;
		rev ? THREAD[TP].sp-- : THREAD[TP].sp++;
		break;
	case INSTR_MANY:
		THREAD[TP].ip++;
		if (!(sp >= 0 && subject[sp])) FAIL;
		rev ? THREAD[TP].sp-- : THREAD[TP].sp++;
		break;
	case INSTR_BRANCH:
		THREAD[TP].ip = re->c[ip].b;
		new_thread(re, sp, re->c[ip].a, opt, fp, la, ep);
		break;
	case INSTR_MATCH: {
		int n = 0;

		for (unsigned i = 0; i < re->num_matches; i++)
			if (re->vec[i][0] == sp)
				n++;

		if (n) FAIL;
		if (!(opt & KTRE_UNANCHORED) && (sp < 0 || subject[sp])) FAIL;

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
		if (!(opt & KTRE_GLOBAL)) return true;

		TP = 0;
		THREAD[TP].ip = 0;
		THREAD[TP].sp = sp;

		if (THREAD[TP].sp > slen) return true;
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
		CHECKBOUNDS;
		if (!strchr(DIGIT, subject[sp]) || !subject[sp]) FAIL;
		rev ? THREAD[TP].sp-- : THREAD[TP].sp++;
		break;
	case INSTR_WORD:
		THREAD[TP].ip++;
		if (!strchr(WORD, subject[sp]) || !subject[sp]) FAIL;
		rev ? THREAD[TP].sp-- : THREAD[TP].sp++;
		break;
	case INSTR_SPACE:
		THREAD[TP].ip++;
		if (!strchr(WHITESPACE, subject[sp]) || !subject[sp]) FAIL;
		rev ? THREAD[TP].sp-- : THREAD[TP].sp++;
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
run(struct ktre *re, const char *subject, int ***vec)
{
	*vec = NULL;
	re->num_matches = 0;
	TP = -1;

	if (!re->info.thread_alloc) {
		re->info.thread_alloc = 25;
		re->t = malloc(re->info.thread_alloc * sizeof *THREAD);
		if (re->err) return false;
		memset(re->t, 0, re->info.thread_alloc * sizeof *THREAD);
	}

	int slen = (int)strlen(subject);
	if (re->opt & KTRE_CONTINUE && re->cont >= slen) return false;

	/* Push the initial thread. */
	new_thread(re, re->opt & KTRE_CONTINUE ? re->cont : 0, 0, re->opt, 0, 0, 0);

	int num_steps = 0;
	DBG("\n|   ip |   sp |   tp |   fp | step |");

	while (TP >= 0)
		if (!execute_instr(re,
				   THREAD[TP].ip,
				   THREAD[TP].sp,
				   THREAD[TP].fp,
				   THREAD[TP].la,
				   THREAD[TP].ep,
				   THREAD[TP].opt,
				   re->c[THREAD[TP].ip].loc,
				   THREAD[TP].rev,
				   slen,
				   num_steps++,
				   subject,
				   vec))
			return false;

	return !!re->num_matches;
}

struct ktre_info
ktre_free(struct ktre *re)
{
	if (re->copied) return re->info;
	free_node(re, re->n);
	if (re->err) free(re->err_str);

	if (re->c) {
		for (int i = 0; i < re->ip; i++)
			if (re->c[i].op == INSTR_TSTR)
				free(re->c[i].class);

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
			free(re->group[i].name);

	free(re->group);
	free(re->t);
	struct ktre_info info = re->info;
	struct ktre_minfo *mi = re->minfo;

	while (mi) {
		struct ktre_minfo *mi2 = mi;
		mi = mi->next;
		free(mi2);
	}

	free(re);
	return info;
}

_Bool
ktre_exec(struct ktre *re, const char *subject, int ***vec)
{
	DBG("\nsubject: %s", subject);

	if (re->err) {
		if (re->err_str) free(re->err_str);
		re->err = KTRE_ERROR_NO_ERROR;
	}

	int **v = NULL;
	_Bool ret = false;

	if (vec) ret = run(re, subject, vec);
	else     ret = run(re, subject, &v);

	if (vec) print_finish(re, subject, re->pat, ret, *vec, NULL);
	else     print_finish(re, subject, re->pat, ret, v, NULL);

	return ret;
}

_Bool ktre_match(const char *subject, const char *pat, int opt, int ***vec)
{
	struct ktre *re = ktre_compile(pat, opt);

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

char *ktre_replace(const char *subject, const char *pat, const char *replacement, const char *indicator, int opt)
{
	struct ktre *re = ktre_compile(pat, opt);

	if (!re->err) {
		char *ret = ktre_filter(re, subject, replacement, indicator);
		ktre_free(re);
		return ret;
	}

	ktre_free(re);
	return NULL;
}

static void
smartcopy(char *dest, const char *src, size_t n,
          bool u, bool uch, bool l, bool lch)
{
	for (int i = 0; i < (int)n; i++) {
		if (i == 0 && uch) {
			dest[i] = uc(src[i]);
			continue;
		}

		if (i == 0 && lch) {
			dest[i] = lc(src[i]);
			continue;
		}

		if (u)      dest[i] = uc(src[i]);
		else if (l) dest[i] = lc(src[i]);
		else        dest[i] =    src[i];
	}
}

#define SIZE_STRING(ptr,n) ptr = realloc(ptr, n * sizeof *ptr)
char *ktre_filter(struct ktre *re, const char *subject, const char *replacement, const char *indicator)
{
	DBG("\nsubject: %s", subject);

	int **vec = NULL;
	if (!run(re, subject, &vec) || re->err) {
		print_finish(re, subject, re->pat, false, vec, NULL);
		return NULL;
	}

	char *ret = malloc(16);
	*ret = 0;
	int idx = 0;

	for (unsigned i = 0; i < re->num_matches; i++) {
		bool u   = false, l   = false;
		bool uch = false, lch = false;

		if (i > 0) {
			int len = vec[i][0]
				- (vec[i - 1][0]
				   +  vec[i - 1][1]);
			SIZE_STRING(ret, idx + len + 1);
			strncpy(ret + idx,
			        subject
			        + vec[i - 1][0]
			        + vec[i - 1][1], len);

			idx += len;
		} else {
			idx = vec[i][0];
			SIZE_STRING(ret, idx + 1);
			strncpy(ret, subject, idx);
		}

		ret[idx] = 0;

		char *match = NULL;
		int j = 0;

		const char *r = replacement;

		while (*r) {
			if (!strncmp(r, indicator, strlen(indicator))) {
				const char *t = r;
				t += strlen(indicator);
				int n = dec_num(&t);

				if (n < 0 || n >= re->num_groups)
					goto skip_capture;

				r = t;

				/* ignore unintialized groups */
				if (vec[i][n * 2] < 0 || vec[i][n * 2 + 1] < 0)
					continue;

				SIZE_STRING(match, j + vec[i][n * 2 + 1] + 1);
				smartcopy(match + j, subject + vec[i][n * 2],
				          vec[i][n * 2 + 1], u, uch, l, lch);

				j += vec[i][n * 2 + 1];
				uch = lch = false;
				continue;
			}

		skip_capture:
			if (*r != '\\') {
				SIZE_STRING(match, j + 2);
				match[j] = *r;
				if (uch || u) match[j] = uc(*r);
				if (lch || l) match[j] = lc(*r);
				j++;
				r++;
				lch = uch = false;
				continue;
			}

			r++;

			switch (*r) {
			case 'U': u =     true;  r++; break;
			case 'L': l =     true;  r++; break;
			case 'E': l = u = false; r++; break;
			case 'l': lch =   true;  r++; break;
			case 'u': uch =   true;  r++; break;

			case '0': case '1': case '2': case '3':
			case '4': case '5': case '6': case '7':
			case '8': case '9': {
				int n = dec_num(&r);
				SIZE_STRING(match, j + 2);
				match[j] = n;
			} break;
			}
		}

		if (match) {
			match[j] = 0;

			SIZE_STRING(ret, idx + j + 1);
			strcpy(ret + idx, match);
			ret[idx + j] = 0;
			idx += j;

			free(match);
		}
	}

	int end = vec[re->num_matches - 1][0]
		+ vec[re->num_matches - 1][1];
	SIZE_STRING(ret, idx + strlen(subject) - end + 1);
	strncpy(ret + idx, subject + end, strlen(subject) - end);
	idx += strlen(subject) - end;
	ret[idx] = 0;

	char *a = malloc(strlen(ret) + 1);
	strcpy(a, ret);
	free(ret);
	print_finish(re, subject, re->pat, ret, vec, a);

	return a;
}

char **ktre_split(ktre *re, const char *subject, int *len)
{
	DBG("\nsubject: %s", subject);

	*len = 0;
	int **vec = NULL;

	if (!run(re, subject, &vec) || re->err) {
		print_finish(re, subject, re->pat, false, vec, NULL);
		return NULL;
	}

	char **r = NULL;
	unsigned j = 0;

	for (unsigned i = 0; i < re->num_matches; i++) {
		if (vec[i][0] == 0 || vec[i][0] == (int)strlen(subject)) continue;
		r = realloc(r, (*len + 1) * sizeof *r);

		r[*len] = malloc(vec[i][0] - j + 1);
		strncpy(r[*len], subject + j, vec[i][0] - j);

		r[*len][vec[i][0] - j] = 0;
		j = vec[i][0] + vec[i][1];

		(*len)++;
	}

	if (strlen(subject) >= j) {
		r = realloc(r, (*len + 1) * sizeof *r);
		r[*len] = malloc(strlen(subject) - j + 1);
		strcpy(r[*len], subject + j);
		(*len)++;
	}

	return r;
}

int **ktre_getvec(const struct ktre *re)
{
	int **vec = malloc(re->num_matches * sizeof *re->vec);

	for (unsigned i = 0; i < re->num_matches; i++) {
		vec[i] = malloc(re->num_groups * 2 * sizeof **re->vec);
		memcpy(vec[i], re->vec[i], re->num_groups * 2 * sizeof **re->vec);
	}

	return vec;
}
