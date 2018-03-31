#include <inttypes.h>
#include <string.h>
#include <stdbool.h>
#include "error.h"

bool
pusherror(kdgu *k, struct kdgu_error err)
{
	if (!k->errlist) {
		k->errlist = malloc(sizeof *k->errlist);
		if (!k->errlist) return false;
		memset(k->errlist, 0, sizeof *k->errlist);
	}

	void *p = realloc(k->errlist->err,
	                  (k->errlist->num + 1)
	                  * sizeof *k->errlist->err);

	if (!p) {
		free(k->errlist->err);
		return false;
	}

	k->errlist->err = p;
	k->errlist->err[k->errlist->num++] = err;

	return true;
}

/*
 * VOLATILE: This table must be kept up-to-date with the order of enum
 * kdgu_errorcode.
 */
char *error[] = {
	"no error",

	"a continuation byte cannot be the start of a well-formed"
	" sequence",

	"impossible byte value cannot appear anywhere in well-formed"
	" UTF-8",

	"sequence initializer is followed by an unexpected number of"
	" continuation bytes",

	"sequence represents a noncharacter; noncharacters are"
	" permanently reserved for internal usage",

	"initializer byte is not in any of the possible ranges",

	"sequence contains continuation bytes that are outside of the"
	" valid range",

	"initializer byte indicates an impossible sequence length",

	"initializer byte indicates a length that is not valid for"
	" its range",

	"sequence actual length does not match the length declared"
	" by the initializer byte",

	"buffer contains a trailing byte",

	"sequences contains an invalid low surrogate byte",

	"invalid byte in CP1252",

	"ASCII character is out-of-range",

	"no representation for character U+%02"PRIX32
	" in encoding '%s'",

	"kdgu has run out of memory; you should never see this"
};
