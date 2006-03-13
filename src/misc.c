/**
 * misc.c: Miscellaneous functions.
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

#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "liberror.h"
#include "misc.h"
#include "scmpc.h"
#include "preferences.h"

FILE *log_file;

void __die(const char msg[], const char file[], int line)
{
	scmpc_log(ERROR, "%s:%d %s", file, line, msg);
	end_program();
}

FILE *file_open(const char *filename, const char *mode, error_t **error)
{
	FILE *file;
	struct stat file_status;

	*error = NULL;
	
	if (stat(filename, &file_status) != 0) {
		if (! (errno == ENOENT && strchr(mode, 'r') == NULL)) {
			/* Something went wrong - the file doesn't exist an we're trying to
			 * read from it, or anything else. */
			*error = error_set(errno, strerror(errno), NULL);
			return NULL;
		}
		/* Otherwise the file is going to be created by fopen() */
	} else if (! S_ISREG(file_status.st_mode) && strchr(mode, 'r') == NULL) {
		char *errorstr = alloc_sprintf(strlen(filename)+1, "%s is not a regular"
				" text file - scmpc will not write to it.", filename);
		*error = error_set(0, errorstr, NULL);
		free(errorstr);
		return NULL;
	}

	file = fopen(filename, mode);
	if (file == NULL) {
		*error = error_set(errno, strerror(errno), NULL);
		return NULL;
	}
	
	return file;
}

void open_log(const char *filename)
{
	error_t *error = NULL;
	
	if (! prefs.fork) {
		log_file = stdout;
	} else {
		log_file = file_open(filename, "a", &error);
		if (ERROR_IS_SET(error)) {
			fprintf(stderr, "The log file (%s) cannot be opened: %s\n",
					filename, error->msg);
			error_clear(error);
		}
	}
}

void close_log(void)
{
	if (log_file != NULL && log_file != stdout)
		fclose(log_file);
}

void scmpc_log(enum loglevel level, const char *format, ...)
{
#define TIME_BUF_LEN 22
	char time_buf[TIME_BUF_LEN];
	time_t t;
	struct tm *t_broken_down;
	va_list ap;

	/* The user doesn't want to see this message. */
	if (level > prefs.log_level)
		return;
	
	/* Log file doesn't exist, so do nothing. */
	if (log_file == NULL)
		return;
	
	t = time(NULL);
	t_broken_down = localtime(&t);
	strftime(time_buf, TIME_BUF_LEN, "%Y-%m-%d %H:%M:%S  ", t_broken_down);

	fputs(time_buf, log_file);
	
	va_start(ap, format);
	vfprintf(log_file, format, ap);
	va_end(ap);

	fputs("\n", log_file);
	fflush(log_file);
}

/**
 * buffer_alloc()
 *
 * Allocates a write buffer to be used by buffer_write().
 */
buffer_t *buffer_alloc(void)
{
	buffer_t *buffer;
	
	buffer = malloc(sizeof(buffer_t));
	if (buffer == NULL)
		return NULL;
	buffer->buffer = calloc(1024, 1);
	if (buffer->buffer == NULL) {
		free(buffer);
		return NULL;
	}
	buffer->len = 0;
	buffer->avail = 1023;

	return buffer;
}

/**
 * buffer_free()
 *
 * Frees the buffer allocated by buffer_alloc()
 */
void buffer_free(buffer_t *buffer)
{
	free(buffer->buffer);
	free(buffer);
}

/**
 * buffer_write()
 *
 * Callback used by curl whenever it receives data from the server.
 */
size_t buffer_write(void *input, size_t size, size_t nmemb, void *buf)
{
	size_t input_length = size * nmemb;
	char *old_contents, *new_buffer;
	buffer_t *buffer = (buffer_t *)buf;

	if (input_length > buffer->avail) {
		size_t alloc_size;

		old_contents = strdup(buffer->buffer);
		if (old_contents == NULL)
			return 0;
	
		/* Memory is added in 1024 byte blocks. 
		 * Work out how many more we need. */
		alloc_size = ((input_length/1024)+1)*1024 + buffer->len+buffer->avail+1;
		new_buffer = realloc(buffer->buffer, alloc_size);
		if (new_buffer == NULL) {
			free(old_contents);
			return 0;
		} else {
			buffer->buffer = new_buffer;
		}
		buffer->avail = alloc_size-1;

		/* So that strncat doesn't break horribly on dirty memory. */
		memset(&(buffer->buffer), '\0', alloc_size);
		strcpy(buffer->buffer, old_contents);
		
		free(old_contents);
	}

	strncat(buffer->buffer, input, input_length);
	buffer->len += input_length;
	buffer->avail -= input_length;

	return input_length;
}

/* Based on the glibc printf manpage, so the copyright for this probably
 * belongs to the GNU Foundation.
 *
 * XXX: I'm not sure about the portability of using the return value of
 * vnsprintf... */
char *alloc_sprintf(int size, const char *format, ...)
{
	int n;
	char *p, *np;
	va_list ap;

	if (size == 0)
		size = 100;
	
	if ((p = malloc(size)) == NULL)
		return NULL;

	while (1) {
		/* Try to print in the allocated space. */
		va_start(ap, format);
		n = vsnprintf(p, size, format, ap);
		va_end(ap);
		
		/* If that worked, return the string. */
		if (n > -1 && n < size)
			return p;
		
		/* Else try again with more space. */
		if (n > -1)    /* glibc 2.1 */
			size = n+1; /* precisely what is needed */
		else           /* glibc 2.0 */
			size *= 2;  /* twice the old size */
		
		if ((np = realloc (p, size)) == NULL) {
			free(p);
			return NULL;
		} else {
			p = np;
		}
	}
}

/**
 * read_line_from_file()
 *
 * Reads a line from the file specified. It allocates the memory required, and
 * keeps reading until a newline is read.
 */
char *read_line_from_file(FILE *file)
{
	char *line, *tmp, *newline, *ret;
	size_t line_length = 256;
	
	if ((line = malloc(line_length)) == NULL)
		return NULL;

	ret = fgets(line, line_length, file);
	if (ret == NULL) {
		free(line);
		return NULL;
	}

	while ((newline = strchr(line, '\n')) == NULL) {
		char *old_line = line;

		if ((tmp = malloc(line_length)) == NULL) {
			free(line);
			return NULL;
		}
		
		ret = fgets(tmp, line_length, file);
		if (ret == NULL && ! feof(file)) {
			free(tmp);
			free(line);
			return NULL;
		}
		
		line_length *= 2;
		if ((line = realloc(old_line, line_length)) == NULL) {
			free(old_line);
			free(tmp);
			return NULL;
		}

		strncat(line, tmp, line_length/2);
		free(tmp);
	}

	if (newline != NULL)
		*newline = '\0';

	return line;
}
