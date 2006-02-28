/**
 * liberror.c: A simple library to handle errors without using return values.
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

#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include "liberror.h"

error_t *error_set(int code, const char *msg, error_t *child)
{
	error_t *error;

	error = malloc(sizeof(error_t));
	if (error == NULL) {
		error_clear(child);
		return ERROR_OUT_OF_MEMORY;
	}

	error->code = code;
	error->child = child;
	
	assert(msg != NULL);
	if (msg == NULL)
		error->msg = strdup("");
	else
		error->msg = strdup(msg);

	if (error->msg == NULL) {
		error_clear(error);
		return ERROR_OUT_OF_MEMORY;
	}

	return error;
}

void error_clear(error_t *error)
{
	error_t *current = error, *tmp;

	while (current != NULL && current != ERROR_OUT_OF_MEMORY) {
		free(current->msg);
		tmp = current->child;
		free(current);
		current = tmp;
	}
}
