/**
 * exception.h: A simple header defining exceptions and some error codes.
 *
 * ==================================================================
 * Copyright (c) 2006 Jonathan Coome.
 *
 * This file is part of scmpc.
 *
 * scmpc is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * scmpc is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with scmpc; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 * ==================================================================
 */

enum errors {
	USER_DEFINED  = -1,
	NO_ERROR      = 0,
	OUT_OF_MEMORY = 1,
	FILE_NOT_FOUND,
	CONNECTION_TIMEOUT,
	CONNECTION_FAILURE,
	INVALID_PASSWORD
};

struct s_exception {
	enum errors code;
	char *msg;
};

#define EXCEPTION_INIT {0, NULL}

/* Strings assigned in misc.c */
extern char *error_strings[];

#define exception_raise(e, ecode) \
	do { \
		e->code = ecode; \
		e->msg = error_strings[ecode]; \
	} while(0)

#define exception_create(e, str) \
	do { \
		e->code = USER_DEFINED; \
		e->msg = strdup(str); \
		if (e->msg == NULL) \
			e->code = OUT_OF_MEMORY; \
	} while(0)

#define exception_reraise(e, ce) \
	do { \
		e->code = ce.code; \
		e->msg = ce.msg; \
	} while(0)

#define exception_clear(e) \
	if (e.code == USER_DEFINED && e.msg != NULL) { \
		free(e.msg); \
	}
