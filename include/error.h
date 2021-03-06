#ifndef ERROR_H
#define ERROR_H

#include <stdint.h>
#include <stdbool.h>

struct error {
	enum errorcode {
		ERR_NO_ERROR,

		/* Generic errors. */

		ERR_NONCHARACTER,

		ERR_UTF8_STRAY_CONTINUATION_BYTE,
		ERR_UTF8_INVALID_BYTE,
		ERR_UTF8_MISSING_CONTINUATION,
		ERR_UTF8_INVALID_RANGE,
		ERR_UTF8_INVALID_CONTRANGE,
		ERR_UTF8_RANGE_LENGTH_MISMATCH,
		ERR_UTF8_INVALID_LENGTH,
		ERR_UTF8_INCORRECT_LENGTH,

		ERR_UTF16_EOS,
		ERR_UTF16_MISSING_SURROGATE,

		ERR_UTF32_EOS,

		ERR_INVALID_CP1252,
		ERR_INVALID_ASCII,
		ERR_INVALID_EBCDIC,

		ERR_NO_CONVERSION,
		ERR_OUT_OF_MEMORY
	} kind;

	unsigned loc;
	uint32_t codepoint;
	char *data;
};

#define ERR(X,Y)	  \
	(struct error){.kind = (X), .loc = (Y), .data = NULL}

#define ENCERR(X,...)					\
	do {						\
		err = ERR(ERR_NO_CONVERSION, -1);	\
		X(KDGU_REPLACEMENT, __VA_ARGS__);	\
	} while (false)

extern char *error_str[];
bool pusherror(kdgu *k, struct error err);

#endif
