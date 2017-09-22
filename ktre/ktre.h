/*
 * MIT License
 *
 * Copyright (c) 2017 "KrokodileGlue" (https://github.com/KrokodileGlue)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/*
 * KTRE: a tiny regex engine
 * KTRE supports character classes [a-zA-Z_], submatching and
 * backreferencing (cat|dog)\1, negated character classes [^a-zA-Z_],
 * a variety of escaped metacharacters (\w, \W, \d, \D, \s, \b), mode
 * modifiers (?x-i), comments (?#this is a comment), free-spaced
 * expressions, lookaround assertions (?=re) (?!re) (?<=re) (?<!re),
 * atomic groups (?>re), subroutine calls (foo)(?1), recursion (?R),
 * and, of course, the greedy, non-greedy, and possessive quantifiers
 * you would expect.
 *
 * KTRE currently supports the following options: KTRE_INSENSITIVE,
 * KTRE_UNANCHORED, and KTRE_EXTENDED. All of those options except for
 * KTRE_UNANCHORED may be modified by the regular expression. The
 * options are reset every time the regex is run.
 *
 * KTRE_INSENSITIVE: (?i)
 *     Makes the runtime match characters of different cases.
 * KTRE_UNANCHORED:
 *     Counts a match anywhere in the subject as a match. The default
 *     behavior is to reject any string that does not match exactly.
 * KTRE_EXTENDED: (?x)
 *     This option turns on so-called 'free-spaced' mode, which allows
 *     whitespace to occur in most parts of the grammar without
 *     side-effects.  Note that this does not allow whitespace
 *     anywhere; '(?#foobar)' is not the same thing as '( ?#foobar)',
 *     but 'foobar' is the same as 'foo bar'.  If you want to match a
 *     whitespace character in free-spaced mode, you must escape it or
 *     put it into a character class.
 *
 * Here's a simple demo that runs a regex on a subject from the
 * commandline options:
 *
 * #include <stdlib.h>
 * #include <stdio.h>
 *
 * #define KTRE_DEBUG
 * #define KTRE_IMPLEMENTATION
 * #include "ktre.h"
 *
 * static inline void
 * do_regex(struct ktre *re, const char *regex, const char *subject, FILE *f)
 * {
 * 	int **vec = NULL;
 *
 * 	if (ktre_exec(re, subject, &vec)) {
 * 		for (int i = 0; i < re->num_matches; i++) {
 * 			fprintf(f, "\nmatch %d: `%.*s`", i + 1, vec[i][1], subject + vec[i][0]);
 * 			for (int j = 1; j < re->num_groups; j++) {
 * 				fprintf(f, "\ngroup %d: `%.*s`", j, vec[i][j * 2 + 1], subject + vec[i][j * 2]);
 * 			}
 * 		}
 * 	} else if (re->err) {
 * 		fprintf(f, "\nfailed at runtime with error code %d: %s\n", re->err, re->err_str);
 * 		fprintf(f, "\t%s\n\t", regex);
 * 		for (int i = 0; i < re->loc; i++) fputc(' ', f);
 * 		fputc('^', f);
 * 	} else {
 * 		fprintf(f, "\nno match.");
 * 	}
 *
 * 	fputc('\n', f);
 * }
 *
 * static inline void
 * do_replace(struct ktre *re, const char *regex, const char *subject, const char *replacement, FILE *f)
 * {
 * 	char *ret = NULL;
 *
 * 	if ((ret = ktre_filter(re, subject, replacement))) {
 * 		fprintf(f, "\nmatched: %s", ret);
 * 		free(ret);
 * 	} else if (re->err) {
 * 		fprintf(f, "\nfailed at runtime with error code %d: %s\n", re->err, re->err_str);
 * 		fprintf(f, "\t%s\n\t", regex);
 * 		for (int i = 0; i < re->loc; i++) fputc(' ', f);
 * 		fputc('^', f);
 * 	} else {
 * 		fprintf(f, "\nno match.");
 * 	}
 *
 * 	fputc('\n', f);
 * }
 *
 * int
 * main(int argc, char *argv[])
 * {
 * 	if (argc < 3 || argc > 4) {
 * 		fprintf(stderr, "Usage: subject regex [replacement]\n");
 * 		return EXIT_FAILURE;
 * 	}
 *
 * 	char *subject = argv[1], *regex = argv[2], *replacement = NULL;
 * 	if (argc == 4) replacement = argv[3];
 *
 * 	struct ktre *re = ktre_compile(regex, KTRE_UNANCHORED | KTRE_GLOBAL);
 *
 * 	if (re->err) {
 * 		fprintf(stderr, "\nfailed to compile with error code %d: %s\n", re->err, re->err_str);
 * 		fprintf(stderr, "\t%s\n\t", regex);
 * 		for (int i = 0; i < re->loc; i++) fputc(' ', stderr);
 * 		fputc('^', stderr);
 * 		return EXIT_FAILURE;
 * 	}
 *
 * 	if (replacement) do_replace(re, regex, subject, replacement, stderr);
 * 	else             do_regex(re, regex, subject, stderr);
 *
 * 	ktre_free(re);
 *
 * 	return EXIT_SUCCESS;
 * }
 */

#ifndef KTRE_H
#define KTRE_H

#ifdef __cplusplus
extern "C" {
#endif

/* manually disable warnings and errors about "unsafe" functions on
 * the Microsoft platform. */
#ifdef _MSC_VER
#pragma warning(disable:4996)
#endif

#if defined(_MSC_VER) && defined(KTRE_DEBUG)
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#endif

/* error codes */
enum ktre_error {
	KTRE_ERROR_NO_ERROR,
	KTRE_ERROR_STACK_OVERFLOW,
	KTRE_ERROR_CALL_OVERFLOW,
	KTRE_ERROR_SYNTAX_ERROR,
	KTRE_ERROR_OUT_OF_MEMORY,
	KTRE_ERROR_TOO_MANY_GROUPS
};

/* options */
enum {
	KTRE_INSENSITIVE = 1 << 0,
	KTRE_UNANCHORED  = 1 << 1,
	KTRE_EXTENDED    = 1 << 2,
	KTRE_GLOBAL      = 1 << 3,
};

/* settings and limits */
#define KTRE_MAX_GROUPS 100
#define KTRE_MAX_THREAD 200
#define KTRE_MAX_CALL_DEPTH 100
#define KTRE_MEM_CAP 1000000

struct ktre_info {
	int ba, mba; /* bytes allocated and max bytes allocated */
	int bf;      /* bytes freed */
	int num_allocations, num_alloc, num_free;

	int thread_alloc;
	int instr_alloc;
	int parser_alloc;
	int runtime_alloc;
};

struct ktre_minfo {
#ifdef KTRE_DEBUG
	const char *file;
	int line;
#endif
	int size;
	struct ktre_minfo *prev, *next;
};

struct ktre {
	/* ===== public fields ===== */
	int num_matches;
	int num_groups;
	int opt;

	/*
	 * String containing an error message in the case of failure
	 * during parsing or compilation.
	 */
	char *err_str;
	enum ktre_error err; /* error status code */

	/*
	 * The location of any error that occurred, as an index in the
	 * last subject the regex was run on.
	 */
	int loc;

	/* ===== private fields ===== */
	struct instr *c; /* code */
	int ip;          /* instruction pointer */
	int num_prog;    /* number of progress instructions */
	char const *pat; /* pattern that's currently begin compiled */
	char const *sp;  /* to the character currently being parsed */
	int popt;        /* the options, as seen by the parser */
	int gp;          /* group pointer */
	struct node *n;  /* the head node of the ast */

	struct group {
		int address;

		/*
		 * These boolean flags indicate whether this groups
		 * has been compiled or called as a subroutine,
		 * respectively. The is_called field is flexible; it's
		 * used whenever a group must be called as a
		 * subroutine, which a counted repetition may do.
		 */
		_Bool is_compiled;
		_Bool is_called;
	} group[KTRE_MAX_GROUPS];

	/* runtime */
	struct thread {
		int ip, sp, fp, la, e, opt;
		int *frame, *vec, *prog, *las, *exception;
		_Bool die, rev;
	} *t;

	int tp, max_tp;
	int **vec;

	struct ktre_info info;
	struct ktre_minfo *minfo;

	_Bool copied;
};

/* API prototypes */
struct ktre *ktre_compile(const char *pat, int opt);
struct ktre *ktre_copy(struct ktre *re);
_Bool ktre_exec(struct ktre *re, const char *subject, int ***vec);
char *ktre_filter(struct ktre *re, const char *subject, const char *replacement);
int **ktre_getvec(const struct ktre *re);
struct ktre_info ktre_free(struct ktre *re);

#ifdef __cplusplus
}
#endif

#ifdef KTRE_IMPLEMENTATION
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define WHITESPACE " \t\r\n\v\f"
#define DIGIT      "0123456789"
#define WORD       "_0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"

#ifdef KTRE_DEBUG
#include <stdio.h>
#include <assert.h>

static void print_node(struct node *n);
#define DBG(...) fprintf(stderr, __VA_ARGS__)
static void
dbgf(const char *str)
{
	for (int i = 0; i < (int)strlen(str); i++) {
		if ((strchr(WHITESPACE, str[i]) || str[i] == '\\') && str[i] != ' ') {
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
		} else {
			fputc(str[i], stderr);
		}
	}
}
#endif /* KTRE_DEBUG */

static void error(struct ktre *re, enum ktre_error err, int loc, char *fmt, ...);

/* memory functions */
#ifndef KTRE_MALLOC
#define KTRE_MALLOC  malloc
#define KTRE_FREE    free
#endif

/*
 * These memory functions must always be passed a pointer to the first
 * byte of an allocated block; passing a pointer to any other byte in
 * the block will cause it to incorrectly parse the block data and
 * will probably result in a segfault later on down the line.
 * So something like:
 *     char *a = ktre__malloc(5);
 *     a++;
 *     ktre__free(a);
 * would be incorrect.
 */

static void *ktre__malloc (struct ktre *re,            size_t n, const char *file, int line);
static void *ktre__realloc(struct ktre *re, void *ptr, size_t n, const char *file, int line);
static void  ktre__free   (struct ktre *re, void *ptr,           const char *file, int line);

static void *
ktre__malloc(struct ktre *re, size_t n, const char *file, int line)
{
	if (re->info.ba + n + sizeof (struct ktre) > KTRE_MEM_CAP) {
		error(re, KTRE_ERROR_OUT_OF_MEMORY, 0, NULL);
#ifdef KTRE_DEBUG
		DBG("\nrunning out of memory at %d bytes:\n\trequest for %zd bytes at %s:%d",
		    re->info.ba, n, file, line);
#endif
		return NULL;
	}

	struct ktre_minfo *mi = KTRE_MALLOC(n + sizeof (struct ktre_minfo));

	if (!mi) {
		error(re, KTRE_ERROR_OUT_OF_MEMORY, 0, NULL);
		return NULL;
	}

	re->info.num_alloc++;
	re->info.ba += n + sizeof (struct ktre_minfo);
	re->info.mba += n + sizeof (struct ktre_minfo);

#ifdef KTRE_DEBUG
	mi->file = file;
	mi->line = line;
#endif
	mi->next = re->minfo;

	if (re->minfo)
		re->minfo->prev = mi;

	mi->prev = NULL;
	mi->size = (int)n;
	re->minfo = mi;

	return mi + 1;
}

static void *
ktre__realloc(struct ktre *re, void *ptr, size_t n, const char *file, int line)
{
	if (!ptr) return ktre__malloc(re, n, file, line);

	struct ktre_minfo *mi = (struct ktre_minfo *)ptr - 1;
	int diff = n - mi->size;

	/*
	 * We don't need to grow the block, so just return early.
	 */
	if (diff <= 0) return ptr;

	if (re->info.ba + diff > KTRE_MEM_CAP) {
		error(re, KTRE_ERROR_OUT_OF_MEMORY, 0, NULL);
		return NULL;
	}

	void *p = ktre__malloc(re, n, file, line);

	if (p) {
		memcpy(p, ptr, n > mi->size ? mi->size : n);
		ktre__free(re, ptr, file, line);
	}

	return p;
}

static void
ktre__free(struct ktre *re, void *ptr, const char *file, int line)
{
	if (!ptr) return;

	struct ktre_minfo *mi = (struct ktre_minfo *)ptr - 1;
	re->info.ba -= mi->size + sizeof (struct ktre_minfo);
	re->info.bf += mi->size + sizeof (struct ktre_minfo);
	re->info.num_free++;

	if (!mi->prev) {
		re->minfo = mi->next;
	} else {
		mi->prev->next = mi->next;
	}

	if (mi->next)
		mi->next->prev = mi->prev;

	KTRE_FREE(mi);
}

#define ktre__malloc(re, n)       ktre__malloc (re, n,      __FILE__, __LINE__)
#define ktre__realloc(re, ptr, n) ktre__realloc(re, ptr, n, __FILE__, __LINE__)
#define ktre__free(re, ptr)       ktre__free   (re, ptr,    __FILE__, __LINE__)

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

	re->group[re->gp].is_compiled = false;
	re->group[re->gp].address = -1;
	re->group[re->gp].is_called = false;
	return re->gp++;
}

struct instr {
	enum {
		INSTR_MATCH,
		INSTR_CHAR,
		INSTR_JMP,
		INSTR_BRANCH,
		INSTR_ANY,
		INSTR_CLASS,
		INSTR_TSTR,
		INSTR_STR,
		INSTR_NOT,
		INSTR_BACKREF,
		INSTR_BOL,
		INSTR_EOL,
		INSTR_OPTON,
		INSTR_OPTOFF,
		INSTR_TRY,
		INSTR_CATCH,
		INSTR_SET_START,
		INSTR_WB,
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

	int loc;
};

static void
grow_code(struct ktre *re, int n)
{
	if (!re->info.instr_alloc) {
		re->info.instr_alloc = 25;
		re->c = ktre__malloc(re, sizeof re->c[0] * re->info.instr_alloc);
	}

	if (re->ip + n >= re->info.instr_alloc) {
		if (re->ip + n >= re->info.instr_alloc * 2) {
			re->info.instr_alloc = re->ip + n;
		} else {
			re->info.instr_alloc *= 2;
		}

		re->c = ktre__realloc(re, re->c, sizeof re->c[0] * re->info.instr_alloc);
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
		NODE_CHAR,
		NODE_SEQUENCE,
		NODE_ASTERISK,
		NODE_PLUS,
		NODE_OR,
		NODE_GROUP,
		NODE_QUESTION,
		NODE_ANY,	/* . matches anything */
		NODE_CLASS,	/* a character class */
		NODE_STR,
		NODE_NOT,
		NODE_BACKREF,	/* a backreference to an existing group */
		NODE_BOL,
		NODE_EOL,
		NODE_OPTON,	/* turns some option off */
		NODE_OPTOFF,	/* turns some option on */
		NODE_REP,	/* counted repetition */
		NODE_ATOM,	/* atomic group */
		NODE_SET_START, /* set the start position of the group capturing the entire match */
		NODE_WB,	/* word boundary */
		NODE_CALL,
		NODE_PLA,	/* positive lookahead */
		NODE_PLB,	/* positive lookbehind */
		NODE_NLA,	/* negative lookahead */
		NODE_NLB,	/* negative lookbehind */
		NODE_RECURSE,	/* (?R) tries to match the entire regex again at the current point */
		NODE_DIGIT,
		NODE_SPACE,
		NODE_WORD,
		NODE_NONE
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
	/* sometimes parse errors can produce NULL nodes */
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
	case NODE_CLASS: case NODE_STR: ktre__free(re, n->class); break;
	default: break;
	}

	ktre__free(re, n);
}

static void
next_char(struct ktre *re)
{
	if (*re->sp) re->sp++;
}

#define MAX_ERROR_LEN 256 /* some arbitrary limit */

#include <stdarg.h>
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

	re->err_str = ktre__malloc(re, MAX_ERROR_LEN);
	va_list args;
	va_start(args, fmt);
	vsnprintf(re->err_str, MAX_ERROR_LEN, fmt, args);
	va_end(args);
}

static struct node *parse(struct ktre *re);

static inline char
lc(char c)
{
	if (c >= 'A' && c <= 'Z') {
		c = (c - 'A') + 'a';
	}

	return c;
}

static char *
class_add_char(struct ktre *re, char *class, char c)
{
	size_t len = class ? strlen(class) : 0;
	class = ktre__realloc(re, class, len + 2);
	class[len] = c;
	class[len + 1] = 0;
	return class;
}

static char *
class_add_str(struct ktre *re, char *class, const char *c)
{
	size_t len = class ? strlen(class) : 0;
	class = ktre__realloc(re, class, len + strlen(c) + 1);
	strcpy(class + len, c);
	return class;
}

static char *
strclone(struct ktre *re, const char *str)
{
	char *ret = ktre__malloc(re, strlen(str) + 1);
	if (!ret) return NULL;
	re->info.parser_alloc += strlen(str) + 1;
	strcpy(ret, str);
	return ret;
}

static struct node *
new_node(struct ktre *re)
{
	struct node *n = ktre__malloc(re, sizeof *n);
	if (!n) return NULL;
	re->info.parser_alloc += sizeof *n;
	memset(n, 0, sizeof *n);
	n->loc = -1;
	return n;
}

static int
dec_num(const char **s)
{
	int n = 0;

	while (**s && isdigit(**s)) {
		n = n * 10 + (**s - '0');
		(*s)++;
	}

	return n;
}

static int
hex_num(const char **s)
{
	int n = 0;

	while (**s
	       && ((lc(**s) >= 'a' && lc(**s) <= 'f')
	           || (**s >= '0' && **s <= '9')))
	{
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
	int n = 0;

	while (**s >= '0' && **s <= '7') {
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
	if (n < 0)
		error(re, KTRE_ERROR_SYNTAX_ERROR, re->sp - re->pat, "expected a number");
	return n;
}

static int
parse_hex_num(struct ktre *re)
{
	int n = hex_num(&re->sp);
	if (n < 0)
		error(re, KTRE_ERROR_SYNTAX_ERROR, re->sp - re->pat, "expected a number");
	return n;
}

static int
parse_oct_num(struct ktre *re)
{
	int n = oct_num(&re->sp);
	if (!(*re->sp >= '0' && *re->sp <= '7'))
		error(re, KTRE_ERROR_SYNTAX_ERROR, re->sp - re->pat, "expected a number");
	return n;
}

static struct node *
parse_special_group(struct ktre *re)
{
	struct node *left = new_node(re);
	left->loc = re->sp - re->pat;

	if (*re->sp == '#') { /* comments */
		next_char(re);

		while (*re->sp && *re->sp != ')') re->sp++;

		left->type = NODE_NONE;
	} else if (re->sp[0] == '<' && re->sp[1] == '=') { /* positive lookbehind */
		re->sp += 2;

		left->type = NODE_PLB;
		left->a = parse(re);
	} else if (*re->sp == ':') { /* uncaptured group */
		next_char(re);

		free_node(re, left);
		left = parse(re);
	} else if (*re->sp == '<' && re->sp[1] == '!') { /* negative lookbehind */
		re->sp += 2;

		left->type = NODE_NLB;
		left->a = parse(re);
	} else if (*re->sp == '>') {
		next_char(re);

		left->type = NODE_ATOM;
		left->a = parse(re);
	} else if (isdigit(*re->sp)) { /* subroutine call */
		int len = 0, a = -1;

		while (re->sp[len] && re->sp[len] != ')') len++;
		if (re->sp[len] != ')') {
			error(re, KTRE_ERROR_SYNTAX_ERROR, re->sp + len - re->pat - 1, "unmatched '('");
			free_node(re, left->a); left->a = NULL;
			free_node(re, left);
			return NULL;
		}

		len--;

		for (int i = len; i >= 0; i--) {
			if (!isdigit(re->sp[i])) {
				error(re, KTRE_ERROR_SYNTAX_ERROR, re->pat - re->sp + i,
				      "non-digits and negative numbers are forbidden in subroutine calls");
				free_node(re, left);
				return NULL;
			}

			if (i == len) a = re->sp[i] - '0';
			else {
				int d = re->sp[i] - '0';
				for (int j = 0; j < len - i; j++) {
					d *= 10;
				}
				a += d;
			}
		}

		left->type = NODE_CALL;
		left->c = a;

		if (a <= re->gp) {
			re->group[a].is_called = true;
		}

		re->sp += len + 1;
	} else if (*re->sp == '=') {
		next_char(re);

		left->type = NODE_PLA;
		left->a = parse(re);
	} else if (*re->sp == '!') {
		next_char(re);

		left->type = NODE_NLA;
		left->a = parse(re);
	} else { /* mode modifiers */
		left->type = NODE_NONE;
		bool neg = false, off = false;
		int opt;

		while (*re->sp && *re->sp != ')') {
			opt = 0;
			switch (*re->sp) {
			case 'i': opt |=  KTRE_INSENSITIVE;             break;
			case 'c': opt |=  KTRE_INSENSITIVE; off = true; break;
			case 'x': opt |=  KTRE_EXTENDED;                break;
			case 't': opt |=  KTRE_EXTENDED;    off = true; break;
			case '-': neg = true; next_char(re); continue;
			default:
				error(re, KTRE_ERROR_SYNTAX_ERROR, re->sp - re->pat, "invalid mode modifier");
				free_node(re, left);
				return NULL;
			}

			if (off || neg) re->popt &= ~opt;
			else re->popt |= opt;

			struct node *tmp = new_node(re);
			tmp->loc = re->sp - re->pat;
			tmp->type = NODE_SEQUENCE;
			tmp->a = left;
			tmp->b = new_node(re);
			tmp->b->loc = re->sp - re->pat;
			tmp->b->type = off || neg ? NODE_OPTOFF : NODE_OPTON;
			tmp->b->c = opt;
			left = tmp;

			off = false;
			re->sp++;
		}
	}

	return left;
}

static struct node *
parse_group(struct ktre *re)
{
	struct node *left = new_node(re);
	left->loc = re->sp - re->pat;
	next_char(re);

	if (!strncmp(re->sp, "?R)", 3)) {
		re->sp += 2;
		left->type = NODE_RECURSE;
		re->group[0].is_called = true;
	} else if (*re->sp == '?') {
		next_char(re);
		free_node(re, left);
		left = parse_special_group(re);
	} else {
		left->gi = add_group(re);
		re->group[left->gi].is_called = false;
		left->type = NODE_GROUP;
		left->a = parse(re);
	}

	if (*re->sp != ')' && !re->err) {
		error(re, KTRE_ERROR_SYNTAX_ERROR, re->sp - re->pat, "unmatched '('");
		free_node(re, left->a);
		left->a = NULL;
		free_node(re, left);
		return NULL;
	}

	next_char(re);

	return left;
}

static struct node *
parse_primary(struct ktre *re)
{
	struct node *left = new_node(re);

	if (*re->sp == ')') {
		free_node(re, left);
		return NULL;
	}

again:
	switch (*re->sp) {
	case '\\': /* escape sequences */
		next_char(re);

		switch (*re->sp) {
		case 's':
			left->type = NODE_SPACE;
			next_char(re);
			break;

		case 'S':
			left->type = NODE_NOT;
			left->class = strclone(re, WHITESPACE);
			next_char(re);
			break;

		case 'd':
			left->type = NODE_DIGIT;
			next_char(re);
			break;

		case 'D':
			left->type = NODE_NOT;
			left->class = strclone(re, DIGIT);
			next_char(re);
			break;

		case 'w':
			left->type = NODE_WORD;
			next_char(re);
			break;

		case 'W':
			left->type = NODE_NOT;
			left->class = strclone(re, WORD);
			next_char(re);
			break;

		case '0': /* octal escape sequences */
			next_char(re);
			left->type = NODE_CHAR;
			left->c = parse_oct_num(re);
			break;

		case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			left->type = NODE_BACKREF;
			left->c = *re->sp - '0';
			if (isdigit(re->sp[1])) {
				left->c *= 10;
				left->c += re->sp[1] - '0';
				next_char(re);
			}
			next_char(re);
			break;

		case 'K':
			left->type = NODE_SET_START;
			next_char(re);
			break;

		case 'b':
			left->type = NODE_WB;
			next_char(re);
			break;

		case 'n':
			left->type = NODE_CHAR;
			left->c = '\n';
			next_char(re);
			break;

		case 'x': /* hexadecimal escape sequences */
			next_char(re);
			left->type = NODE_CHAR;
			left->c = parse_hex_num(re);
			break;

		default:
			left->type = NODE_CHAR;
			left->c = *re->sp;
			next_char(re);
		}
		break;

	case '[': { /* character classes */
		int loc = re->sp - re->pat;
		next_char(re);

		if (*re->sp == '^') {
			next_char(re);
			left->type = NODE_NOT;
		} else {
			left->type = NODE_CLASS;
		}

		char *class = NULL;
		while (*re->sp && *re->sp != ']') {
			if (re->sp[1] == '-' && re->sp[2] != ']') {
				for (int i = re->sp[0]; i <= re->sp[2]; i++) {
					class = class_add_char(re, class, i);

					if (re->popt & KTRE_INSENSITIVE) {
						if (i >= 'A' && i <= 'Z')
							class = class_add_char(re, class, lc(i));
					}
				}
				re->sp += 3;
			} else if (*re->sp == '\\') {
				next_char(re);

				switch (*re->sp) {
				case 's':
					class = class_add_str(re, class, WHITESPACE);
					re->sp++;
					break;
				case 'd':
					class = class_add_str(re, class, DIGIT);
					re->sp++;
					break;
				case 'w':
					class = class_add_str(re, class, WORD);
					re->sp++;
					break;
				case 'n':
					class = class_add_char(re, class, '\n');
					re->sp++;
				}
			} else {
				class = class_add_char(re, class, *re->sp);

				if (re->popt & KTRE_INSENSITIVE
				    && *re->sp >= 'A' && *re->sp <= 'Z') {
					class = class_add_char(re, class, lc(*re->sp));
				}

				re->sp++;
			}
		}

		if (*re->sp != ']') {
			error(re, KTRE_ERROR_SYNTAX_ERROR, loc, "unterminated character class");
			free_node(re, left);
			return NULL;
		}

		if (!class) {
			error(re, KTRE_ERROR_SYNTAX_ERROR, loc, "empty character class");
			free_node(re, left);
			return NULL;
		}

		left->class = class;
		next_char(re);
	} break;

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

	case '#': /* extended mode comments */
		if (re->popt & KTRE_EXTENDED) {
			while (*re->sp && *re->sp != '\n') { next_char(re); }
		} else {
			left->type = NODE_CHAR;
			left->c = *re->sp;
			next_char(re);
		}
		break;

	default:
		/* ignore whitespace if we're in extended mode */
		if (re->popt & KTRE_EXTENDED && strchr(WHITESPACE, *re->sp)) {
			while (*re->sp && strchr(WHITESPACE, *re->sp))
				next_char(re);
			goto again;
		}

		left->type = NODE_CHAR;
		left->c = *re->sp;
		next_char(re);
	}

	if (left) left->loc = re->sp - re->pat;
	return left;
}

static struct node *
factor(struct ktre *re)
{
	if (!*re->sp) return NULL;

	struct node *left = parse_primary(re);

	while (*re->sp && (*re->sp == '*' || *re->sp == '+' || *re->sp == '?' || *re->sp == '{')) {
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
				if (isdigit(*re->sp))
					n->d = parse_dec_num(re);
				else
					n->d = -1;
			}

			if (*re->sp != '}') {
				error(re, KTRE_ERROR_SYNTAX_ERROR, re->sp - re->pat - 1, "unmatched '{'");
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

	if (left)
		left->loc = re->sp - re->pat - 1;

	return left;
}

static struct node *
term(struct ktre *re)
{
	struct node *left = new_node(re);
	left->type = NODE_NONE;
	left->loc = re->sp - re->pat;

	while (*re->sp && *re->sp != '|' && *re->sp != ')') {
		struct node *right = factor(re);

		if (re->err) {
			free_node(re, left);
			free_node(re, right);
			return NULL;
		}

		if (left->type == NODE_NONE) {
			ktre__free(re, left);
			left = right;
		} else {
			if ((left->type == NODE_CHAR || left->type == NODE_STR) && right->type == NODE_CHAR) {
				if (left->type == NODE_CHAR) {
					char a = left->c;
					left->type = NODE_STR;
					left->class = ktre__malloc(re, 3);
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
					if (re->popt & KTRE_INSENSITIVE) {
						left->class = class_add_char(re, left->class, lc(right->c));
					} else {
						left->class = class_add_char(re, left->class, right->c);
					}

					free_node(re, right);
				}
			} else if (right->type == NODE_CHAR && left->type == NODE_SEQUENCE
				   && (left->b->type == NODE_CHAR || left->b->type == NODE_STR)) {
				if (left->b->type == NODE_CHAR) {
					char a = left->b->c;
					left->b->type = NODE_STR;
					left->b->class = ktre__malloc(re, 3);
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
					if (re->popt & KTRE_INSENSITIVE) {
						left->b->class = class_add_char(re, left->b->class, lc(right->c));
					} else {
						left->b->class = class_add_char(re, left->b->class, right->c);
					}

					free_node(re, right);
				}
			} else {
				struct node *tmp = new_node(re);
				tmp->loc = re->sp - re->pat;
				tmp->a = left;
				tmp->b = right;
				tmp->type = NODE_SEQUENCE;
				left = tmp;
			}
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
		m->loc = re->sp - re->pat;

		if (re->err) {
			free_node(re, m->b);
			m->b = NULL;
			return m;
		}

		return m;
	}

	return n;
}

#ifdef KTRE_DEBUG
static void
print_node(struct node *n)
{
#define join arm[l - 1] = 0
#define split arm[l - 1] = 1
	static int l = 0, arm[2048] = { 0 };
	DBG("\n");
	arm[l] = 0;

	for (int i = 0; i < l - 1; i++) {
		if (arm[i]) DBG("|   ");
		else DBG("    ");
	}

	if (l) {
		if (arm[l - 1]) DBG("|-- ");
		else DBG("`-- ");
	}

#define N1(...)                                               \
	do {                                                  \
		DBG(__VA_ARGS__); l++; print_node(n->a); l--; \
	} while(0);

#define N2(...)                         \
	do {                            \
		DBG(__VA_ARGS__);       \
		l++; split;             \
		print_node(n->a); join; \
		print_node(n->b);       \
		l--;                    \
	} while(0);

	if (!n) {
		DBG("(null)");
		return;
	}

	switch (n->type) {
	case NODE_ANY:       DBG("(any)");                                    break;
	case NODE_DIGIT:     DBG("(digit)");                                  break;
	case NODE_WORD:      DBG("(word)");                                   break;
	case NODE_SPACE:     DBG("(space)");                                  break;
	case NODE_NONE:      DBG("(none)");                                   break;
	case NODE_CHAR:      DBG("(char '%c')", n->c);                        break;
	case NODE_WB:        DBG("(word boundary)");                          break;
	case NODE_BACKREF:   DBG("(backreference to %d)", n->c);              break;
	case NODE_CLASS:     DBG("(class '"); dbgf(n->class); DBG("')");      break;
	case NODE_STR:       DBG("(string '"); dbgf(n->class); DBG("')");     break;
	case NODE_NOT:       DBG("(not '%s')", n->class);                     break;
	case NODE_BOL:       DBG("(bol)");                                    break;
	case NODE_EOL:       DBG("(eol)");                                    break;
	case NODE_RECURSE:   DBG("(recurse)");                                break;
	case NODE_SET_START: DBG("(set_start)");                              break;
	case NODE_OPTON:     DBG("(opton %d)", n->c);                         break;
	case NODE_OPTOFF:    DBG("(optoff %d)", n->c);                        break;
	case NODE_CALL:      DBG("(call %d)", n->c);                          break;
	case NODE_SEQUENCE:  N2 ("(sequence)");                               break;
	case NODE_OR:        N2 ("(or)");                                     break;
	case NODE_REP:       N1 ("(counted repetition %d - %d)", n->c, n->d); break;
	case NODE_ASTERISK:  N1 ("(asterisk)");                               break;
	case NODE_PLUS:      N1 ("(plus)");                                   break;
	case NODE_GROUP:     N1 ("(group)");                                  break;
	case NODE_QUESTION:  N1 ("(question)");                               break;
	case NODE_ATOM:      N1 ("(atom)");                                   break;
	case NODE_PLA:       N1 ("(lookahead)");                              break;
	case NODE_NLA:       N1 ("(negative lookahead)");                     break;
	case NODE_PLB:       N1 ("(lookbehind)");                             break;
	case NODE_NLB:       N1 ("(negative lookbehind)");                    break;

	default:
		DBG("\nunimplemented printer for node of type %d\n", n->type);
		assert(false);
	}
}
#endif

static void
compile(struct ktre *re, struct node *n, bool rev)
{
#define PATCH_A(loc, _a) if (re->c) re->c[loc].a = _a
#define PATCH_B(loc, _b) if (re->c) re->c[loc].b = _b
#define PATCH_C(loc, _c) if (re->c) re->c[loc].c = _c
	int a = -1, b = -1, old = -1;

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
		if (re->group[n->gi].is_called) {
			emit_c(re, INSTR_CALL, re->ip + 3, n->loc);
			emit_c(re, INSTR_SAVE, re->num_groups * 2 + 1, n->loc);
			a = re->ip;
			emit_c(re, INSTR_JMP, -1, n->loc);
			emit_c(re, INSTR_SAVE, re->num_groups * 2, n->loc);

			old = re->num_groups;
			re->group[re->num_groups++].address = re->ip - 1;

			compile(re, n->a, rev);

			emit(re, INSTR_RET, n->loc);
			PATCH_C(a, re->ip);

			re->group[old].is_compiled = true;
		} else {
			emit_c(re, INSTR_SAVE, re->num_groups * 2, n->loc);

			old = re->num_groups;
			re->num_groups++;
			re->group[old].address = re->ip - 1;

			compile(re, n->a, rev);

			emit_c(re, INSTR_SAVE, old * 2 + 1, n->loc);

			re->group[old].is_compiled = true;
		}
		break;

	case NODE_CALL:
		if (n->c <= 0 || n->c >= re->num_groups) {
			error(re, KTRE_ERROR_SYNTAX_ERROR, n->loc, "subroutine number is invalid or calls a group that does not yet exist");
			return;
		}

		emit_c(re, INSTR_CALL, re->group[n->c].address + 1, n->loc);
		break;

	case NODE_PLUS:
		switch (n->a->type) {
		case NODE_ASTERISK: /* fall through */
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
			error(re, KTRE_ERROR_SYNTAX_ERROR, n->loc, "backreference number is invalid or references a group that does not yet exist");
			return;
		}

		if (!re->group[n->c].is_compiled) {
			error(re, KTRE_ERROR_SYNTAX_ERROR, n->loc, "backreferences may not reference the group they occur in");
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
					char *str = ktre__malloc(re, n->c + 1);
					if (!str) return;
					re->info.parser_alloc += n->c + 1;

					for (int i = 0; i < n->c; i++) str[i] = n->a->c;
					str[n->c] = 0;
					emit_class(re, INSTR_TSTR, str, n->loc);
					break;
				} else compile(re, n->a, rev);
			}
		}

		if (n->d == -1) {
			if (n->a->type == NODE_GROUP) {
				/* basically just manually emit the
				 * bytecode for * */
				emit_ab(re, INSTR_BRANCH, re->ip + 1, re->ip + 2, n->loc);
				emit_c(re, INSTR_CALL, re->group[n->a->gi].address + 1, n->loc);
				emit_ab(re, INSTR_BRANCH, re->ip - 1, re->ip + 1, n->loc);
			} else {
				emit_ab(re, INSTR_BRANCH, a, re->ip + 1, n->loc);
			}
		} else {
			for (int i = 0; i < n->d - n->c; i++) {
				a = re->ip;
				emit_ab(re, INSTR_BRANCH, re->ip + 1, -1, n->loc);

				if (n->a->type == NODE_GROUP) {
					emit_c(re, INSTR_CALL, re->group[n->a->gi].address + 1, n->loc);
				} else {
					compile(re, n->a, rev);
				}

				PATCH_B(a, re->ip);
			}
		}
		break;

	case NODE_ATOM:
		emit(re, INSTR_TRY, n->loc);
		compile(re, n->a, rev);
		emit(re, INSTR_CATCH, n->loc);
		break;

	case NODE_PLA:
		emit(re, INSTR_PLA, n->loc);
		compile(re, n->a, false);
		emit(re, INSTR_PLA_WIN, n->loc);
		break;

	case NODE_NLA:
		a = re->ip;
		emit(re, INSTR_NLA, n->loc);
		compile(re, n->a, false);
		emit(re, INSTR_NLA_FAIL, n->loc);
		PATCH_C(a, re->ip);
		break;

	case NODE_PLB:
		emit(re, INSTR_PLB, n->loc);
		compile(re, n->a, true);
		emit(re, INSTR_PLB_WIN, n->loc);
		break;

	case NODE_NLB:
		a = re->ip;
		emit_c(re, INSTR_NLB, -1, n->loc);
		compile(re, n->a, true);
		emit(re, INSTR_NLB_FAIL, n->loc);
		PATCH_C(a, re->ip);
		break;

	case NODE_CLASS:     emit_class(re, INSTR_CLASS,  n->class, n->loc); break;
	case NODE_STR:       emit_class(re, INSTR_STR,    n->class, n->loc); break;
	case NODE_NOT:       emit_class(re, INSTR_NOT,    n->class, n->loc); break;
	case NODE_OPTON:     emit_c    (re, INSTR_OPTON,  n->c,     n->loc); break;
	case NODE_OPTOFF:    emit_c    (re, INSTR_OPTOFF, n->c,     n->loc); break;
	case NODE_CHAR:      emit_c    (re, INSTR_CHAR,   n->c,     n->loc); break;
	case NODE_BOL:       emit      (re, INSTR_BOL,              n->loc); break;
	case NODE_EOL:       emit      (re, INSTR_EOL,              n->loc); break;
	case NODE_ANY:       emit      (re, INSTR_ANY,              n->loc); break;
	case NODE_SET_START: emit      (re, INSTR_SET_START,        n->loc); break;
	case NODE_WB:        emit      (re, INSTR_WB,               n->loc); break;
	case NODE_DIGIT:     emit      (re, INSTR_DIGIT,            n->loc); break;
	case NODE_WORD:      emit      (re, INSTR_WORD,             n->loc); break;
	case NODE_SPACE:     emit      (re, INSTR_SPACE,            n->loc); break;
	case NODE_NONE:                                                      break;

	default:
#ifdef KTRE_DEBUG
		DBG("\nunimplemented compiler for node of type %d\n", n->type);
		assert(false);
#endif
		break;
	}
}

#ifdef KTRE_DEBUG
static void
print_compile_error(struct ktre *re)
{
	DBG("\nfailed to compile with error code %d: %s\n",
		        re->err, re->err_str ? re->err_str : "no error message");
	DBG("\t%s\n\t", re->pat);
	for (int i = 0; i < re->loc; i++)
		DBG(" ");
	DBG("^");
}
#endif

struct ktre *
ktre_compile(const char *pat, int opt)
{
	struct ktre *re = KTRE_MALLOC(sizeof *re);
	memset(re, 0, sizeof *re);

#ifdef KTRE_DEBUG
	DBG("regexpr: %s", pat);
	if (opt) DBG("\noptions:");
	for (size_t i = 0; i < sizeof opt; i++) {
		switch (opt & 1 << i) {
		case KTRE_INSENSITIVE: DBG("\n\tINSENSITIVE"); break;
		case KTRE_UNANCHORED:  DBG("\n\tUNANCHORED");  break;
		case KTRE_EXTENDED:    DBG("\n\tEXTENDED");    break;
		case KTRE_GLOBAL:      DBG("\n\tGLOBAL");      break;
		default: continue;
		}
	}
#endif

	re->pat                  = pat;
	re->opt                  = opt;
	re->sp                   = pat;
	re->popt                 = opt;
	re->max_tp               = -1;
	re->err_str              = "no error";
	re->n                    = new_node(re);
	re->n->loc               = 0;
	re->n->type              = NODE_GROUP;
	re->n->gi                = add_group(re);
	re->group[0].is_compiled = false;
	re->group[0].is_called   = false;
	re->n->a                 = NULL;
	struct node *n           = parse(re);
	re->n->a                 = n;

	if (re->err) {
		free_node(re, re->n);
		re->n = NULL;
#ifdef KTRE_DEBUG
		print_compile_error(re);
#endif
		return re;
	}

	if (*re->sp) {
		error(re, KTRE_ERROR_SYNTAX_ERROR, re->sp - re->pat, "invalid regex syntax; unmatched righthand delimiter");
#ifdef KTRE_DEBUG
		print_compile_error(re);
#endif
		return re;
	}

#ifdef KTRE_DEBUG
	print_node(re->n);
#endif

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
		emit(re, INSTR_ANY, 0);
		emit_ab(re, INSTR_BRANCH, 3, 1, 0);
	}

	compile(re, re->n, false);

	if (re->err) {
#ifdef KTRE_DEBUG
		print_compile_error(re);
#endif
		return re;
	}

	emit(re, INSTR_MATCH, re->sp - re->pat);

#ifdef KTRE_DEBUG

	for (int i = 0; i < re->ip; i++) {
		for (int j = 0; j < re->num_groups; j++) {
			if (re->group[j].address == i)
				DBG("\ngroup %d:", j);
		}

		DBG("\n%3d: [%3d] ", i, re->c[i].loc);

		switch (re->c[i].op) {
		case INSTR_CLASS: DBG("CLASS   '"); dbgf(re->c[i].class); DBG("'");    break;
		case INSTR_STR:   DBG("STR     '"); dbgf(re->c[i].class); DBG("'");    break;
		case INSTR_TSTR:  DBG("TSTR    '"); dbgf(re->c[i].class); DBG("'");    break;
		case INSTR_BRANCH:    DBG("BRANCH    %d, %d", re->c[i].a, re->c[i].b); break;
		case INSTR_NOT:       DBG("NOT     '%s'", re->c[i].class);             break;
		case INSTR_CHAR:      DBG("CHAR    '%c'", re->c[i].a);                 break;
		case INSTR_SAVE:      DBG("SAVE     %d",  re->c[i].a);                 break;
		case INSTR_JMP:       DBG("JMP      %d",  re->c[i].a);                 break;
		case INSTR_OPTON:     DBG("OPTON    %d",  re->c[i].a);                 break;
		case INSTR_OPTOFF:    DBG("OPTOFF   %d",  re->c[i].a);                 break;
		case INSTR_BACKREF:   DBG("BACKREF  %d",  re->c[i].a);                 break;
		case INSTR_CALL:      DBG("CALL     %d",  re->c[i].a);                 break;
		case INSTR_PROG:      DBG("PROG     %d",  re->c[i].a);                 break;
		case INSTR_SET_START: DBG("SET_START");                                break;
		case INSTR_TRY:       DBG("TRY");                                      break;
		case INSTR_CATCH:     DBG("CATCH");                                    break;
		case INSTR_ANY:       DBG("ANY");                                      break;
		case INSTR_DIGIT:     DBG("DIGIT");                                    break;
		case INSTR_WORD:      DBG("WORD");                                     break;
		case INSTR_SPACE:     DBG("SPACE");                                    break;
		case INSTR_BOL:       DBG("BOL");                                      break;
		case INSTR_EOL:       DBG("EOL");                                      break;
		case INSTR_RET:       DBG("RET");                                      break;
		case INSTR_WB:        DBG("WB");                                       break;
		case INSTR_MATCH:     DBG("MATCH");                                    break;
		case INSTR_PLA:       DBG("PLA");                                      break;
		case INSTR_PLA_WIN:   DBG("PLA_WIN");                                  break;
		case INSTR_NLA:       DBG("NLA      %d",  re->c[i].a);                 break;
		case INSTR_NLA_FAIL:  DBG("NLA_FAIL");                                 break;
		case INSTR_PLB:       DBG("PLB");                                      break;
		case INSTR_PLB_WIN:   DBG("PLB_WIN");                                  break;
		case INSTR_NLB:       DBG("NLB      %d",  re->c[i].a);                 break;
		case INSTR_NLB_FAIL:  DBG("NLB_FAIL");                                 break;

		default:
 			DBG("\nunimplemented instruction printer %d\n", re->c[i].op);
			assert(false);
		}
	}
#endif

	return re;
}

struct ktre *ktre_copy(struct ktre *re)
{
	struct ktre *ret = KTRE_MALLOC(sizeof *ret);
	memset(ret, 0, sizeof *ret);
	ret->c = re->c;
	re->copied = true;
	return ret;
}

#define TP (re->tp)
#define THREAD (re->t)

static void
new_thread(struct ktre *re, int ip, int sp, int opt, int fp, int la, int e)
{
	++TP;

	if (TP >= re->info.thread_alloc - 1) {
		if (re->info.thread_alloc * 2 >= KTRE_MAX_THREAD) {
			re->info.thread_alloc = KTRE_MAX_THREAD;

			/*
			 * Account for the case where we're just about
			 * to bump up against the thread limit.
			 */
			TP = TP >= KTRE_MAX_THREAD ? KTRE_MAX_THREAD - 1 : TP;
		} else
			re->info.thread_alloc *= 2;

		re->t = ktre__realloc(re, re->t, re->info.thread_alloc * sizeof THREAD[0]);
		memset(&THREAD[TP], 0, (re->info.thread_alloc - TP) * sizeof THREAD[0]);
	}

	if (!THREAD[TP].vec) {
		THREAD[TP].vec = ktre__malloc(re, re->num_groups * 2 * sizeof THREAD[TP].vec[0]);
		memset(THREAD[TP].vec, 0,         re->num_groups * 2 * sizeof THREAD[TP].vec[0]);
		re->info.runtime_alloc +=         re->num_groups * 2 * sizeof THREAD[TP].vec[0];
	}

	if (!THREAD[TP].prog && re->num_prog) {
		THREAD[TP].prog = ktre__malloc(re, re->num_prog * sizeof THREAD[TP].prog[0]);
		memset(THREAD[TP].prog, -1,        re->num_prog * sizeof THREAD[TP].prog[0]);
		re->info.runtime_alloc +=          re->num_prog * sizeof THREAD[TP].prog[0];
	}

	if (!THREAD[TP].frame) {
		THREAD[TP].frame = ktre__malloc(re, (fp + 1) * sizeof THREAD[TP].frame[0]);
		memset(THREAD[TP].frame, -1,        (fp + 1) * sizeof THREAD[TP].frame[0]);
		re->info.runtime_alloc +=           (fp + 1) * sizeof THREAD[TP].frame[0];
	} else if (THREAD[TP].fp < fp) {
		THREAD[TP].frame = ktre__realloc(re, THREAD[TP].frame, (fp + 1) * sizeof THREAD[TP].frame[0]);
	}

	if (!THREAD[TP].las) {
		THREAD[TP].las = ktre__malloc(re, (la + 1) * sizeof THREAD[TP].las[0]);
		memset(THREAD[TP].las, -1,        (la + 1) * sizeof THREAD[TP].las[0]);
		re->info.runtime_alloc +=         (la + 1) * sizeof THREAD[TP].las[0];
	} else if (THREAD[TP].la < la) {
		THREAD[TP].las = ktre__realloc(re, THREAD[TP].las, (la + 1) * sizeof THREAD[TP].las[0]);
	}

	if (!THREAD[TP].exception) {
		THREAD[TP].exception = ktre__malloc(re, (e + 1) * sizeof THREAD[TP].exception[0]);
		memset(THREAD[TP].exception, -1,        (e + 1) * sizeof THREAD[TP].exception[0]);
		re->info.runtime_alloc +=               (e + 1) * sizeof THREAD[TP].exception[0];
	} else if (THREAD[TP].e < e) {
		THREAD[TP].exception = ktre__realloc(re, THREAD[TP].exception, (e + 1) * sizeof THREAD[TP].exception[0]);
	}

	THREAD[TP].e   = e;
	THREAD[TP].ip  = ip;
	THREAD[TP].sp  = sp;
	THREAD[TP].fp  = fp;
	THREAD[TP].la  = la;
	THREAD[TP].opt = opt;

	if (TP > 0) {
		memcpy(THREAD[TP].frame,
		       THREAD[TP - 1].frame,
		       THREAD[TP - 1].fp > fp
		       ? fp * sizeof THREAD[0].frame[0]
		       : THREAD[TP - 1].fp * sizeof THREAD[0].frame[0]);

		memcpy(THREAD[TP].las,
		       THREAD[TP - 1].las,
		       THREAD[TP - 1].la > la
		       ? la * sizeof THREAD[0].las[0]
		       : THREAD[TP - 1].la * sizeof THREAD[0].las[0]);

		memcpy(THREAD[TP].exception,
		       THREAD[TP - 1].exception,
		       THREAD[TP - 1].e > e
		       ? e * sizeof THREAD[0].exception[0]
		       : THREAD[TP - 1].e * sizeof THREAD[0].exception[0]);

		memcpy(THREAD[TP].vec,
		       THREAD[TP - 1].vec,
		       re->num_groups * 2 * sizeof THREAD[0].vec[0]);

		memcpy(THREAD[TP].prog,
		       THREAD[TP - 1].prog,
		       re->num_prog * sizeof THREAD[0].prog[0]);
	}

	re->max_tp = TP > re->max_tp ? TP : re->max_tp;
}

static bool
run(struct ktre *re, const char *subject, int ***vec)
{
	if (*vec) {
		for (int i = 0; i < re->num_matches; i++) {
			ktre__free(re, (*vec)[i]);
		}

		ktre__free(re, *vec);
	}

	*vec = NULL;
	re->vec = NULL;
	re->num_matches = 0;
	TP = -1;

	if (!re->info.thread_alloc) {
		re->info.thread_alloc = 25;
		re->t = ktre__malloc(re, re->info.thread_alloc * sizeof THREAD[0]);
		if (re->err) return false;
		memset(re->t, 0, re->info.thread_alloc * sizeof THREAD[0]);
	}

	char *subject_lc = ktre__malloc(re, strlen(subject) + 1);
	for (int i = 0; i <= (int)strlen(subject); i++)
		subject_lc[i] = lc(subject[i]);

	/* push the initial thread */
	new_thread(re, 0, 0, re->opt, 0, 0, 0);

#ifdef KTRE_DEBUG
	int num_steps = 0;
	DBG("\n|   ip |   sp |   tp |   fp | step |");
#endif

	while (TP >= 0) {
		int ip   = THREAD[TP].ip;
		int sp   = THREAD[TP].sp;
		int fp   = THREAD[TP].fp;
		int la   = THREAD[TP].la;
		int e    = THREAD[TP].e;
		int opt  = THREAD[TP].opt;
		int loc  = re->c[ip].loc;
		bool rev = THREAD[TP].rev;

#ifdef KTRE_DEBUG
		DBG("\n| %4d | %4d | %4d | %4d | %4d | ", ip, sp, TP, fp, num_steps);
		if (sp >= 0) DBG("%s", subject + sp);
		else DBG("%s", subject);
#endif

		if (THREAD[TP].die) {
			THREAD[TP].die = false;
			--TP; continue;
		}

		switch (re->c[ip].op) {
		case INSTR_BACKREF:
			THREAD[TP].ip++;

			if (rev) {
				if (opt & KTRE_INSENSITIVE) {
					for (int i = 0; i < THREAD[TP].vec[re->c[ip].c * 2 + 1]; i++) {
						if (subject_lc[sp - i]
						    != lc(subject[THREAD[TP].vec[re->c[ip].c * 2] + sp - i])) {
							--TP;
							continue;
						}
					}

					THREAD[TP].sp -= THREAD[TP].vec[re->c[ip].c * 2 + 1];
				} else {
					if (!strncmp(subject + sp + 1 - THREAD[TP].vec[re->c[ip].c * 2 + 1],
					             &subject[THREAD[TP].vec[re->c[ip].c * 2]],
					             THREAD[TP].vec[re->c[ip].c * 2 + 1]))
						THREAD[TP].sp += THREAD[TP].vec[re->c[ip].c * 2 + 1];
					else
						--TP;
				}
			} else {
				if (opt & KTRE_INSENSITIVE) {
					for (int i = 0; i < THREAD[TP].vec[re->c[ip].c * 2 + 1]; i++) {
						if (subject_lc[sp + i] != lc(subject[THREAD[TP].vec[re->c[ip].c * 2] + i])) {
							--TP;
							continue;
						}
					}

					THREAD[TP].sp += THREAD[TP].vec[re->c[ip].c * 2 + 1];
				} else {
					if (!strncmp(subject + sp,
					             &subject[THREAD[TP].vec[re->c[ip].c * 2]],
					             THREAD[TP].vec[re->c[ip].c * 2 + 1]))
						THREAD[TP].sp += THREAD[TP].vec[re->c[ip].c * 2 + 1];
					else
						--TP;
				}
			}
			break;

		case INSTR_CLASS:
			THREAD[TP].ip++;

			if (!(subject[sp] && sp >= 0)) {
				--TP;
				continue;
			}

			if (strchr(re->c[ip].class, subject[sp]))
				THREAD[TP].sp++;
			else if (opt & KTRE_INSENSITIVE && strchr(re->c[ip].class, subject_lc[sp]))
				THREAD[TP].sp++;
			else
				--TP;
			break;

		case INSTR_STR: case INSTR_TSTR:
			THREAD[TP].ip++;

			if (rev) {
				if (opt & KTRE_INSENSITIVE) {
					if (!strncmp(subject_lc + sp + 1 - strlen(re->c[ip].class), re->c[ip].class, strlen(re->c[ip].class)))
						THREAD[TP].sp -= strlen(re->c[ip].class);
					else
						--TP;
				} else {
					if (!strncmp(subject + sp + 1 - strlen(re->c[ip].class), re->c[ip].class, strlen(re->c[ip].class)))
						THREAD[TP].sp -= strlen(re->c[ip].class);
					else
						--TP;
				}
			} else {
				if (opt & KTRE_INSENSITIVE) {
					if (!strncmp(subject_lc + sp, re->c[ip].class, strlen(re->c[ip].class)))
						THREAD[TP].sp += strlen(re->c[ip].class);
					else
						--TP;
				} else {
					if (!strncmp(subject + sp, re->c[ip].class, strlen(re->c[ip].class)))
						THREAD[TP].sp += strlen(re->c[ip].class);
					else
						--TP;
				}
			}
			break;

		case INSTR_NOT:
			THREAD[TP].ip++;

			if (!strchr(re->c[ip].class, subject[sp]) && subject[sp] && sp >= 0)
				THREAD[TP].sp++;
			else
				--TP;
			break;

		case INSTR_BOL:
			if (sp == 0) THREAD[TP].ip++;
			else --TP;
			break;

		case INSTR_EOL:
			if ((subject[sp] == 0 || subject[sp] == '\n') && sp >= 0)
				THREAD[TP].ip++;
			else
				--TP;
			break;

		case INSTR_WB:
			if (sp >= 0 &&
			    ((strchr(WORD, subject[sp - 1]) && !strchr(WORD, subject[sp]))
			     || (!strchr(WORD, subject[sp - 1]) && strchr(WORD, subject[sp]))))
				THREAD[TP].ip++;
			else if (sp == 0 || sp == (int)strlen(subject))
				THREAD[TP].ip++;
			else
				--TP;
			break;

		case INSTR_CHAR:
			THREAD[TP].ip++;

			if ((sp >= 0 && ((opt & KTRE_INSENSITIVE) && lc(subject[sp]) == lc(re->c[ip].c)))
			    || subject[sp] == re->c[ip].c)
				if (rev) THREAD[TP].sp--; else THREAD[TP].sp++;
			else
				--TP;
			break;

		case INSTR_ANY:
			THREAD[TP].ip++;

			if (subject[sp] && sp >= 0)
				if (rev) THREAD[TP].sp--; else THREAD[TP].sp++;
			else
				--TP;
			break;

		case INSTR_BRANCH:
			THREAD[TP].ip = re->c[ip].b;
			new_thread(re, re->c[ip].a, sp, opt, fp, la, e);
			break;

		case INSTR_MATCH:
			if ((opt & KTRE_UNANCHORED) != 0 || (sp >= 0 && !subject[sp])) {
				*vec = ktre__realloc(re, *vec, (re->num_matches + 1) * sizeof *vec);
				(*vec)[re->num_matches] = ktre__malloc(re, re->num_groups * 2 * sizeof *vec[0]);

				memcpy((*vec)[re->num_matches++],
				       THREAD[TP].vec,
				       re->num_groups * 2 * sizeof *vec[0][0]);

				re->vec = *vec;

				if ((opt & KTRE_GLOBAL) == 0) {
					ktre__free(re, subject_lc);
					return true;
				} else {
					TP = 0;
					THREAD[TP].ip = 0;
					THREAD[TP].sp = sp;

					if (re->num_matches - 1 && sp == (*vec)[0][0] + (*vec)[0][1]) {
						ktre__free(re, subject_lc);
						return true;
					}

					continue;
				}
			}

			--TP;
			break;

		case INSTR_SAVE:
			THREAD[TP].ip++;

			if (re->c[ip].c % 2 == 0)
				THREAD[TP].vec[re->c[ip].c] = sp;
			else
				THREAD[TP].vec[re->c[ip].c] = sp - THREAD[TP].vec[re->c[ip].c - 1];
			break;

		case INSTR_JMP:
			THREAD[TP].ip = re->c[ip].c;
			break;

		case INSTR_OPTON:
			THREAD[TP].ip++;
			THREAD[TP].opt |= re->c[ip].c;
			break;

		case INSTR_OPTOFF:
			THREAD[TP].ip++;
			THREAD[TP].opt &= ~re->c[ip].c;
			break;

		case INSTR_SET_START:
			THREAD[TP].ip++;
			THREAD[TP].vec[0] = sp;
			break;

		case INSTR_CALL:
			THREAD[TP].ip = re->c[ip].c;
			THREAD[TP].frame = ktre__realloc(re, THREAD[TP].frame, (fp + 1) * sizeof THREAD[TP].frame[0]);
			THREAD[TP].frame[THREAD[TP].fp++] = ip + 1;
			break;

		case INSTR_RET:
			THREAD[TP].ip = THREAD[TP].frame[--THREAD[TP].fp];
			break;

		case INSTR_PROG:
			THREAD[TP].ip++;

			if (THREAD[TP].prog[re->c[ip].c] == sp)
				--TP;
			else
				THREAD[TP].prog[re->c[ip].c] = sp;

			break;

		case INSTR_DIGIT:
			THREAD[TP].ip++;

			if (strchr(DIGIT, subject[sp]) && subject[sp]) {
				if (rev) THREAD[TP].sp--; else THREAD[TP].sp++;
			} else --TP;
			break;

		case INSTR_WORD:
			THREAD[TP].ip++;

			if (strchr(WORD, subject[sp]) && subject[sp]) {
				if (rev) THREAD[TP].sp--; else THREAD[TP].sp++;
			} else --TP;
			break;

		case INSTR_SPACE:
			THREAD[TP].ip++;

			if (strchr(WHITESPACE, subject[sp]) && subject[sp]) {
				if (rev) THREAD[TP].sp--; else THREAD[TP].sp++;
			} else
				--TP;
			break;

		case INSTR_TRY:
			THREAD[TP].ip++;
			THREAD[TP].exception = ktre__realloc(re, THREAD[TP].exception, (e + 1) * sizeof THREAD[TP].exception[0]);
			THREAD[TP].exception[THREAD[TP].e++] = TP;
			break;

		case INSTR_CATCH:
			TP = THREAD[TP].exception[e - 1];
			THREAD[TP].ip = ip + 1;
			THREAD[TP].sp = sp;
			break;

		case INSTR_PLB:
			THREAD[TP].die = true;
			new_thread(re, ip + 1, sp - 1, opt, fp, la, e + 1);
			THREAD[TP].exception[e] = TP - 1;
			THREAD[TP].rev = true;
			break;

		case INSTR_PLB_WIN:
			TP = THREAD[TP].exception[--THREAD[TP].e];
			THREAD[TP].rev = false;
			THREAD[TP].die = false;
			THREAD[TP].ip = ip + 1;
			break;

		case INSTR_NLB:
			THREAD[TP].ip = re->c[ip].c;
			new_thread(re, ip + 1, sp - 1, opt, fp, la, e + 1);
			THREAD[TP].exception[e] = TP - 1;
			THREAD[TP].rev = true;
			break;

		case INSTR_NLB_FAIL:
			TP = THREAD[TP].exception[--THREAD[TP].e] - 1;
			break;

		case INSTR_PLA:
			THREAD[TP].die = true;
			new_thread(re, ip + 1, sp, opt, fp, la, e + 1);
			THREAD[TP].exception[e] = TP - 1;
			break;

		case INSTR_PLA_WIN:
			TP = THREAD[TP].exception[--THREAD[TP].e];
			THREAD[TP].die = false;
			THREAD[TP].ip = ip + 1;
			break;

		case INSTR_NLA:
			THREAD[TP].ip = re->c[ip].a;
			new_thread(re, ip + 1, sp, opt, fp, la, e + 1);
			THREAD[TP].exception[e] = TP - 1;
			break;

		case INSTR_NLA_FAIL:
			TP = THREAD[TP].exception[--THREAD[TP].e] - 1;
			break;

		default:
#ifdef KTRE_DEBUG
			DBG("\nunimplemented instruction %d\n", re->c[ip].op);
			assert(false);
#endif
			ktre__free(re, subject_lc);
			return false;
		}

		if (TP >= KTRE_MAX_THREAD - 1) {
			error(re, KTRE_ERROR_STACK_OVERFLOW, loc, "regex exceeded the maximum number of executable threads");
			ktre__free(re, subject_lc);
			return false;
		}

		if (fp >= KTRE_MAX_CALL_DEPTH - 1) {
			error(re, KTRE_ERROR_CALL_OVERFLOW, loc, "regex exceeded the maximum depth for subroutine calls");
			ktre__free(re, subject_lc);
			return false;
		}

#ifdef KTRE_DEBUG
		num_steps++;
#endif
	}

	ktre__free(re, subject_lc);
	return !!re->num_matches;
}

struct ktre_info
ktre_free(struct ktre *re)
{
	if (re->copied)
		return re->info;

	free_node(re, re->n);
	if (re->err && re->err_str)
		ktre__free(re, re->err_str);

	for (int i = 0; i < re->ip; i++) {
		if (re->c[i].op == INSTR_TSTR)
			ktre__free(re, re->c[i].class);
	}

	ktre__free(re, re->c);

	for (int i = 0; i <= re->max_tp; i++) {
		ktre__free(re, THREAD[i].vec);
		ktre__free(re, THREAD[i].prog);
		ktre__free(re, THREAD[i].frame);
		ktre__free(re, THREAD[i].las);
		ktre__free(re, THREAD[i].exception);
	}

	if (re->vec) {
		for (int i = 0; i < re->num_matches; i++) {
			ktre__free(re, (re->vec)[i]);
		}

		ktre__free(re, re->vec);
	}

	ktre__free(re, re->t);
	struct ktre_info info = re->info;

#if defined(_MSC_VER) && defined(KTRE_DEBUG)
	_CrtDumpMemoryLeaks();
#endif

#ifdef KTRE_DEBUG
	DBG("\nFinished with %d allocations, %d frees, and %zd bytes allocated.",
	    info.num_alloc, info.num_free,
	    info.mba + sizeof (struct ktre));

	DBG("\n%5d bytes were allocated for the main structure.",
	    sizeof (struct ktre));
	DBG("\n%5d bytes were allocated for the bytecode.",
	    info.instr_alloc * sizeof (struct instr));
	DBG("\n%5d bytes were allocated for the runtime.",
	    info.runtime_alloc + info.thread_alloc * sizeof (struct thread));
	DBG("\n%5d bytes were allocated for the parser.",
	    info.parser_alloc);

	if (info.ba) {
		DBG("\nThere were %d leaked bytes from %d unmatched allocations.",
		    info.ba - (info.num_alloc - info.num_free) * sizeof (struct ktre_minfo),
		    info.num_alloc - info.num_free);

		struct ktre_minfo *mi = re->minfo;
		int i = 0;

		while (mi->next) mi = mi->next;

		while (mi) {
			DBG("\n\tleak %d: %d bytes at %s:%d",
			    i + 1, mi->size, mi->file, mi->line);
			mi = mi->prev;
			i++;
		}
	}
#endif
	KTRE_FREE(re);

	return info;
}

_Bool
ktre_exec(struct ktre *re, const char *subject, int ***vec)
{
#ifdef KTRE_DEBUG
	DBG("\nsubject: %s", subject);
#endif
	if (re->err) {
		if (re->err_str)
			ktre__free(re, re->err_str);
		re->err = KTRE_ERROR_NO_ERROR;
	}

	return run(re, subject, vec);
}

char *ktre_filter(struct ktre *re, const char *subject, const char *replacement)
{
#ifdef KTRE_DEBUG
	DBG("\nsubject: %s", subject);
#endif
	int **vec = NULL;
	if (!run(re, subject, &vec) || re->err) {
		return NULL;
	}

	char *ret = ktre__malloc(re, 512);
	*ret = 0;
	int idx = 0;

	for (int i = 0; i < re->num_matches; i++) {
		if (i > 0) {
			int diff = vec[i][0] - (vec[i - 1][0] + vec[i - 1][1]);
			ret = ktre__realloc(re, ret, (idx + diff + 1) * sizeof *ret);
			strncpy(ret + idx, subject + vec[i - 1][0] + vec[i - 1][1], diff);
			idx += diff;
		} else {
			idx = vec[i][0];
			ret = ktre__realloc(re, ret, (idx + 1) * sizeof *ret);
			strncpy(ret, subject, idx);
		}

		ret[idx] = 0;

		char *group = NULL;
		int j = 0;

		const char *r = replacement;

		while (*r) {
			if (*r == '\\') {
				r++;
				int n = dec_num(&r);

				if (n < 0 || n >= re->num_groups) {
					ktre__free(re, group);
					ktre__free(re, ret);
					error(re, KTRE_ERROR_SYNTAX_ERROR, 0, "backreference references a group that does not exist");
					return NULL;
				}

				group = ktre__realloc(re, group, (j + vec[i][n * 2 + 1] + 2) * sizeof *group);
				strncpy(group + j, subject + vec[i][n * 2], vec[i][n * 2 + 1]);
				j += vec[i][n * 2 + 1];
			} else {
				group = ktre__realloc(re, group, (j + 2) * sizeof *group);
				group[j++] = *r;
				r++;
			}
		}

		if (group) {
			group[j] = 0;

			ret = ktre__realloc(re, ret, (idx + j + 1) * sizeof *ret);
			strcpy(ret + idx, group);
			ret[idx + j] = 0;
			idx += j;

			ktre__free(re, group);
		}
	}

	int diff = vec[re->num_matches - 1][0] + vec[re->num_matches - 1][1];
	ret = ktre__realloc(re, ret, (idx + diff + 2) * sizeof *ret);
	strncpy(ret + idx, subject + diff, strlen(subject) - diff);
	idx += strlen(subject) - diff;
	ret[idx] = 0;

	char *a = KTRE_MALLOC(strlen(ret) + 1);
	strcpy(a, ret);
	ktre__free(re, ret);

	return a;
}

int **ktre_getvec(const struct ktre *re)
{
	int **vec = KTRE_MALLOC(re->num_matches  * sizeof re->vec[0]);

	for (int i = 0; i < re->num_matches; i++) {
		vec[i] = KTRE_MALLOC(re->num_groups * 2 * sizeof re->vec[0][0]);
		memcpy(vec[i], re->vec[i], re->num_groups * 2 * sizeof re->vec[0][0]);
	}

	return vec;
}
#endif /* ifdef KTRE_IMPLEMENTATION */
#endif /* ifndef KTRE_H */
