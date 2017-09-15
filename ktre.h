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
 * The API consists of only three functions: ktre_compile(),
 * ktre_exec(), and ktre_free(). The prototypes for these functions
 * are:
 *
 * struct ktre *ktre_compile(const char *pat, int opt);
 * _Bool ktre_exec(struct ktre *re, const char *subject, int **vec);
 * void ktre_free(struct ktre *re);
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
 * do_regex(struct ktre *re, const char *regex, const char *subject)
 * {
 * 	int *vec = NULL;
 * 	fprintf(stderr, "\nsubject: %s", subject);
 *
 * 	if (re->err) { // failed to compile
 * 		fprintf(stderr, "\nregex failed to compile with error code %d: %s\n", re->err, re->err_str);
 * 		fprintf(stderr, "\t%s\n\t", regex);
 * 		for (int i = 0; i < re->loc; i++) fputc(' ', stderr);
 * 		fprintf(stderr, "^\n");
 * 	} else if (ktre_exec(re, subject, &vec)) { // ran and succeeded
 * 		fprintf(stderr, "\nmatched: '%.*s'", vec[1], subject + vec[0]);
 *
 * 		for (int i = 1; i < re->num_groups; i++) {
 * 			if (vec[i * 2 + 1] && (int)strlen(subject) != vec[i * 2])
 * 				fprintf(stderr, "\ngroup %d: '%.*s'", i, vec[i * 2 + 1], subject + vec[i * 2]);
 * 		}
 *
 * 		fputc('\n', stderr);
 * 	} else if (re->err) { // ran, but failed with an error
 * 		fprintf(stderr, "\nregex failed at runtime with error code %d: %s\n", re->err, re->err_str);
 *
 * 		fprintf(stderr, "\t%s\n\t", regex);
 * 		for (int i = 0; i < re->loc; i++) putchar(' ');
 * 		fprintf(stderr, "^\n");
 * 	} else { // ran, but failed
 * 		fprintf(stderr, "\ndid not match!\n");
 * 	}
 * }
 *
 * int
 * main(int argc, char *argv[])
 * {
 * 	if (argc != 3) {
 * 		fprintf(stderr, "Usage: ktre [subject] [pattern]\n");
 * 		exit(EXIT_FAILURE);
 *
 * 	}
 *
 * 	char *subject = argv[1], *regex = argv[2];
 * 	struct ktre *re = ktre_compile(regex, KTRE_UNANCHORED);
 * 	do_regex(re, regex, subject);
 * 	ktre_free(re);
 *
 * 	return EXIT_SUCCESS;
 * }
 */

#ifndef KTRE_H
#define KTRE_H

/* error codes */
enum ktre_error {
	KTRE_ERROR_NO_ERROR,
	KTRE_ERROR_UNMATCHED_PAREN,
	KTRE_ERROR_UNTERMINATED_CLASS,
	KTRE_ERROR_STACK_OVERFLOW,
	KTRE_ERROR_INVALID_MODE_MODIFIER,
	KTRE_ERROR_INVALID_RANGE,
	KTRE_ERROR_INVALID_BACKREFERENCE,
	KTRE_ERROR_CALL_OVERFLOW,
	KTRE_ERROR_SYNTAX_ERROR
};

/* options */
enum {
	KTRE_INSENSITIVE = 1 << 0,
	KTRE_UNANCHORED  = 1 << 1,
	KTRE_EXTENDED    = 1 << 2,
};

/* settings and limits */
#define KTRE_MAX_GROUPS 100
#define KTRE_MAX_THREAD 200
#define KTRE_MAX_CALL_DEPTH 100

struct ktre {
	/* ===== public fields ===== */
	int num_groups, opt;
	/* string containing an error message in the case
	 * of failure during parsing or compilation */
	char *err_str;
	enum ktre_error err; /* error status code */
	_Bool failed; /* whether or not this regex compiled successfully */
	/* the location of any error that occurred, as an index
	 * in the last subject the regex was run on */
	int loc;

	/* ===== private fields ===== */
	struct instr *c; /* code */
	int ip;          /* instruction pointer */
	int instr_alloc; /* the number of instructions that have been allocated so far */
	int num_prog;    /* the number of progress instructions */
	char const *pat;
	char const *sp;  /* pointer to the character currently being parsed */
	struct node *n;
	int popt;        /* the options, as seen by the parser */

	struct group {
		int address;
		_Bool is_compiled;
		_Bool is_called; /* whether this group is ever called as a subroutine */
	} *group;
	int gp;

	/* runtime */
	struct thread {
		/*
		 * instruction pointer, string pointer, frame pointer,
		 * lookaround stack pointer, options.
		 */
		int ip, sp, fp, la, opt;

		/*
		 * frame return addresses, vec contains the bounds
		 * for the submatches, and prog contains the
		 * forbidden sp values for individual prog instructions.
		 */
		int *frame, *vec, *prog, *las;

		/*
		 * Threads with the die field set to true are special;
		 * if the vm ever backtracks into one the entire match must
		 * fail immediately.
		 * This is used to implement the kill_tp instruction,
		 * which prevents backtracking.
		 */
		_Bool die;
	} *t;

	/* The number of threads that have been allocated. This
	 * persists between executions. */
	int thread_alloc;
	int tp, max_tp;
};

struct ktre *ktre_compile(const char *pat, int opt);
_Bool ktre_exec(struct ktre *re, const char *subject, int **vec);
void ktre_free(struct ktre *re);

#ifdef KTRE_IMPLEMENTATION
/* memory functions */
#ifndef KTRE_MALLOC
#define KTRE_MALLOC  malloc
#endif

#ifndef KTRE_REALLOC
#define KTRE_REALLOC realloc
#endif

#ifndef KTRE_FREE
#define KTRE_FREE    free
#endif

#define WHITESPACE " \t\r\n\v\f"
#define WORD "_0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
#define DIGIT "0123456789"

#ifdef KTRE_DEBUG
#include <stdio.h>
#include <assert.h>

static void print_node(struct node *n);

#define DBG(...) fprintf(stderr, __VA_ARGS__)
#define DBGF(str)							\
	do {								\
		for (int _i = 0; _i < (int)strlen(str); _i++) { \
			if ((strchr(WHITESPACE, str[_i]) || str[_i] == '\\') && str[_i] != ' ') { \
				fputc('\\', stderr);			\
				switch (str[_i]) {			\
				case '\t': fputc('t', stderr); break;	\
				case '\r': fputc('r', stderr); break;	\
				case '\n': fputc('n', stderr); break;	\
				case '\v': fputc('v', stderr); break;	\
				case '\f': fputc('f', stderr); break;	\
				case '\\': fputc('\\',stderr); break;	\
				default: fputc(str[_i], stderr);	\
				}					\
			} else {					\
				fputc(str[_i], stderr);			\
			}						\
		}							\
	} while (0);
#endif

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <setjmp.h>

static int
add_group(struct ktre *re)
{
	re->group = KTRE_REALLOC(re->group, sizeof re->group[0] * (re->gp + 1));
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
		INSTR_SPLIT,
		INSTR_ANY,
		INSTR_CLASS,
		INSTR_STR,
		INSTR_NOT,
		INSTR_BACKREF,
		INSTR_BOL,
		INSTR_EOL,
		INSTR_OPT_ON,
		INSTR_OPT_OFF,
		INSTR_KILL_TP,
		INSTR_DIE,
		INSTR_SET_START,
		INSTR_WB,
		INSTR_SAVE,
		INSTR_CALL,
		INSTR_LA_YES,
		INSTR_LA_NO,
		INSTR_LA_WIN,
		INSTR_LA_FAIL,
		INSTR_LB_YES,
		INSTR_LB_NO,
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
	if (!re->instr_alloc) {
		re->instr_alloc = 25;
		re->c = KTRE_MALLOC(sizeof re->c[0] * re->instr_alloc);
	}

	if (re->ip + n >= re->instr_alloc) {
		if (re->ip + n >= re->instr_alloc * 2) {
			re->instr_alloc = re->ip + n;
		} else {
			re->instr_alloc *= 2;
		}

		re->c = KTRE_REALLOC(re->c, sizeof re->c[0] * re->instr_alloc);
	}
}

static void
emit_ab(struct ktre *re, int instr, int a, int b, int loc)
{
	grow_code(re, 1);
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
	re->c[re->ip].op = instr;
	re->c[re->ip].c = c;
	re->c[re->ip].loc = loc;

	re->ip++;
}

static void
emit_class(struct ktre *re, int instr, char *class, int loc)
{
	grow_code(re, 1);
	re->c[re->ip].op = instr;
	re->c[re->ip].class = class;
	re->c[re->ip].loc = loc;

	re->ip++;
}

static void
emit(struct ktre *re, int instr, int loc)
{
	grow_code(re, 1);
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
		NODE_OPT_ON,	/* turns some option off */
		NODE_OPT_OFF,	/* turns some option on */
		NODE_REP,	/* counted repetition */
		NODE_ATOM,	/* atomic group */
		NODE_SET_START, /* set the start position of the group capturing the entire match */
		NODE_WB,	/* word boundary */
		NODE_CALL,
		NODE_LA_YES,	/* positive lookahead */
		NODE_LB_YES,	/* positive lookbehind */
		NODE_LA_NO,	/* negative lookahead */
		NODE_LB_NO,	/* negative lookbehind */
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
free_node(struct node *n)
{
	/* sometimes parse errors can produce NULL nodes */
	if (!n) return;

	switch (n->type) {
	case NODE_SEQUENCE: case NODE_OR:
		free_node(n->a);
		free_node(n->b);
		break;
	case NODE_QUESTION: case NODE_REP:   case NODE_ASTERISK:
	case NODE_PLUS:     case NODE_GROUP: case NODE_ATOM:
	case NODE_LA_YES:   case NODE_LA_NO: case NODE_LB_YES:
	case NODE_LB_NO:
		free_node(n->a);
		break;
	case NODE_CLASS: case NODE_STR: KTRE_FREE(n->class); break;
	default: break;
	}

	KTRE_FREE(n);
}

#define NEXT                           \
	do {                           \
		if (*re->sp) re->sp++; \
	} while (0)

#define MAX_ERROR_LEN 256 /* some arbitrary limit */

#include <stdarg.h>
static void
error(struct ktre *re, enum ktre_error err, int loc, char *fmt, ...)
{
	re->failed = true;
	re->err = err;
	re->err_str = KTRE_MALLOC(MAX_ERROR_LEN);
	re->loc = loc;

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
class_add_char(char *class, char c)
{
	size_t len = class ? strlen(class) : 0;
	class = KTRE_REALLOC(class, len + 2);
	class[len] = c;
	class[len + 1] = 0;
	return class;
}

static char *
class_add_str(char *class, const char *c)
{
	size_t len = class ? strlen(class) : 0;
	class = KTRE_REALLOC(class, len + strlen(c) + 1);
	strcpy(class + len, c);
	return class;
}

static char *
strclone(const char *str)
{
	char *ret = KTRE_MALLOC(strlen(str) + 1);
	strcpy(ret, str);
	return ret;
}

static struct node *
new_node()
{
	struct node *n = KTRE_MALLOC(sizeof *n);
	memset(n, 0, sizeof *n);
	n->loc = -1;
	return n;
}

static struct node *
factor(struct ktre *re)
{
	if (!*re->sp) return NULL;

	struct node *left = new_node();
	left->loc = re->sp - re->pat;

again:
	/* parse a primary */
	switch (*re->sp) {
	case '\\': /* escape sequences */
		NEXT;

		switch (*re->sp) {
		case 's':
			left->type = NODE_SPACE;
			NEXT;
			break;

		case 'S':
			left->type = NODE_NOT;
			left->class = strclone(WHITESPACE);
			NEXT;
			break;

		case 'd':
			left->type = NODE_DIGIT;
			NEXT;
			break;

		case 'D':
			left->type = NODE_NOT;
			left->class = strclone(DIGIT);
			NEXT;
			break;

		case 'w':
			left->type = NODE_WORD;
			NEXT;
			break;

		case 'W':
			left->type = NODE_NOT;
			left->class = strclone(WORD);
			NEXT;
			break;

		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			left->type = NODE_BACKREF;
			left->c = *re->sp - '0';
			if (isdigit(re->sp[1])) {
				left->c *= 10;
				left->c += re->sp[1] - '0';
				NEXT;
			}
			NEXT;
			break;

		case 'K':
			left->type = NODE_SET_START;
			NEXT;
			break;

		case 'b':
			left->type = NODE_WB;
			NEXT;
			break;

		case 'n':
			left->type = NODE_CHAR;
			left->c = '\n';
			NEXT;
			break;

		default:
			left->type = NODE_CHAR;
			left->c = *re->sp;
			NEXT;
		}
		break;

	case '[': { /* character classes */
		int loc = re->sp - re->pat;
		NEXT;

		if (*re->sp == '^') {
			NEXT;
			left->type = NODE_NOT;
		} else {
			left->type = NODE_CLASS;
		}

		char *class = NULL;
		while (*re->sp && *re->sp != ']') {
			if (re->sp[1] == '-' && re->sp[2] != ']') {
				for (int i = re->sp[0]; i <= re->sp[2]; i++) {
					class = class_add_char(class, i);

					if (re->popt & KTRE_INSENSITIVE) {
						if (i >= 'A' && i <= 'Z')
							class = class_add_char(class, lc(i));
					}
				}
				re->sp += 3;
			} else if (*re->sp == '\\') {
				NEXT;

				switch (*re->sp) {
				case 's':
					class = class_add_str(class, WHITESPACE);
					re->sp++;
					break;
				case 'd':
					class = class_add_str(class, DIGIT);
					re->sp++;
					break;
				case 'w':
					class = class_add_str(class, WORD);
					re->sp++;
					break;
				case 'n':
					class = class_add_char(class, '\n');
					re->sp++;
				}
			} else {
				if (re->popt & KTRE_INSENSITIVE) {
					class = class_add_char(class, *re->sp);
					if (*re->sp >= 'A' && *re->sp <= 'Z')
						class = class_add_char(class, lc(*re->sp));
				}
				else class = class_add_char(class, *re->sp);
				re->sp++;
			}
		}

		if (*re->sp != ']') {
			error(re, KTRE_ERROR_UNTERMINATED_CLASS, loc, "unterminated character class");
			free_node(left);
			return NULL;
		}

		left->class = class;
		NEXT;
	} break;

	case '(':
		NEXT;

		if (*re->sp == '?' && re->sp[1] == '#') { /* comments */
			while (*re->sp && *re->sp != ')') re->sp++;

			if (*re->sp != ')') {
				error(re, KTRE_ERROR_UNMATCHED_PAREN, re->sp - re->pat, "unmatched '('");
				free_node(left->a);
				left->a = NULL;
				free_node(left);
				return NULL;
			}

			left->type = NODE_NONE;
		} else if (!strncmp(re->sp, "?R)", 3)) {
			re->sp += 2;
			left->type = NODE_RECURSE;
			re->group[0].is_called = true;
		} else if (*re->sp == '?' && re->sp[1] == '<' && re->sp[2] == '=') { /* positive lookbehind */
			left->type = NODE_LB_YES;
			re->sp += 3;
			left->a = parse(re);

			if (*re->sp != ')') {
				error(re, KTRE_ERROR_UNMATCHED_PAREN, re->sp - re->pat, "unmatched '('");
				free_node(left->a); left->a = NULL;
				free_node(left);
				return NULL;
			}
		} else if (*re->sp == '?' && re->sp[1] == ':') { /* uncaptured group */
			re->sp += 2;
			free_node(left);
			left = parse(re);

			if (*re->sp != ')') {
				error(re, KTRE_ERROR_UNMATCHED_PAREN, re->sp - re->pat, "unmatched '('");
				free_node(left->a); left->a = NULL;
				free_node(left);
				return NULL;
			}
		} else if (*re->sp == '?' && re->sp[1] == '<' && re->sp[2] == '!') { /* positive lookbehind */
			left->type = NODE_LB_NO;
			re->sp += 3;
			left->a = parse(re);

			if (*re->sp != ')') {
				error(re, KTRE_ERROR_UNMATCHED_PAREN, re->sp - re->pat, "unmatched '('");
				free_node(left->a); left->a = NULL;
				free_node(left);
				return NULL;
			}
		} else if (*re->sp == '?' && re->sp[1] == '>') {
			re->sp += 2;
			left->type = NODE_ATOM;
			left->a = parse(re);

			if (*re->sp != ')') {
				error(re, KTRE_ERROR_UNMATCHED_PAREN, re->sp - re->pat, "unmatched '('");
				free_node(left->a); left->a = NULL;
				free_node(left);
				return NULL;
			}
		} else if (*re->sp == '?' && isdigit(re->sp[1])) { /* subroutine call */
			NEXT;
			int len = 0, a = -1;

			while (re->sp[len] && re->sp[len] != ')') len++;
			if (re->sp[len] != ')') {
				error(re, KTRE_ERROR_UNMATCHED_PAREN, re->sp + len - re->pat - 1, "unmatched '('");
				free_node(left->a); left->a = NULL;
				free_node(left);
				return NULL;
			}

			len--;

			for (int i = len; i >= 0; i--) {
				if (!isdigit(re->sp[i])) {
					error(re, KTRE_ERROR_INVALID_RANGE, re->pat - re->sp + i,
					      "non-digits and negative numbers are forbidden in subroutine calls");
					free_node(left);
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
		} else if (*re->sp == '?' && re->sp[1] == '=') {
			left->type = NODE_LA_YES;
			NEXT; NEXT;
			left->a = parse(re);

			if (*re->sp != ')') {
				error(re, KTRE_ERROR_UNMATCHED_PAREN, re->sp - re->pat, "unmatched '('");
				free_node(left->a); left->a = NULL;
				free_node(left);
				return NULL;
			}
		} else if (*re->sp == '?' && re->sp[1] == '!') {
			left->type = NODE_LA_NO;
			NEXT; NEXT;
			left->a = parse(re);

			if (*re->sp != ')') {
				error(re, KTRE_ERROR_UNMATCHED_PAREN, re->sp - re->pat, "unmatched '('");
				free_node(left->a); left->a = NULL;
				free_node(left);
				return NULL;
			}
		} else if (*re->sp == '?') { /* mode modifiers */
			NEXT;
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
				case '-': neg = true; NEXT; continue;
				default:
					error(re, KTRE_ERROR_INVALID_MODE_MODIFIER, re->sp - re->pat, "invalid mode modifier");
					free_node(left);
					return NULL;
				}

				if (off || neg) re->popt &= ~opt;
				else re->popt |= opt;

				struct node *tmp = new_node();
				tmp->loc = re->sp - re->pat;
				tmp->type = NODE_SEQUENCE;
				tmp->a = left;
				tmp->b = new_node();
				tmp->b->loc = re->sp - re->pat;
				tmp->b->type = off || neg ? NODE_OPT_OFF : NODE_OPT_ON;
				tmp->b->c = opt;
				left = tmp;

				off = false;
				re->sp++;
			}

			if (*re->sp != ')') {
				error(re, KTRE_ERROR_UNMATCHED_PAREN, re->sp - re->pat, "unmatched '('");
				free_node(left->a); left->a = NULL;
				free_node(left);
				return NULL;
			}
		} else {
			left->gi = add_group(re);
			re->group[left->gi].is_called = false;
			left->type = NODE_GROUP;
			left->a = parse(re);

			if (*re->sp != ')') {
				error(re, KTRE_ERROR_UNMATCHED_PAREN, re->sp - re->pat - 1, "unmatched '('");
				free_node(left->a); left->a = NULL;
				free_node(left);
				return NULL;
			}
		}

		NEXT;
		break;
	case '.':
		NEXT;
		left->type = NODE_ANY;
		break;

	case '^':
		NEXT;
		left->type = NODE_BOL;
		break;

	case '$':
		NEXT;
		left->type = NODE_EOL;
		break;

	case '#': /* extended mode comments */
		if (re->popt & KTRE_EXTENDED) {
			while (*re->sp && *re->sp != '\n') { NEXT; }
		} else {
			left->type = NODE_CHAR;
			left->c = *re->sp;
			NEXT;
		}
		break;

	default:
		/* ignore whitespace if we're in extended mode */
		if (re->popt & KTRE_EXTENDED && strchr(WHITESPACE, *re->sp)) {
			while (*re->sp && strchr(WHITESPACE, *re->sp))
				NEXT;
			goto again;
		}

		left->type = NODE_CHAR;
		left->c = *re->sp;
		NEXT;
	}

	while (*re->sp && (*re->sp == '*' || *re->sp == '+' || *re->sp == '?' || *re->sp == '{')) {
		NEXT;
		struct node *n = new_node();

		switch (re->sp[-1]) {
		case '*': n->type = NODE_ASTERISK; break;
		case '?': n->type = NODE_QUESTION; break;
		case '+': n->type = NODE_PLUS;     break;
		case '{': { /* counted repetition */
			int len = 0, a = 0, b = 0;

			while (re->sp[len] != '}' && re->sp[len] != ',' && re->sp[len]) len++;
			if (re->sp[len] == ',') {
				len--;
				for (int i = len; i >= 0; i--) {
					if (!isdigit(re->sp[i])) {
						error(re, KTRE_ERROR_INVALID_RANGE, re->pat - re->sp + i,
						      "non-digits and negative numbers are forbidden in counted repetitions");
						free_node(left);
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

				re->sp += len + 2;
				len = 0;

				while (re->sp[len] != '}' && re->sp[len]) len++;
				len--;
				for (int i = len; i >= 0; i--) {
					if (!isdigit(re->sp[i])) {
						error(re, KTRE_ERROR_INVALID_RANGE, re->pat - re->sp + i,
						      "non-digits and negative numbers are forbidden in counted repetitions");
						free_node(left);
						return NULL;
					}

					if (i == len) b = re->sp[i] - '0';
					else {
						int d = re->sp[i] - '0';
						for (int j = 0; j < len - i; j++) {
							d *= 10;
						}
						b += d;
					}
				}

				re->sp += len + 2;
			} else {
				len--;
				for (int i = len; i >= 0; i--) {
					if (!isdigit(re->sp[i])) {
						error(re, KTRE_ERROR_INVALID_RANGE, re->pat - re->sp + i,
						      "non-digits and negative numbers are forbidden in counted repetitions");
						free_node(left);
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

				re->sp += len + 2;
				b = -1;
			}

			n->type = NODE_REP;
			n->c = a;
			n->d = b;
		} break;
		}

		n->a = left;

		if (n->type == NODE_REP)
			if (n->a->type == NODE_GROUP)
				re->group[n->a->gi].is_called = true;

		left = n;
	}

	left->loc = re->sp - re->pat - 1;

	return left;
}

static struct node *
term(struct ktre *re)
{
	struct node *left = new_node();
	left->type = NODE_NONE;
	left->loc = re->sp - re->pat;

	while (*re->sp && *re->sp != '|' && *re->sp != ')') {
		struct node *right = factor(re);

		if (re->err) {
			free_node(left);
			free_node(right);
			return NULL;
		}

		if (left->type == NODE_NONE) {
			KTRE_FREE(left);
			left = right;
		} else {
			if ((left->type == NODE_CHAR || left->type == NODE_STR) && right->type == NODE_CHAR) {
				if (left->type == NODE_CHAR) {
					char a = left->c;
					left->type = NODE_STR;
					left->class = KTRE_MALLOC(3);

					if (re->popt & KTRE_INSENSITIVE) {
						left->class[0] = lc(a);
						left->class[1] = lc(right->c);
					} else {
						left->class[0] = a;
						left->class[1] = right->c;
					}

					left->class[2] = 0;
					free_node(right);
				} else {
					if (re->popt & KTRE_INSENSITIVE) {
						left->class = class_add_char(left->class, lc(right->c));
					} else {
						left->class = class_add_char(left->class, right->c);
					}

					free_node(right);
				}
			} else if (right->type == NODE_CHAR && left->type == NODE_SEQUENCE
				   && (left->b->type == NODE_CHAR || left->b->type == NODE_STR)) {
				if (left->b->type == NODE_CHAR) {
					char a = left->b->c;
					left->b->type = NODE_STR;
					left->b->class = KTRE_MALLOC(3);

					if (re->popt & KTRE_INSENSITIVE) {
						left->b->class[0] = lc(a);
						left->b->class[1] = lc(right->c);
					} else {
						left->b->class[0] = a;
						left->b->class[1] = right->c;
					}

					left->b->class[2] = 0;
					free_node(right);
				} else {
					if (re->popt & KTRE_INSENSITIVE) {
						left->b->class = class_add_char(left->b->class, lc(right->c));
					} else {
						left->b->class = class_add_char(left->b->class, right->c);
					}

					free_node(right);
				}
			} else {
				struct node *tmp = new_node();
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
		NEXT;

		struct node *m = new_node();
		m->type = NODE_OR;
		m->a = n;
		m->b = parse(re);
		m->loc = re->sp - re->pat;

		if (re->err) {
			free_node(m->b);
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
	case NODE_CLASS:     DBG("(class '"); DBGF(n->class); DBG("')");      break;
	case NODE_STR:       DBG("(string '"); DBGF(n->class); DBG("')");     break;
	case NODE_NOT:       DBG("(not '%s')", n->class);                     break;
	case NODE_BOL:       DBG("(bol)");                                    break;
	case NODE_EOL:       DBG("(eol)");                                    break;
	case NODE_RECURSE:   DBG("(recurse)");                                break;
	case NODE_SET_START: DBG("(set_start)");                              break;
	case NODE_OPT_ON:    DBG("(opt on %d)", n->c);                        break;
	case NODE_OPT_OFF:   DBG("(opt off %d)", n->c);                       break;
	case NODE_CALL:      DBG("(call %d)", n->c);                          break;
	case NODE_SEQUENCE:  N2 ("(sequence)");                               break;
	case NODE_OR:        N2 ("(or)");                                     break;
	case NODE_REP:       N1 ("(counted repetition %d - %d)", n->c, n->d); break;
	case NODE_ASTERISK:  N1 ("(asterisk)");                               break;
	case NODE_PLUS:      N1 ("(plus)");                                   break;
	case NODE_GROUP:     N1 ("(group)");                                  break;
	case NODE_QUESTION:  N1 ("(question)");                               break;
	case NODE_ATOM:      N1 ("(atom)");                                   break;
	case NODE_LA_YES:    N1 ("(lookahead)");                              break;
	case NODE_LA_NO:     N1 ("(negative lookahead)");                     break;
	case NODE_LB_YES:    N1 ("(lookbehind)");                             break;
	case NODE_LB_NO:     N1 ("(negative lookbehind)");                    break;

	default:
		DBG("\nunimplemented printer for node of type %d\n", n->type);
		assert(false);
	}
}
#endif

static void
compile(struct ktre *re, struct node *n)
{
#define PATCH_A(loc, _a) re->c[loc].a = _a
#define PATCH_B(loc, _b) re->c[loc].b = _b
#define PATCH_C(loc, _c) re->c[loc].c = _c
	int a = -1, b = -1, old = -1;

	switch (n->type) {
	case NODE_ASTERISK:
		a = re->ip;
		emit_ab(re, INSTR_SPLIT, re->ip + 1, -1, n->loc);
		emit_c(re, INSTR_PROG, re->num_prog++, n->loc);
		compile(re, n->a);
		emit_ab(re, INSTR_SPLIT, a + 1, re->ip + 1, n->loc);
		PATCH_B(a, re->ip);
		break;

	case NODE_QUESTION: {
		switch (n->a->type) {
		case NODE_ASTERISK:
			a = re->ip;
			emit_ab(re, INSTR_SPLIT, -1, re->ip + 1, n->loc);
			emit_c(re, INSTR_PROG, re->num_prog++, n->loc);
			compile(re, n->a->a);
			emit_ab(re, INSTR_SPLIT, re->ip + 1, a + 1, n->loc);
			PATCH_A(a, re->ip);
			break;
		case NODE_PLUS:
			a = re->ip;
			emit_c(re, INSTR_PROG, re->num_prog++, n->loc);
			compile(re, n->a->a);
			emit_ab(re, INSTR_SPLIT, re->ip + 1, a, n->loc);
			break;
		case NODE_QUESTION:
			a = re->ip;
			emit_ab(re, INSTR_SPLIT, -1, re->ip + 1, n->loc);
			emit_c(re, INSTR_PROG, re->num_prog++, n->loc);
			compile(re, n->a->a);
			PATCH_A(a, re->ip);
			break;
		default:
			a = re->ip;
			emit_ab(re, INSTR_SPLIT, re->ip + 1, -1, n->loc);
			compile(re, n->a);
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

			compile(re, n->a);

			emit(re, INSTR_RET, n->loc);
			PATCH_C(a, re->ip);

			re->group[old].is_compiled = true;
		} else {
			emit_c(re, INSTR_SAVE, re->num_groups * 2, n->loc);

			old = re->num_groups;
			re->num_groups++;
			re->group[old].address = re->ip - 1;

			compile(re, n->a);

			emit_c(re, INSTR_SAVE, old * 2 + 1, n->loc);

			re->group[old].is_compiled = true;
		}
		break;

	case NODE_CALL:
		if (n->c <= 0 || n->c >= re->num_groups) {
			error(re, KTRE_ERROR_INVALID_BACKREFERENCE, n->loc, "subroutine number is invalid or calls a group that does not yet exist");
			return;
		}

		emit_c(re, INSTR_CALL, re->group[n->c].address + 1, n->loc);
		break;

	case NODE_PLUS:
		switch (n->a->type) {
		case NODE_ASTERISK: case NODE_PLUS: case NODE_QUESTION:
		case NODE_REP:
			emit_c(re, INSTR_PROG, re->num_prog++, n->loc);
			compile(re, n->a);
			emit(re, INSTR_KILL_TP, n->loc);
			break;
		default:
			a = re->ip;
			emit_c(re, INSTR_PROG, re->num_prog++, n->loc);
			compile(re, n->a);
			emit_ab(re, INSTR_SPLIT, a, re->ip + 1, n->loc);
		}
		break;

	case NODE_OR:
		a = re->ip;
		emit_ab(re, INSTR_SPLIT, re->ip + 1, -1, n->loc);
		compile(re, n->a);
		b = re->ip;
		emit_c(re, INSTR_JMP, -1, n->loc);
		PATCH_B(a, re->ip);
		compile(re, n->b);
		PATCH_C(b, re->ip);
		break;

	case NODE_SEQUENCE:
		compile(re, n->a);
		compile(re, n->b);
		break;

	case NODE_CLASS:     emit_class(re, INSTR_CLASS, n->class, n->loc); break;
	case NODE_STR:       emit_class(re, INSTR_STR,   n->class, n->loc); break;
	case NODE_NOT:       emit_class(re, INSTR_NOT, n->class, n->loc);   break;
	case NODE_OPT_ON:    emit_c(re, INSTR_OPT_ON,  n->c, n->loc);       break;
	case NODE_OPT_OFF:   emit_c(re, INSTR_OPT_OFF, n->c, n->loc);       break;
	case NODE_CHAR:      emit_c(re, INSTR_CHAR,    n->c, n->loc);       break;
	case NODE_BOL:       emit(re, INSTR_BOL, n->loc);                   break;
	case NODE_EOL:       emit(re, INSTR_EOL, n->loc);                   break;
	case NODE_ANY:       emit(re, INSTR_ANY, n->loc);                   break;
	case NODE_SET_START: emit(re, INSTR_SET_START, n->loc);             break;
	case NODE_WB:        emit(re, INSTR_WB, n->loc);                    break;
	case NODE_NONE:                                                     break;

	case NODE_BACKREF:
		if (n->c <= 0 || n->c >= re->num_groups) {
			error(re, KTRE_ERROR_INVALID_BACKREFERENCE, n->loc, "backreference number is invalid or references a group that does not yet exist");
			return;
		}

		if (!re->group[n->c].is_compiled) {
			error(re, KTRE_ERROR_INVALID_BACKREFERENCE, n->loc, "backreferences may not reference the group they occur in");
			return;
		}

		emit_c(re, INSTR_BACKREF, n->c, n->loc);
		break;

	case NODE_REP:
		a = 0;
		for (int i = 0; i < n->c; i++) {
			a = re->ip;
			if (n->a->type == NODE_GROUP && !re->group[n->a->gi].is_compiled) {
				compile(re, n->a);
			} else if (n->a->type == NODE_GROUP) {
				emit_c(re, INSTR_CALL, re->group[n->a->gi].address, n->loc);
			} else {
				compile(re, n->a);
			}
		}

		/* TODO: make repetitions work with groups */
		if (n->d == 0) {
			emit_ab(re, INSTR_SPLIT, a, re->ip + 1, n->loc);
		} else {
			/* this will fail to do anything if n->d is less than n->c,
			 * which handles the case where n->d == -1.
			 * n->d == -1 when the repetition is of the form {n}. */
			for (int i = 0; i < n->d - n->c; i++) {
				a = re->ip;
				emit_ab(re, INSTR_SPLIT, re->ip + 1, -1, n->loc);

				if (n->a->type == NODE_GROUP) {
					emit_c(re, INSTR_CALL, re->group[n->a->gi].address, n->loc);
				} else {
					compile(re, n->a);
				}

				PATCH_B(a, re->ip);
			}
		}
		break;

	case NODE_ATOM:
		compile(re, n->a);
		emit(re, INSTR_KILL_TP, n->loc);
		break;

	case NODE_LA_YES:
		emit(re, INSTR_LA_YES, n->loc);
		compile(re, n->a);
		emit(re, INSTR_LA_WIN, n->loc);
		break;

	case NODE_LA_NO:
		a = re->ip;
		emit_c(re, INSTR_LA_NO, -1, n->loc);
		compile(re, n->a);
		emit(re, INSTR_LA_FAIL, n->loc);
		PATCH_C(a, re->ip);
		break;

	case NODE_LB_YES:
		emit(re, INSTR_LB_YES, n->loc);
		compile(re, n->a);
		break;

	case NODE_LB_NO:
		a = re->ip;
		emit_c(re, INSTR_LB_NO, -1, n->loc);
		compile(re, n->a);
		emit(re, INSTR_DIE, n->loc);
		PATCH_C(a, re->ip);
		break;

	case NODE_RECURSE:
		emit_c(re, INSTR_CALL, re->group[0].address + 1, n->loc);
		break;

	case NODE_DIGIT:
		emit(re, INSTR_DIGIT, n->loc);
		break;

	case NODE_WORD:
		emit(re, INSTR_WORD, n->loc);
		break;

	case NODE_SPACE:
		emit(re, INSTR_SPACE, n->loc);
		break;

	default:
#ifdef KTRE_DEBUG
		DBG("\nunimplemented compiler for node of type %d\n", n->type);
		assert(false);
#endif
		break;
	}
}

struct ktre *
ktre_compile(const char *pat, int opt)
{
	struct ktre *re = KTRE_MALLOC(sizeof *re);
	memset(re, 0, sizeof *re);

	re->pat = pat;
	re->opt = opt;
	re->sp = pat;
	re->popt = opt;
	re->max_tp = -1;
	re->err_str = "no error";

#ifdef KTRE_DEBUG
	DBG("regexpr: %s", pat);
	if (opt) DBG("\noptions:");
	for (size_t i = 0; i < sizeof opt; i++) {
		switch (opt & 1 << i) {
		case KTRE_INSENSITIVE: DBG("\n\tINSENSITIVE"); break;
		case KTRE_UNANCHORED:  DBG("\n\tUNANCHORED");  break;
		case KTRE_EXTENDED:    DBG("\n\tEXTENDED");    break;
		default: continue;
		}
	}
#endif

	re->n = new_node();
	re->n->loc = 0;
	re->n->type = NODE_GROUP;
	re->n->gi = add_group(re);
	re->group[0].is_compiled = false;
	re->group[0].is_called = false;

	re->n->a = NULL;
	struct node *n = parse(re);
	re->n->a = n;

	if (re->failed) {
		free_node(re->n);
		re->n = NULL;
		return re;
	}

	if (*re->sp) {
		error(re, KTRE_ERROR_SYNTAX_ERROR, re->sp - re->pat, "invalid regex syntax; unmatched righthand delimiter");
		re->failed = true;
		return re;
	}

#ifdef KTRE_DEBUG
	print_node(re->n);
#endif

	/* just emit the bytecode for .*? */
	if (re->opt & KTRE_UNANCHORED) {
		emit_ab(re, INSTR_SPLIT, 3, 1, 0);
		emit(re, INSTR_ANY, 0);
		emit_ab(re, INSTR_SPLIT, 3, 1, 0);
	}

	compile(re, re->n);
	if (re->failed)
		return re;

	emit(re, INSTR_MATCH, re->sp - re->pat);

#ifdef KTRE_DEBUG

	for (int i = 0; i < re->ip; i++) {
		for (int j = 0; j < re->num_groups; j++) {
			if (re->group[j].address == i)
				DBG("\ngroup %d:", j);
		}

		DBG("\n%3d: [%3d] ", i, re->c[i].loc);

		switch (re->c[i].op) {
		case INSTR_CLASS: DBG("CLASS   '"); DBGF(re->c[i].class); DBG("'");   break;
		case INSTR_STR:   DBG("STR     '"); DBGF(re->c[i].class); DBG("'");   break;
		case INSTR_SPLIT:     DBG("SPLIT    %d, %d", re->c[i].a, re->c[i].b); break;
		case INSTR_NOT:       DBG("NOT     '%s'", re->c[i].class);            break;
		case INSTR_CHAR:      DBG("CHAR    '%c'", re->c[i].c);                break;
		case INSTR_SAVE:      DBG("SAVE     %d",  re->c[i].c);                break;
		case INSTR_JMP:       DBG("JMP      %d",  re->c[i].c);                break;
		case INSTR_OPT_ON:    DBG("OPTON    %d",  re->c[i].c);                break;
		case INSTR_OPT_OFF:   DBG("OPTOFF   %d",  re->c[i].c);                break;
		case INSTR_BACKREF:   DBG("BACKREF  %d",  re->c[i].c);                break;
		case INSTR_CALL:      DBG("CALL     %d",  re->c[i].a);                break;
		case INSTR_LA_NO:     DBG("LA_NO    %d",  re->c[i].a);                break;
		case INSTR_PROG:      DBG("PROG     %d",  re->c[i].a);                break;
		case INSTR_SET_START: DBG("SET_START");                               break;
		case INSTR_KILL_TP:   DBG("KILL_TP");                                 break;
		case INSTR_ANY:       DBG("ANY");                                     break;
		case INSTR_DIGIT:     DBG("DIGIT");                                   break;
		case INSTR_WORD:      DBG("WORD");                                    break;
		case INSTR_SPACE:     DBG("SPACE");                                   break;
		case INSTR_BOL:       DBG("BOL");                                     break;
		case INSTR_EOL:       DBG("EOL");                                     break;
		case INSTR_DIE:       DBG("DIE");                                     break;
		case INSTR_RET:       DBG("RET");                                     break;
		case INSTR_WB:        DBG("WB");                                      break;
		case INSTR_MATCH:     DBG("MATCH");                                   break;
		case INSTR_LA_YES:    DBG("LA_YES");                                  break;
		case INSTR_LA_WIN:    DBG("LA_WIN");                                  break;
		case INSTR_LA_FAIL:   DBG("LA_FAIL");                                 break;
		case INSTR_LB_YES:    DBG("LB_YES");                                  break;
		case INSTR_LB_NO:     DBG("LB_NO");                                   break;
		default:
 			DBG("\nunimplemented instruction printer %d\n", re->c[i].op);
			assert(false);
		}
	}
#endif

	return re;
}

#define TP (re->tp)
#define THREAD (re->t)

static void
new_thread(struct ktre *re, int ip, int sp, int opt, int fp, int la)
{
	++TP;

	if (TP >= re->thread_alloc - 1) {
		if (re->thread_alloc * 2 >= KTRE_MAX_THREAD)
			re->thread_alloc = KTRE_MAX_THREAD;
		else
			re->thread_alloc *= 2;

		re->t = KTRE_REALLOC(re->t, re->thread_alloc * sizeof THREAD[0]);
		memset(&THREAD[TP], 0, (re->thread_alloc - TP - 1) * sizeof THREAD[0]);
	}

	THREAD[TP].ip  = ip;
	THREAD[TP].sp  = sp;
	THREAD[TP].fp  = fp;
	THREAD[TP].la  = la;
	THREAD[TP].opt = opt;

	if (!THREAD[TP].vec) {
		THREAD[TP].vec = KTRE_MALLOC(re->num_groups * 2 * sizeof THREAD[TP].vec[0]);
		memset(THREAD[TP].vec, 0,    re->num_groups * 2 * sizeof THREAD[TP].vec[0]);
	}

	if (!THREAD[TP].prog) {
		THREAD[TP].prog = KTRE_MALLOC(re->num_prog * sizeof THREAD[TP].prog[0]);
		memset(THREAD[TP].prog, -1,   re->num_prog * sizeof THREAD[TP].prog[0]);
	}

	if (!THREAD[TP].frame) {
		THREAD[TP].frame = KTRE_MALLOC((fp + 1) * sizeof THREAD[TP].frame[0]);
		memset(THREAD[TP].frame, -1,   (fp + 1) * sizeof THREAD[TP].frame[0]);
	} else if (THREAD[TP].fp < fp) {
		THREAD[TP].frame = KTRE_REALLOC(THREAD[TP].frame, (fp + 1) * sizeof THREAD[TP].frame[0]);
	}

	if (!THREAD[TP].las) {
		THREAD[TP].las = KTRE_MALLOC((la + 1) * sizeof THREAD[TP].las[0]);
		memset(THREAD[TP].las, -1,   (la + 1) * sizeof THREAD[TP].las[0]);
	} else if (THREAD[TP].la < la) {
		THREAD[TP].las = KTRE_REALLOC(THREAD[TP].las, (la + 1) * sizeof THREAD[TP].las[0]);
	}

	if (TP > 0) memcpy(THREAD[TP].frame, THREAD[TP - 1].frame, THREAD[TP - 1].fp * sizeof THREAD[0].frame[0]);
	if (TP > 0) memcpy(THREAD[TP].las,   THREAD[TP - 1].las,   THREAD[TP - 1].la * sizeof THREAD[0].frame[0]);
	if (TP > 0) memcpy(THREAD[TP].vec,   THREAD[TP - 1].vec,  re->num_groups * 2 * sizeof THREAD[0].vec[0]);
	if (TP > 0) memcpy(THREAD[TP].prog,  THREAD[TP - 1].prog, re->num_prog       * sizeof THREAD[0].prog[0]);

	re->max_tp = TP > re->max_tp ? TP : re->max_tp;
}

static bool
run(struct ktre *re, const char *subject)
{
	TP = -1;

	if (!re->thread_alloc) {
		re->thread_alloc = 25;
		re->t = KTRE_MALLOC(re->thread_alloc * sizeof THREAD[0]);
		memset(re->t, 0, re->thread_alloc * sizeof THREAD[0]);
	}

	char *subject_lc = KTRE_MALLOC(strlen(subject) + 1);
	for (int i = 0; i <= (int)strlen(subject); i++)
		subject_lc[i] = lc(subject[i]);

	/* push the initial thread */
	new_thread(re, 0, 0, re->opt, 0, 0);

#ifdef KTRE_DEBUG
	int num_steps = 0;
	DBG("\n|   ip |   sp |   tp |   fp | step |");
#endif

	while (TP >= 0) {
		int ip  = THREAD[TP].ip;
		int sp  = THREAD[TP].sp;
		int fp  = THREAD[TP].fp;
		int la  = THREAD[TP].la;
		int opt = THREAD[TP].opt;
		int loc = re->c[ip].loc;

#ifdef KTRE_DEBUG
		DBG("\n| %4d | %4d | %4d | %4d | %4d | %s", ip, sp, TP, fp, num_steps, sp <= (int)strlen(subject) && sp >= 0 ? subject + sp : "");
#endif

		if (THREAD[TP].die) {
			free(subject_lc);
			return false;
		}

		switch (re->c[ip].op) {
		case INSTR_BACKREF:
			--TP;

			if (!strncmp(subject + sp, &subject[THREAD[TP].vec[re->c[ip].c * 2]], THREAD[TP].vec[re->c[ip].c * 2 + 1]))
				new_thread(re, ip + 1, sp + THREAD[TP].vec[re->c[ip].c * 2 + 1], opt, fp, la);
			break;

		case INSTR_CLASS:
			--TP;

			if (strchr(re->c[ip].class, subject[sp]) && subject[sp])
				new_thread(re, ip + 1, sp + 1, opt, fp, la);
			break;

		case INSTR_STR:
			--TP;

			if (opt & KTRE_INSENSITIVE) {
				if (!strncmp(subject_lc + sp, re->c[ip].class, strlen(re->c[ip].class)))
					new_thread(re, ip + 1, sp + strlen(re->c[ip].class), opt, fp, la);
			} else {
				if (!strncmp(subject + sp, re->c[ip].class, strlen(re->c[ip].class)))
					new_thread(re, ip + 1, sp + strlen(re->c[ip].class), opt, fp, la);
			}
			break;

		case INSTR_NOT:
			--TP;

			if (!strchr(re->c[ip].class, subject[sp]))
				new_thread(re, ip + 1, sp + 1, opt, fp, la);
			break;

		case INSTR_BOL:
			--TP;
			if (sp == 0)
				new_thread(re, ip + 1, sp, opt, fp, la);
			break;

		case INSTR_EOL:
			--TP;

			if (subject[sp] == 0 || subject[sp] == '\n')
				new_thread(re, ip + 1, sp, opt, fp, la);
			break;

		case INSTR_WB:
			--TP;

			if (sp &&
			    ((strchr(WORD, subject[sp - 1]) && !strchr(WORD, subject[sp]))
			     || (!strchr(WORD, subject[sp - 1]) && strchr(WORD, subject[sp]))))
				new_thread(re, ip + 1, sp, opt, fp, la);
			else if (sp == 0 || sp == (int)strlen(subject))
				new_thread(re, ip + 1, sp, opt, fp, la);
			break;

		case INSTR_CHAR:
			--TP;

			if (opt & KTRE_INSENSITIVE) {
				if (lc(subject[sp]) == lc(re->c[ip].c)) {
					new_thread(re, ip + 1, sp + 1, opt, fp, la);
				}
			} else {
				if (subject[sp] == re->c[ip].c) {
					new_thread(re, ip + 1, sp + 1, opt, fp, la);
				}
			}
			break;

		case INSTR_ANY:
			--TP;

			if (subject[sp])
				new_thread(re, ip + 1, sp + 1, opt, fp, la);
			break;

		case INSTR_SPLIT:
			THREAD[TP].ip = re->c[ip].b;
			new_thread(re, re->c[ip].a, sp, opt, fp, la);
			break;

		case INSTR_MATCH:
			if ((opt & KTRE_UNANCHORED) == 0) {
				if (!subject[sp]) {
					free(subject_lc);
					return true;
				}
			} else {
				free(subject_lc);
				return true;
			}

			--TP;
			break;

		case INSTR_SAVE:
			--TP;

			if (re->c[ip].c % 2 == 0)
				THREAD[TP].vec[re->c[ip].c] = sp;
			else
				THREAD[TP].vec[re->c[ip].c] = sp - THREAD[TP].vec[re->c[ip].c - 1];
			new_thread(re, ip + 1, sp, opt, fp, la);
			break;

		case INSTR_JMP:
			THREAD[TP].ip = re->c[ip].c;
			break;

		case INSTR_OPT_ON:
			THREAD[TP].opt |= re->c[ip].c;
			THREAD[TP].ip++;
			break;

		case INSTR_OPT_OFF:
			THREAD[TP].opt &= ~re->c[ip].c;
			THREAD[TP].ip++;
			break;

		case INSTR_KILL_TP: {
			new_thread(re, ip, sp, opt, fp, la);
			THREAD[TP].die = true;
			new_thread(re, ip + 1, sp, opt, fp, la);
		} break;

		case INSTR_DIE:
			free(subject_lc);
			return false;

		case INSTR_SET_START:
			THREAD[TP].vec[0] = sp;
			THREAD[TP].ip++;
			break;

		case INSTR_CALL:
			--TP;

			new_thread(re, re->c[ip].c, sp, opt, fp + 1, la);
			THREAD[TP].frame[fp] = ip + 1;
			break;

		case INSTR_RET:
			--TP;

			new_thread(re, THREAD[TP + 1].frame[fp - 1], sp, opt, fp - 1, la);
			break;

		case INSTR_LA_YES:
			--TP;

			new_thread(re, ip + 1, sp, opt, fp, la + 1);
			THREAD[TP].las[la] = sp;
			break;

		case INSTR_LA_NO:
			THREAD[TP].ip = re->c[ip].c;
			new_thread(re, ip + 1, sp, opt, fp, la);
			break;

		case INSTR_LA_WIN:
			THREAD[TP].sp = THREAD[TP].las[--THREAD[TP].la];
			THREAD[TP].ip++;
			break;

		case INSTR_LA_FAIL:
			--TP;

			break;

		case INSTR_LB_YES:
			--TP;

			new_thread(re, ip + 1, 0, opt, fp, la);
			break;

		case INSTR_LB_NO:
			--TP;

			new_thread(re, re->c[ip].c, 0, opt, fp, la);
			new_thread(re, ip + 1, 0, opt, fp, la);
			break;

		case INSTR_PROG:
			--TP;

			if (THREAD[TP].prog[re->c[ip].a] == sp) {
				--TP;
			} else {
				THREAD[TP].prog[re->c[ip].a] = sp;
				new_thread(re, ip + 1, sp, opt, fp, la);
			}
			break;

		case INSTR_DIGIT:
			--TP;

			if (strchr(DIGIT, subject[sp]) && subject[sp])
				new_thread(re, ip + 1, sp + 1, opt, fp, la);
			break;

		case INSTR_WORD:
			--TP;

			if (strchr(WORD, subject[sp]) && subject[sp])
				new_thread(re, ip + 1, sp + 1, opt, fp, la);
			break;

		case INSTR_SPACE:
			--TP;

			if (strchr(WHITESPACE, subject[sp]) && subject[sp])
				new_thread(re, ip + 1, sp + 1, opt, fp, la);
			break;

		default:
#ifdef KTRE_DEBUG
			DBG("\nunimplemented instruction %d\n", re->c[ip].op);
			assert(false);
#endif
			free(subject_lc);
			return false;
		}

		if (TP == KTRE_MAX_THREAD - 1) {
			error(re, KTRE_ERROR_STACK_OVERFLOW, loc, "regex exceeded the maximum number of executable threads");
			free(subject_lc);
			return false;
		}

		if (fp == KTRE_MAX_CALL_DEPTH - 1) {
			error(re, KTRE_ERROR_CALL_OVERFLOW, loc, "regex exceeded the maximum depth for subroutine calls");
			free(subject_lc);
			return false;
		}

#ifdef KTRE_DEBUG
		num_steps++;
#endif
	}

	free(subject_lc);
	return false;
}

void
ktre_free(struct ktre *re)
{
	free_node(re->n);
	KTRE_FREE(re->c);
	if (re->err) KTRE_FREE(re->err_str);
	KTRE_FREE(re->group);

	for (int i = 0; i <= re->max_tp; i++) {
		KTRE_FREE(THREAD[i].vec);
		KTRE_FREE(THREAD[i].prog);
		KTRE_FREE(THREAD[i].frame);
		KTRE_FREE(THREAD[i].las);
	}

	KTRE_FREE(re->t);
	KTRE_FREE(re);
}

_Bool
ktre_exec(struct ktre *re, const char *subject, int **vec)
{
	if (re->err) {
		KTRE_FREE(re->err_str);
		re->err = 0;
	}

	if (run(re, subject)) {
		*vec = THREAD[re->tp].vec;
		return true;
	} else {
		return false;
	}
}
#endif /* ifdef KTRE_IMPLEMENTATION */
#endif /* ifndef KTRE_H */
