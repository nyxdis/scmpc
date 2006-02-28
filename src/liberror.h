/**
 * liberror.h: A simple library to handle errors without using return values.
 *
 * ==================================================================
 * Copyright (c) 2005-2006 Jonathan Coome.
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

typedef struct error_type {
	int code;
	char *msg;
	struct error_type *child;
} error_t;

#define ERROR_OUT_OF_MEMORY ((error_t *)-1)

/**
 * ERROR_IS_SET()
 *
 * A macro to check whether an error has occurred. Made to be used in
 * conditions statements, particularly if/else.
 *
 * Parameters: 
 *   error, error_t pointer.
 * Returns: A boolean true or false.
 */
#define ERROR_IS_SET(error) \
	((error != NULL) && (error != ERROR_OUT_OF_MEMORY) && (error->code != 0))

/**
 * ERROR_CHECK_NOMEM()
 *
 * A macro to check whether the error was that the application has run out of
 * memory - if so then it will not be able to malloc a new error_t struct, and
 * so returns (error_t *)-1.
 *
 * Parameters:
 *   error, error_t pointer.
 * Returns: A boolean true or false.
#define ERROR_CHECK_NOMEM(error) (error == ERROR_OUT_OF_MEMORY)
*/

/**
 * error_set()
 *
 * A function to create a new error. It mallocs, populates, and returns a new
 * error_t struct with the error code, message, and optional child error set.
 * This should be freed after use with the error_clear function, unless it will
 * be cleared as a child of a different error. If the program runs out of
 * memory, this function will return ERROR_OUT_OF_MEMORY instead.
 *
 * Parameters:
 *   code, integer: The error code (for the use of the program).
 *   msg, string: The human readable error message.
 *   child, error_t pointer: A child error. Can be NULL if not required.
 * Returns: An pointer to an error_t struct, or ERROR_OUT_OF_MEMORY.
 */
error_t *error_set(int code, const char *msg, error_t *child);

/**
 * error_clear()
 *
 * Clears the error pointed to by error, and any child errors.
 *
 * Parameters:
 *   error, error_t pointer.
 * Returns: Nothing.
 */
void error_clear(error_t *error);
