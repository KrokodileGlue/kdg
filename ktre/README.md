# KTRE: a regular expression engine

KTRE is a small, single-header regular expression library that
implements a subset of Perl/PCRE regular expressions.

To use this library, just make a file in your project (usually called
ktre.c), `#define KTRE_IMPLEMENTATION` in that file, and `#include
"ktre.h"`.

## Features

* submatching
* backreferencing
* named capture groups
* subroutine calls
* recursion
* positive and negative lookahead assertions
* arbitrary/infinite positive and negative lookbehind assertions
* character classes/negated character classes
* pattern replacement with `ktre_filter()` and `ktre_replace()`
* global and case-insensitive matching
* \Q and \E
* useful debugging output with KTRE_DEBUG

## API

```c
struct ktre *ktre_compile(const char *pat, int opt);
struct ktre *ktre_copy(struct ktre *re);
_Bool ktre_exec(struct ktre *re, const char *subject, int ***vec);
_Bool ktre_match(const char *subject, const char *pat, int opt, int ***vec);
char *ktre_filter(struct ktre *re, const char *subject, const char *replacement);
char *ktre_replace(const char *subject, const char *pat, const char *replacement, int opt, int ***vec);
int **ktre_getvec(const struct ktre *re);
struct ktre_info ktre_free(struct ktre *re);
```

## Options

KTRE_INSENSITIVE: (?i) - (?c)

> Causes the VM to ignore case when matching English characters.

KTRE_UNANCHORED:

> Allows the VM to reach a winning state regardless of whether or not
> it has reached the end of the subject string; in other words, any
> substring in the subject may be matched.

KTRE_EXTENDED: (?x)

> This option turns on so-called 'free-spaced' mode, which allows
> whitespace to occur in most parts of the grammar without
> side-effects.  Note that this does not allow whitespace anywhere;
> `(?#foobar)` is not the same thing as `( ?#foobar)`, but `foobar` is
> the same as `foo bar`.  If you want to match a whitespace character
> in free-spaced mode, you must escape it or put it into a character
> class.

KTRE_GLOBAL:

> If this option is set then the VM will continue trying to match the
> pattern against the subject string until it has found all possible
> matches. There's one quirk: whenever it finds a match it destroys
> all backtracking information and tries to match at the end of the
> last successful match, which will cause valid matches to be skipped
> over.  This isn't a mistake; it's how most engines handle global
> matching.
