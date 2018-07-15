#ifndef KTRE_H
#define KTRE_H

#include "kdgu.h"

/* Error codes. */
enum ktre_error {
	KTRE_ERROR_NO_ERROR,
	KTRE_ERROR_STACK_OVERFLOW,
	KTRE_ERROR_CALL_OVERFLOW,
	KTRE_ERROR_SYNTAX_ERROR,
	KTRE_ERROR_OUT_OF_MEMORY,
	KTRE_ERROR_TOO_MANY_GROUPS,
	KTRE_ERROR_INVALID_OPTIONS
};

/* Options. */
enum {
	KTRE_INSENSITIVE = 1 << 0,
	KTRE_UNANCHORED  = 1 << 1,
	KTRE_EXTENDED    = 1 << 2,
	KTRE_GLOBAL      = 1 << 3,
	KTRE_MULTILINE   = 1 << 4,
	KTRE_CONTINUE    = 1 << 5,
	KTRE_DEBUG       = 1 << 6,
	KTRE_ECMA        = 1 << 7
};

/* Compile-time settings. */
#define KTRE_MAX_ERROR_LEN 100
#define KTRE_MAX_GROUPS 100
#define KTRE_MAX_THREAD 2000
#define KTRE_MAX_CALL_DEPTH 100

struct ktre {
	/* ===================== public fields ==================== */
	unsigned num_matches;
	int num_groups;
	int opt;

	/*
	 * String containing an error message in the case of failure
	 * during parsing or compilation.
	 */
	char *err_str;
	enum ktre_error err; /* Error status code                   */

	/*
	 * The location of any error that occurred, as an index in the
	 * last subject the regex was run on.
	 */
	int loc;

	/* ==================== private fields ==================== */
	const kdgu *s;   /* The pattern                             */
	unsigned i;      /* The current character being parsed      */
	struct instr *c; /* Code                                    */
	int ip;          /* Instruction pointer                     */
	int num_prog;    /* Number of progress instructions         */
	int popt;        /* The options as seen by the parser       */
	int gp;          /* Group pointer                           */
	struct node *n;  /* The head node of the ast                */
	_Bool literal;   /* Whether to escape metacharacters or not */
	int cont;

	struct group {
		int address;

		/*
		 * These boolean flags indicate whether this group
		 * has been compiled or called as a subroutine,
		 * respectively. The is_called field is flexible; it's
		 * used whenever a group must be called as a
		 * subroutine, which a counted repetition may do.
		 */
		_Bool is_compiled;
		_Bool is_called;
		kdgu *name;
	} *group;

	/* runtime */
	struct thread {
		unsigned ip, sp, fp, la, ep, opt;
		int *frame, *vec, *prog, *las, *exception;
		_Bool die, rev;
	} *t;

	int tp, max_tp;
	int **vec;

	_Bool copied;
	int instr_alloc, thread_alloc;
};

typedef struct ktre ktre;

/* API prototypes. */
ktre *ktre_compile(const kdgu *pat, int opt);
ktre *ktre_copy(ktre *re);
_Bool ktre_exec(ktre *re, const kdgu *subject, int ***vec);
_Bool ktre_match(const kdgu *subject, const kdgu *pat, int opt, int ***vec);
kdgu *ktre_filter(ktre *re, const kdgu *subject, const kdgu *replacement, const kdgu *indicator);
kdgu *ktre_replace(const kdgu *subject, const kdgu *pat, const kdgu *replacement, const kdgu *indicator, int opt);
kdgu **ktre_split(ktre *re, const kdgu *subject, int *len);
int **ktre_getvec(const ktre *re);
void ktre_free(ktre *re);

#endif
