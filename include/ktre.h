#ifndef KTRE_H
#define KTRE_H

/*
 * Manually disable warnings and errors about "unsafe" functions on
 * Windows.
 */
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
	KTRE_ERROR_TOO_MANY_GROUPS,
	KTRE_ERROR_INVALID_OPTIONS
};

/* options */
enum {
	KTRE_INSENSITIVE = 1 << 0,
	KTRE_UNANCHORED  = 1 << 1,
	KTRE_EXTENDED    = 1 << 2,
	KTRE_GLOBAL      = 1 << 3,
	KTRE_MULTILINE   = 1 << 4,
	KTRE_CONTINUE    = 1 << 5
};

/* settings and limits */
#define KTRE_MAX_ERROR_LEN 100
#define KTRE_MAX_GROUPS 100
#define KTRE_MAX_THREAD 200
#define KTRE_MAX_CALL_DEPTH 100
#define KTRE_MEM_CAP 100000000

struct ktre_info {
	int ba;  /* bytes allocated */
	int mba; /* max bytes allocated */
	int bf;  /* bytes freed */
	int num_alloc, num_free;

	int thread_alloc;
	int instr_alloc;

	int parser_alloc;
	int runtime_alloc;
};

struct ktre_minfo {
	const char *file;
	int line;
	int size;
	struct ktre_minfo *prev, *next;
};

struct ktre {
	/* ===== public fields ===== */
	unsigned num_matches;
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
	_Bool literal;   /* whether to escape metacharacters or not */
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
		char *name;
	} *group;

	/* runtime */
	struct thread {
		int ip, sp, fp, la, ep, opt;
		int *frame, *vec, *prog, *las, *exception;
		_Bool die, rev;
	} *t;

	int tp, max_tp;
	int **vec;

	struct ktre_info info;
	struct ktre_minfo *minfo;

	_Bool copied;
};

typedef struct ktre ktre;

/* API prototypes */
ktre *ktre_compile(const char *pat, int opt);
ktre *ktre_copy(ktre *re);
_Bool ktre_exec(ktre *re, const char *subject, int ***vec);
_Bool ktre_match(const char *subject, const char *pat, int opt, int ***vec);
char *ktre_filter(ktre *re, const char *subject, const char *replacement, const char *indicator);
char *ktre_replace(const char *subject, const char *pat, const char *replacement, const char *indicator, int opt);
char **ktre_split(ktre *re, const char *subject, int *len);
int **ktre_getvec(const ktre *re);
struct ktre_info ktre_free(ktre *re);

#endif
