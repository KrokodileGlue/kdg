#ifndef ERROR_H
#define ERROR_H

#include <stdint.h>
#include "kdgu.h"

typedef struct kdgu_error {
	enum kdgu_errorcode {
		KDGU_ERR_NO_ERROR,

		/* TODO: Turn some of these into generic errors. */
		KDGU_ERR_UTF8_STRAY_CONTINUATION_BYTE,
		KDGU_ERR_UTF8_INVALID_BYTE,
		KDGU_ERR_UTF8_MISSING_CONTINUATION,
		KDGU_ERR_UTF8_NONCHARACTER,
		KDGU_ERR_UTF8_INVALID_RANGE,
		KDGU_ERR_UTF8_INVALID_CONTRANGE,
		KDGU_ERR_UTF8_RANGE_LENGTH_MISMATCH,
		KDGU_ERR_UTF8_INVALID_LENGTH,
		KDGU_ERR_UTF8_INCORRECT_LENGTH,

		KDGU_ERR_UTF16_EOS,
		KDGU_ERR_UTF16_MISSING_SURROGATE,

		KDGU_ERR_UTF32_EOS,

		KDGU_ERR_INVALID_CP1252,
		KDGU_ERR_INVALID_ASCII,
		KDGU_ERR_INVALID_EBCDIC,

		KDGU_ERR_NO_CONVERSION,
		KDGU_ERR_OUT_OF_MEMORY
	} kind;

	unsigned loc;
	char *msg;

	uint32_t codepoint;
	char *data;
} kdgu_error;

#define ERR(X,Y)	  \
	(struct kdgu_error){.kind = (X), .loc = (Y), .msg = error[X]}

extern char *error[];
bool pusherror(kdgu *k, struct kdgu_error err);

#endif
