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
 * ktre: a tiny regex engine
 * ktre supports character classes [a-zA-Z_], submatching and backreferencing
 * (cat|dog)\1, negated character classes [^a-zA-Z_], a variety of escaped
 * metacharacters (\w, \W, \d, \D, \s), mode modifiers (?ix), comments
 * (?#this is a comment), free-spaced expressions, and, of course, the full
 * set of metacharacters and operators you would expect: * + ? . ^ and $,
 * including their non-greedy variants: *? +? and ??.
 *
 * ktre currently supports the following options: KTRE_INSENSITIVE,
 * KTRE_UNANCHORED, and KTRE_EXTENDED. All of those options except for
 * KTRE_UNANCHORED may be modified by the regular expression. The options are
 * reset every time the regex is run.
 *
 * KTRE_INSENSITIVE: (?i)
 *     Makes the runtime match characters of different cases.
 * KTRE_UNANCHORED:
 *     Counts a match anywhere in the subject as a match. The default behavior
 *     is to reject any string that does not match exactly.
 * KTRE_EXTENDED: (?x)
 *     This option turns on so-called 'free-spaced' mode, which allows
 *     whitespace to occur in most parts of the grammar. Note that this does
 *     not allow whitespace anywhere; '(?#foobar)' is not the same thing as
 *     '( ?#foobar)', but 'foobar' is the same as 'foo bar'. If you want to
 *     match a whitespace character in free-spaced mode, you may escape
 *     it: '\ '.
 *
 * The API consists of only three functions: ktre_compile(), ktre_exec(), and
 * ktre_free(). The prototypes for these functions are:
 *
 * struct ktre *ktre_compile(const char *pat, int opt);
 * _Bool ktre_exec(struct ktre *re, const char *subject, int **vec);
 * void ktre_free(struct ktre *re);
 *
 * Here's a usage example:
 *
 * #include <stdlib.h>
 * #include <stdio.h>
 *
 * #define KTRE_DEBUG
 * #define KTRE_IMPLEMENTATION
 * #include "ktre.h"
 *
 * int
 * main(int argc, char *argv[])
 * {
 * 	if (argc != 3) {
 * 		fprintf(stderr, "Usage: ktre [subject] [pattern]\n");
 * 		exit(EXIT_FAILURE);
 * 	}
 *
 * 	char *subject = argv[1], *regex = argv[2];
 * 	fprintf(stderr, "matching string '%s' against regex '%s'", subject, regex);
 *
 * 	int *vec = NULL;
 * 	struct ktre *re = ktre_compile(regex, KTRE_INSENSITIVE | KTRE_UNANCHORED);
 *
 * 	if (re->err) { // failed to compile
 * 		fprintf(stderr, "\nregex failed to compile with error code %d: %s\n", re->err, re->err_str);
 * 		fprintf(stderr, "\t%s\n\t", regex);
 * 		for (int i = 0; i < re->loc; i++) fputc(' ', stderr);
 * 		fprintf(stderr, "^\n");
 * 	} else if (ktre_exec(re, subject, &vec)) { // ran and succeeded
 * 		fprintf(stderr, "\nmatched!");
 *
 * 		for (int i = 0; i < re->num_groups; i++)
 * 			fprintf(stderr, "\ngroup %d: '%.*s'", i, vec[i * 2 + 1], &subject[vec[i * 2]]);
 *
 * 		fputc('\n', stderr);
 * 		free(vec);
 * 	} else if (re->err) { // ran, but failed with an error
 * 		fprintf(stderr, "\nregex failed at runtime with error code %d: %s\n", re->err, re->err_str);
 *
 * 		fprintf(stderr, "\t%s\n\t", regex);
 * 		for (int i = 0; i < re->loc; i++) putchar(' ');
 * 		fprintf(stderr, "^\n");
 * 	} else { // ran, but failed
 * 		fprintf(stderr, "\ndid not match!\n");
 * 	}
 *
 * 	ktre_free(re);
 *
 * 	return EXIT_SUCCESS;
 * }
 */

#ifndef KTRE_H
#define KTRE_H

enum ktre_error {
	KTRE_ERROR_NO_ERROR,
	KTRE_ERROR_UNMATCHED_PAREN,
	KTRE_ERROR_UNTERMINATED_CLASS,
	KTRE_ERROR_STACK_OVERFLOW,
	KTRE_ERROR_STACK_UNDERFLOW,
	KTRE_ERROR_INVALID_MODE_MODIFIER,
	KTRE_ERROR_INVALID_RANGE,
	KTRE_ERROR_INVALID_BACKREFERENCE
};

struct ktre {
	/* public fields */
	int num_groups, opt;
	/* string containing an error message in the case
	 * of failure during parsing or compilation */
	char *err_str;
	enum ktre_error err; /* error status code */
	_Bool failed;
	/* the location of any error that occurred, as an index
	 * in the last subject the regex was run on */
	int loc;

	/* implementation details */
	struct instr *c; /* code */
	int ip;          /* instruction pointer */
	char const *pat;
	char const *sp;  /* pointer to the character currently being parsed */
	struct node *n;
};

enum {
	KTRE_INSENSITIVE = 1 << 0,
	KTRE_UNANCHORED  = 1 << 1,
	KTRE_EXTENDED  = 1 << 2,
};

struct ktre *ktre_compile(const char *pat, int opt);
_Bool ktre_exec(struct ktre *re, const char *subject, int **vec);
void ktre_free(struct ktre *re);

#ifdef KTRE_IMPLEMENTATION
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef KTRE_DEBUG
#include <assert.h>
#define DBG(...) fprintf(stderr, __VA_ARGS__)
#endif

static void *
_malloc(size_t n)
{
	void *a = malloc(n);

	if (!a) {
		fprintf(stderr, "ktre: out of memory\n");
		exit(EXIT_FAILURE);
	}

	return a;
}

static void *
_calloc(size_t n)
{
	void *a = calloc(n, 1);

	if (!a) {
		fprintf(stderr, "ktre: out of memory\n");
		exit(EXIT_FAILURE);
	}

	return a;
}

static void *
_realloc(void *ptr, size_t n)
{
	void *a = realloc(ptr, n);

	if (!a) {
		fprintf(stderr, "ktre: out of memory\n");
		exit(EXIT_FAILURE);
	}

	return a;
}

struct instr {
	enum {
		INSTR_MATCH,
		INSTR_CHAR,
		INSTR_JP,
		INSTR_SPLIT,
		INSTR_ANY,
		INSTR_CLASS,
		INSTR_NOT,
		INSTR_BACKREFERENCE,
		INSTR_BOL,
		INSTR_EOL,
		INSTR_OPT_ON,
		INSTR_OPT_OFF,
		INSTR_SAVE
	} op;

	union {
		struct {
			int a, b;
		};
		int c;
		char *class;
	};
};

static void
emit_ab(struct ktre *re, int instr, int a, int b)
{
	re->c = _realloc(re->c, sizeof re->c[0] * (re->ip + 1));
	re->c[re->ip].op = instr;

	re->c[re->ip].a = a;
	re->c[re->ip].b = b;

	re->ip++;
}

static void
emit_c(struct ktre *re, int instr, int c)
{
	re->c = _realloc(re->c, sizeof re->c[0] * (re->ip + 1));
	re->c[re->ip].op = instr;
	re->c[re->ip].c = c;

	re->ip++;
}

static void
emit_class(struct ktre *re, int instr, char *class)
{
	re->c = _realloc(re->c, sizeof re->c[0] * (re->ip + 1));
	re->c[re->ip].op = instr;
	re->c[re->ip].class = class;

	re->ip++;
}

static void
emit(struct ktre *re, int instr)
{
	re->c = _realloc(re->c, sizeof re->c[0] * (re->ip + 1));
	re->c[re->ip].op = instr;
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
		NODE_ANY,
		NODE_CLASS,
		NODE_NOT,
		NODE_BACKREFERENCE,
		NODE_BOL,
		NODE_EOL,
		NODE_OPT_ON,
		NODE_OPT_OFF,
		NODE_REP,
		NODE_NONE
	} type;

	struct {
		struct {
			struct node *a, *b;
		};
		struct {
			int c, d;
		};
		char *class;
	};

	int loc;
};

#define NEXT                             \
	do {                             \
		if (*re->sp) re->sp++; \
	} while (0)

#define MAX_ERROR_LEN 256 /* some arbitrary limit */

#include <stdarg.h>
static void
error(struct ktre *re, enum ktre_error err, int loc, char *fmt, ...)
{
	re->failed = true;
	re->err = err;
	re->err_str = _malloc(MAX_ERROR_LEN);
	re->loc = loc;

	va_list args;
	va_start(args, fmt);
	vsnprintf(re->err_str, MAX_ERROR_LEN, fmt, args);
	va_end(args);
}

static struct node *parse(struct ktre *re);

static char *
append_to_str(char *class, char c)
{
	size_t len = class ? strlen(class) : 0;
	class = _realloc(class, len + 2);
	class[len] = c;
	class[len + 1] = 0;
	return class;
}

static char *
append_str_to_str(char *class, const char *c)
{
	size_t len = class ? strlen(class) : 0;
	class = _realloc(class, len + strlen(c) + 1);
	strcpy(class + len, c);
	return class;
}

static char *
strclone(const char *str)
{
	char *ret = _malloc(strlen(str) + 1);
	strcpy(ret, str);
	return ret;
}

static struct node *
factor(struct ktre *re)
{
	struct node *left = _malloc(sizeof *left);
	left->loc = 0;

again:
	/* parse a primary */
	switch (*re->sp) {
	case '\\': /* escape sequences */
		NEXT;
		switch (*re->sp) {
		case 's':
			left->type = NODE_CLASS;
			left->class = strclone(" \t\r\n\v\f");
			NEXT;
			break;
		case 'S':
			left->type = NODE_NOT;
			left->class = strclone(" \t\r\n\v\f");
			NEXT;
			break;
		case 'd':
			left->type = NODE_CLASS;
			left->class = strclone("0123456789");
			NEXT;
			break;
		case 'D':
			left->type = NODE_NOT;
			left->class = strclone("0123456789");
			NEXT;
			break;
		case 'w':
			left->type = NODE_CLASS;
			left->class = strclone("_0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ");
			NEXT;
			break;
		case 'W':
			left->type = NODE_NOT;
			left->class = strclone("_0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ");
			NEXT;
			break;
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			left->type = NODE_BACKREFERENCE;
			left->c = *re->sp - '0';
			if (isdigit(re->sp[1])) {
				left->c *= 10;
				left->c += re->sp[1] - '0';
				NEXT;
			}
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
					class = append_to_str(class, i);
				}
				re->sp += 3;
			} else if (*re->sp == '\\') {
				NEXT;

				switch (*re->sp) {
				case 's':
					append_str_to_str(class, " \t\r\n\v\f");
					re->sp++;
					break;
				case 'd':
					append_str_to_str(class, "0123456789");
					re->sp++;
					break;
				case 'w':
					append_str_to_str(class, "_0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ");
					re->sp++;
					break;
				case 'n':
					append_to_str(class, '\n');
					re->sp++;
				}
			} else {
				class = append_to_str(class, *re->sp);
				re->sp++;
			}
		}

		if (*re->sp != ']') {
			error(re, KTRE_ERROR_UNTERMINATED_CLASS, loc, "unterminated character class");
		}

		left->class = class;
		NEXT;
	} break;

	case '(':
		NEXT;

		if (*re->sp == '?' && re->sp[1] == '#') { /* comments */
			while (*re->sp && *re->sp != ')') re->sp++;
			if (*re->sp != ')') {
				error(re, KTRE_ERROR_UNMATCHED_PAREN, re->sp - re->pat, "unmatched '(' at character %d", re->sp - re->pat);
				free(left->a);
				left->type = NODE_NONE;
				return left;
			}
			left->type = NODE_NONE;
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
					error(re, KTRE_ERROR_INVALID_MODE_MODIFIER, re->sp - re->pat, "invalid mode modifier at character %d", re->sp - re->pat);
					left->type = NODE_NONE;
					return left;
				}

				struct node *tmp = _malloc(sizeof *tmp);
				tmp->type = NODE_SEQUENCE;
				tmp->a = left;
				tmp->b = _malloc(sizeof *tmp->b);
				tmp->b->type = off || neg ? NODE_OPT_OFF : NODE_OPT_ON;
				tmp->b->c = opt;
				left = tmp;

				off = false;
				re->sp++;
			}

			if (*re->sp != ')') {
				error(re, KTRE_ERROR_UNMATCHED_PAREN, re->sp - re->pat, "unmatched '(' at character %d", re->sp - re->pat);
				free(left->a);
				left->type = NODE_NONE;
				return left;
			}
		} else {
			left->type = NODE_GROUP;
			left->a = parse(re);

			if (*re->sp != ')') {
				error(re, KTRE_ERROR_UNMATCHED_PAREN, re->sp - re->pat - 1, "unmatched '(' at character %d", re->sp - re->pat - 1);
				free(left->a);
				left->type = NODE_NONE;
				return left;
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
	default:
		/* ignore whitespace if we're in extended mode */
		if (re->opt & KTRE_EXTENDED && strchr(" \t\r\n\v\f", *re->sp)) {
			NEXT;
			goto again;
		}

		left->type = NODE_CHAR;
		left->c = *re->sp;
		NEXT;
	}

	while (*re->sp && (*re->sp == '*' || *re->sp == '+' || *re->sp == '?' || *re->sp == '{')) {
		NEXT;
		struct node *n = _malloc(sizeof *n);

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
						left->type = NODE_NONE;
						return left;
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
						left->type = NODE_NONE;
						return left;
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
						left->type = NODE_NONE;
						return left;
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
		left = n;
	}

	left->loc = re->sp - re->pat - 1;

	return left;
}

static struct node *
term(struct ktre *re)
{
	struct node *left = _malloc(sizeof *left);
	left->type = NODE_NONE;

	while (*re->sp && *re->sp != '|' && *re->sp != ')') {
		struct node *right = factor(re);

		if (left->type == NODE_NONE) {
			free(left);
			left = right;
		} else {
			struct node *tmp = _malloc(sizeof *tmp);
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
		NEXT;

		struct node *m = _malloc(sizeof *m);
		m->type = NODE_OR;
		m->a = n;
		m->b = parse(re);
		return m;
	}

	return n;
}

/* freenode? */
static void
free_node(struct node *n)
{
	switch (n->type) {
	case NODE_SEQUENCE: case NODE_OR:
		free_node(n->a);
		free_node(n->b);
		break;
	case NODE_ASTERISK: case NODE_PLUS:
	case NODE_GROUP: case NODE_QUESTION:
		free_node(n->a);
	default: break;
	}

	free(n);
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

#define pnode(x) l++; print_node(x); l--
#define N(...) DBG(__VA_ARGS__); pnode(n->a)

	switch (n->type) {
	case NODE_SEQUENCE:
		DBG("(sequence)");
		l++;
		split;
		print_node(n->a);
		join;
		print_node(n->b);
		l--;
		break;
	case NODE_OR: DBG("(or)");
		l++;
		split;
		print_node(n->a); join;
		print_node(n->b);
		l--;
		break;
	case NODE_ASTERISK: N("(asterisk)");                         break;
	case NODE_PLUS:     N("(plus)");                             break;
	case NODE_GROUP:    N("(group)");                            break;
	case NODE_QUESTION: N("(question)");                         break;
	case NODE_ANY:      DBG("(any)");                            break;
	case NODE_NONE:     DBG("(none)");                           break;
	case NODE_CHAR:     DBG("(char '%c')", n->c);                break;
	case NODE_BACKREFERENCE: DBG("(backreference to %d)", n->c); break;
	case NODE_CLASS:    DBG("(class '%s')", n->class);           break;
	case NODE_NOT:      DBG("(not '%s')", n->class);             break;
	case NODE_BOL:      DBG("(bol)");                            break;
	case NODE_EOL:      DBG("(eol)");                            break;
	case NODE_OPT_ON:   DBG("(opt on %d)", n->c);                break;
	case NODE_OPT_OFF:  DBG("(opt off %d)", n->c);               break;
	case NODE_REP:
		DBG("(counted repetition %d - %d)", n->c, n->d);
		l++;
		print_node(n->a);
		l--;
		break;
	default:
		DBG("\nunimplemented printer for node of type %d\n", n->type);
		assert(false);
	}
#undef N
}
#endif

static void
compile(struct ktre *re, struct node *n)
{
#define patch_a(_re, _a, _c) _re->c[_a].a = _c;
#define patch_b(_re, _a, _c) _re->c[_a].b = _c;
#define patch_c(_re, _a, _c) _re->c[_a].c = _c;

	switch (n->type) {
	case NODE_SEQUENCE:
		compile(re, n->a);
		compile(re, n->b);
		break;

	case NODE_CHAR:
		emit_c(re, INSTR_CHAR, n->c);
		break;

	case NODE_ASTERISK: {
		int a = re->ip;
		emit_ab(re, INSTR_SPLIT, re->ip + 1, -1);
		compile(re, n->a);
		emit_ab(re, INSTR_SPLIT, a + 1, re->ip + 1);
		patch_b(re, a, re->ip);
	} break;

	case NODE_QUESTION: {
		int a;

		switch (n->a->type) {
		case NODE_ASTERISK:
			a = re->ip;
			emit_ab(re, INSTR_SPLIT, -1, re->ip + 1);
			compile(re, n->a->a);
			emit_ab(re, INSTR_SPLIT, re->ip + 1, a + 1);
			patch_a(re, a, re->ip);
			break;
		case NODE_PLUS:
			a = re->ip;
			compile(re, n->a->a);
			emit_ab(re, INSTR_SPLIT, re->ip + 1, a);
			break;
		case NODE_QUESTION:
			a = re->ip;
			emit_ab(re, INSTR_SPLIT, -1, re->ip + 1);
			compile(re, n->a->a);
			patch_a(re, a, re->ip);
			break;
		default:
			a = re->ip;
			emit_ab(re, INSTR_SPLIT, re->ip + 1, -1);
			compile(re, n->a);
			patch_b(re, a, re->ip);
		}
	} break;

	case NODE_GROUP: {
		emit_c(re, INSTR_SAVE, re->num_groups * 2);
		int old = re->num_groups;
		re->num_groups++;
		compile(re, n->a);
		emit_c(re, INSTR_SAVE, old * 2 + 1);
	} break;

	case NODE_ANY:
		emit(re, INSTR_ANY);
		break;

	case NODE_PLUS: {
		int a = re->ip;
		compile(re, n->a);
		emit_ab(re, INSTR_SPLIT, a, re->ip + 1);
	} break;

	case NODE_OR: {
		int a = re->ip;
		emit_ab(re, INSTR_SPLIT, re->ip + 1, -1);
		compile(re, n->a);
		int b = re->ip;
		emit_c(re, INSTR_JP, -1);
		patch_b(re, a, re->ip);
		compile(re, n->b);
		patch_c(re, b, re->ip);
	} break;

	case NODE_CLASS:
		emit_class(re, INSTR_CLASS, n->class);
		break;
	case NODE_NOT:
		emit_class(re, INSTR_NOT, n->class);
		break;
	case NODE_BOL:
		emit(re, INSTR_BOL);
		break;
	case NODE_EOL:
		emit(re, INSTR_EOL);
		break;
	case NODE_NONE:
		/* these nodes are created by comments, mode modifiers, and some other things. */
		break;
	case NODE_OPT_ON:
		emit_c(re, INSTR_OPT_ON, n->c);
		break;
	case NODE_OPT_OFF:
		emit_c(re, INSTR_OPT_OFF, n->c);
		break;
	case NODE_BACKREFERENCE:
		if (n->c <= 0 || n->c - 1 >= re->num_groups) {
			error(re, KTRE_ERROR_INVALID_BACKREFERENCE, n->loc, "backreference number is invalid or references a group that does not yet exist");
		return;
		}
		emit_c(re, INSTR_BACKREFERENCE,
		       n->c);
		break;
	case NODE_REP: {
		int a = 0;
		for (int i = 0; i < n->c; i++) {
			a = re->ip;
			compile(re, n->a);
		}

		if (n->d == 0) {
			emit_ab(re, INSTR_SPLIT, a, re->ip + 1);
		} else {
			/* this will fail to do anything if n->d is less than n->c,
			 * which handles the case where n->d == -1.
			 * n->d == -1 when the repetition is of the form {n}. */
			for (int i = 0; i < n->d - n->c; i++) {
				a = re->ip;
				emit_ab(re, INSTR_SPLIT, re->ip + 1, -1);
				compile(re, n->a);
				patch_b(re, a, re->ip);
			}
		}
	} break;

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
	struct ktre *re = _calloc(sizeof *re);
	re->pat = pat, re->opt = opt, re->sp = pat;
	re->err_str = "no error";

#ifdef KTRE_DEBUG
	DBG("\noptions set:");
	for (size_t i = 0; i < sizeof opt; i++) {
		switch (opt & 1 << i) {
		case KTRE_INSENSITIVE: DBG("\n\tINSENSITIVE"); break;
		case KTRE_UNANCHORED: DBG("\n\tUNANCHORED"); break;
		case KTRE_EXTENDED: DBG("\n\tEXTENDED"); break;
		default: continue;
		}
	}
#endif

	re->n = parse(re);
	if (re->failed) {
		free_node(re->n);
		return re;
	}

	/* just emit the bytecode for .*? */
	if (re->opt & KTRE_UNANCHORED) {
		emit_ab(re, INSTR_SPLIT, 3, 1);
		emit(re, INSTR_ANY);
		emit_ab(re, INSTR_SPLIT, 3, 1);
	}

	/* the entire match is grouped */
	emit_c(re, INSTR_SAVE, 0);
	re->num_groups++;

	compile(re, re->n);
	if (re->failed)
		return re;

	emit_c(re, INSTR_SAVE, 1);

	/* just emit the bytecode for .* */
	if (re->opt & KTRE_UNANCHORED) {
		emit_ab(re, INSTR_SPLIT, re->ip + 1, re->ip + 3);
		emit(re, INSTR_ANY);
		emit_ab(re, INSTR_SPLIT, re->ip - 1, re->ip + 1);
	}

	emit(re, INSTR_MATCH);

#ifdef KTRE_DEBUG
	print_node(re->n);

	for (int i = 0; i < re->ip; i++) {
		DBG("\n%3d: ", i);

		switch (re->c[i].op) {
		case INSTR_CHAR:   DBG("CHAR   '%c'", re->c[i].c);                break;
		case INSTR_SPLIT:  DBG("SPLIT %3d, %3d", re->c[i].a, re->c[i].b); break;
		case INSTR_ANY:    DBG("ANY");                                    break;
		case INSTR_MATCH:  DBG("MATCH");                                  break;
		case INSTR_SAVE:   DBG("SAVE  %3d", re->c[i].c);                  break;
		case INSTR_JP:     DBG("JP    %3d", re->c[i].c);                  break;
		case INSTR_CLASS:  DBG("CLASS '%s'", re->c[i].class);             break;
		case INSTR_NOT:    DBG("NOT   '%s'", re->c[i].class);             break;
		case INSTR_BOL:    DBG("BOL");                                    break;
		case INSTR_EOL:    DBG("EOL");                                    break;
		case INSTR_OPT_ON: DBG("OPTON   %d", re->c[i].c);                 break;
		case INSTR_OPT_OFF:DBG("OPTOFF  %d", re->c[i].c);                 break;
		case INSTR_BACKREFERENCE: DBG("BACKREF %d", re->c[i].c);          break;
		default: assert(false);
		}
	}
#endif
	free_node(re->n);

	return re;
}

static inline char
lc(char c)
{
	if (c >= 'A' && c <= 'Z') {
		c = (c - 'A') + 'a';
	}

	return c;
}

#define MAX_THREAD (1 << 11) /* 2048 should be enough */
static bool
run(struct ktre *re, const char *subject, int *vec)
{
	struct thread {
		int ip, sp;
		int old, old_idx, opt;
	} t[MAX_THREAD];
	int tp = 0; /* toilet paper */

#define new_thread(_ip, _sp, _opt) \
	t[++tp] = (struct thread){ _ip, _sp, -1, -1, _opt }

	/* push the initial thread */
	new_thread(0, 0, re->opt);

	while (tp) {
		int ip = t[tp].ip, sp = t[tp].sp, opt = t[tp].opt;
//		printf("\nip: %d, sp: %d, tp: %d", ip, sp, tp);

		if (tp == MAX_THREAD - 1) {
			error(re, KTRE_ERROR_STACK_OVERFLOW, 0, "regex exceeded the maximum number of executable threads");
			return false;
		}

		if (tp <= 0) {
			error(re, KTRE_ERROR_STACK_UNDERFLOW, 0, "regex killed more threads than it started");
			return false;
		}

		switch (re->c[ip].op) {
		case INSTR_BACKREFERENCE:
			tp--;
			if (!strncmp(subject + sp, &subject[vec[re->c[ip].c * 2]], vec[re->c[ip].c * 2 + 1]))
				new_thread(ip + 1, sp + vec[re->c[ip].c * 2 + 1], opt);
			break;

		case INSTR_CLASS:
			tp--;
			if (strchr(re->c[ip].class, subject[sp]))
				new_thread(ip + 1, sp + 1, opt);
			break;

		case INSTR_NOT:
			tp--;
			if (!strchr(re->c[ip].class, subject[sp]))
				new_thread(ip + 1, sp + 1, opt);
			break;

		case INSTR_BOL:
			tp--;
			if (sp == 0) new_thread(ip + 1, sp, opt);
			break;

		case INSTR_EOL:
			tp--;
			if (subject[sp] == 0 || subject[sp] == '\n') new_thread(ip + 1, sp, opt);
			break;

		case INSTR_CHAR:
			tp--;
			if (opt & KTRE_INSENSITIVE) {
				if (lc(subject[sp]) == lc(re->c[ip].c)) {
				  new_thread(ip + 1, sp + 1, opt);
				}
			} else {
				if (subject[sp] == re->c[ip].c) {
				  new_thread(ip + 1, sp + 1, opt);
				}
			}
			break;

		case INSTR_ANY:
			tp--;
			if (subject[sp] != 0) new_thread(ip + 1, sp + 1, opt);
			break;

		case INSTR_SPLIT:
			t[tp].ip = re->c[ip].b;
			new_thread(re->c[ip].a, sp, opt);
			break;

		case INSTR_MATCH:
			if (subject[sp] == 0)
				return true;
			tp--;
			break;

		case INSTR_SAVE:
			if (t[tp].old == -1) {
				t[tp].old = vec[re->c[ip].c];
				t[tp].old_idx = re->c[ip].c;
				if (re->c[ip].c % 2 == 0)
					vec[re->c[ip].c] = sp;
				else
					vec[re->c[ip].c] = sp - vec[re->c[ip].c - 1];
				new_thread(ip + 1, sp, opt);
			} else {
				vec[t[tp].old_idx] = t[tp].old;
				tp--;
			}
			break;

		case INSTR_JP:
			t[tp].ip = re->c[ip].c;
			break;
		case INSTR_OPT_ON:
			t[tp].opt |= re->c[ip].c;
			t[tp].ip++;
			break;
		case INSTR_OPT_OFF:
			t[tp].opt &= ~re->c[ip].c;
			t[tp].ip++;
			break;
		default:
#ifdef KTRE_DEBUG
			DBG("\nunimplemented instruction %d\n", re->c[ip].op);
			assert(false);
#endif
			return false;
		}
	}

	return false;
}

void
ktre_free(struct ktre *re)
{
	for (int i = 0; i < re->ip; i++)
		if (re->c[i].op == INSTR_CLASS)
			free(re->c[i].class);

	free(re->c);
	if (re->err) free(re->err_str);
	free(re);
}

_Bool
ktre_exec(struct ktre *re, const char *subject, int **vec)
{
	if (re->err) {
		free(re->err_str);
		re->err = 0;
	}

	*vec = _calloc(sizeof (*vec[0]) * re->num_groups * 2);

	if (run(re, subject, *vec)) {
		return true;
	} else {
		free(*vec);
		return false;
	}
}
#endif /* ifdef KTRE_IMPLEMENTATION */
#endif /* ifndef KTRE_H */
