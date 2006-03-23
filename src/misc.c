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

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "exception.h"
#include "misc.h"
#include "preferences.h"
#include "scmpc.h"

char *error_strings[] = {
	"No error occurred.",
	"Out of memory.",
	"File not found.",
	"Connection timed out.",
	"Could not connect to host.",
	"Invalid password specified."
};

static FILE *log_file;

void open_log(const char *filename)
{
	struct s_exception e = EXCEPTION_INIT;
	
	if (! prefs.fork) {
		log_file = stdout;
		return;
	}
	
	log_file = file_open(filename, "a", &e);
	switch (e.code) {
		case 0:
			break;
		case OUT_OF_MEMORY:
			fputs("Out of memory.", stderr);
			exit(EXIT_FAILURE);
		case USER_DEFINED:
			fprintf(stderr, "The log file (%s) cannot be opened: %s\n",
					filename, e.msg);
			exception_clear(e);
		default:
			fprintf(stderr, "Unexpected error from file_open: %s\n", e.msg);
			break;
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
	if (t_broken_down == NULL)
		strlcpy(time_buf, "{localtime() failed}", TIME_BUF_LEN);
	else
		strftime(time_buf, TIME_BUF_LEN, "%Y-%m-%d %H:%M:%S  ", t_broken_down);

	fputs(time_buf, log_file);
	
	va_start(ap, format);
	vfprintf(log_file, format, ap);
	va_end(ap);

	fputs("\n", log_file);
	fflush(log_file);
}

#define BUFFER_SIZE 1024
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
	buffer->buffer = calloc(BUFFER_SIZE, sizeof(char));
	if (buffer->buffer == NULL) {
		free(buffer);
		return NULL;
	}
	buffer->len = 0;
	buffer->avail = BUFFER_SIZE;

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
 * Writes to a buffer that automatically increases in size when required.
 */
size_t buffer_write(void *input, size_t size, size_t nmemb, void *buf)
{
	size_t input_length = size * nmemb;
	char *new_buffer;
	buffer_t *buffer = (buffer_t *)buf;

	if (input_length > buffer->avail) {
		size_t alloc_size;

		/* Memory is added in 1024 byte blocks. 
		 * Work out how many more we need. */
		alloc_size = ((input_length/1024)+1)*1024 + buffer->len + buffer->avail;
		new_buffer = realloc(buffer->buffer, alloc_size);
		if (new_buffer == NULL) {
			return 0;
		} else {
			buffer->buffer = new_buffer;
		}
		buffer->avail = alloc_size - buffer->len;
	}

	strlcat(buffer->buffer, input, buffer->avail + buffer->len);
	buffer->len += input_length;
	buffer->avail -= input_length;

	return input_length;
}


FILE *file_open(const char *filename, const char *mode, struct s_exception *e)
{
	FILE *file;
	struct stat file_status;
	enum modes_e { READ, WRITE, APPEND } mode_e;

	switch (mode[0]) {
		case 'r': mode_e = READ; break;
		case 'w': mode_e = WRITE; break;
		case 'a': mode_e = APPEND; break;
		default:
			e->code = USER_DEFINED;
			if (asprintf(&(e->msg), "Invalid mode '%s' passed to file_open "
						"for file '%s'", mode, filename) == -1)
				e->code = OUT_OF_MEMORY;
			return NULL;
	}

	/* We don't want to write to symbolic links, because they could point to
	 * anything. However, we don't really care about /reading/ from them. */

	if (mode_e == WRITE || mode_e == APPEND) {
		if (stat(filename, &file_status) != 0) {
			/* It doesn't matter if the file doesn't exist - in this case it
			 * isn't a link of any kind, so it can be safely created by fopen
			 * below. */
			if (errno != ENOENT) {
				exception_create(e, strerror(errno));
				return NULL;
			}
		} else if (! S_ISREG(file_status.st_mode)) {
			exception_create(e, "This file is not a regular text file. "
					"scmpc will not write to it.");
			return NULL;
		}
	}

	file = fopen(filename, mode);
	if (file == NULL) {
		if (errno == ENOENT) {
			e->code = FILE_NOT_FOUND;
		} else {
			exception_create(e, strerror(errno));
		}
		return NULL;
	}

	return file;
}

#ifndef HAVE_ASPRINTF
int asprintf(char **ret, const char *format, ...)
{
	int n, size = 100;
	char *p, *np;
	va_list ap;
	
	if ((p = malloc((size_t)size)) == NULL)
		goto error;

	while (TRUE) {
		/* Try to print in the allocated space. */
		va_start(ap, format);
		n = vsnprintf(p, (size_t)size, format, ap);
		va_end(ap);
		
		/* If that worked, return the string. */
		if (n > -1 && n < size)
			break;
		
		/* Else try again with more space. */
		if (n > -1) /* vsnprintf returned the amount of space needed. */
			size = n+1;
		else        /* vsnprintf didn't return anything useful. */
			size *= 2;
		
		if ((np = realloc(p, (size_t)size)) == NULL) {
			free(p);
			goto error;
		} else {
			p = np;
		}
	}

	*ret = p;
	return (int)strlen(*ret);

error:
	*ret = NULL;
	return -1;
}
#endif

#ifndef HAVE_GETLINE
ssize_t getline(char **lineptr, size_t *n, FILE *stream)
{
	char *dest = *lineptr, *ret, *newline;
	size_t len = *n;
	
	if (dest == NULL || len < 1) {
		len = 256;
		if ((dest = malloc(len)) == NULL) {
			goto error;
		}
	}
	
	/* Fetch up to line_length bytes from the file, or up to a newline */
	ret = fgets(dest, (int) (len-1), stream);
	if (ret == NULL) {
		if (feof(stream) != 0) {
			dest[0] = '\0';
			len = 0;
			return 0;
		} else {
			goto error;
		}
	}
	
	/* If the line was too long, and so doesn't contain a newline, carry on
	 * fetching until it does, or we hit the end of the file. */
	while ((newline = strchr(dest, '\n')) == NULL) {
		char *new_dest, *tmp;

		/* Create a new storage space the same size as the last one, and carry
		 * on reading. We'll need to append this to the previous string - fgets
		 * will just overwrite it. */
		if ((tmp = malloc(len)) == NULL) {
			goto error;
		}

		ret = fgets(tmp, (int) (len-1), stream);
		if (ret == NULL) {
			/* This probably shouldn't happen... */
			if (feof(stream) != 0) {
				free(tmp);
				break;
			} else {
				free(tmp);
				goto error;
			}
		}

		len *= 2;
		if ((new_dest = realloc(dest, (size_t)len)) == NULL) {
			free(tmp);
			goto error;
		}

		dest = new_dest;
		strlcat(dest, tmp, len);
		free(tmp);
	}
	
	/* Don't include the newline in the line we return. */
	if (newline != NULL)
		*newline = '\0';
	
	return (ssize_t) (newline - dest - 1);

error:
	free(dest);
	dest = NULL;
	len = 0;
	return -1;
}
#endif

/* The following copyright notice applies to the strlcat function below. */
/*
 * Copyright (c) 1998 Todd C. Miller <Todd.Miller@courtesan.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Appends src to string dst of size siz (unlike strncat, siz is the
 * full size of dst, not space left).  At most siz-1 characters
 * will be copied.  Always NUL terminates (unless siz <= strlen(dst)).
 * Returns strlen(src) + MIN(siz, strlen(initial dst)).
 * If retval >= siz, truncation occurred.
 */
#ifndef HAVE_STRLCAT
size_t
strlcat(char *dst, const char *src, size_t siz)
{
	char *d = dst;
	const char *s = src;
	size_t n = siz;
	size_t dlen;

	/* Find the end of dst and adjust bytes left but don't go past end */
	while (n-- != 0 && *d != '\0')
		d++;
	dlen = d - dst;
	n = siz - dlen;

	if (n == 0)
		return(dlen + strlen(s));
	while (*s != '\0') {
		if (n != 1) {
			*d++ = *s;
			n--;
		}
		s++;
	}
	*d = '\0';

	return(dlen + (s - src));	/* count does not include NUL */
}
#endif

/* The following copyright notice applies to the strlcpy function below. */
/*
 * Copyright (c) 1998 Todd C. Miller <Todd.Miller@courtesan.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Copy src to string dst of size siz.  At most siz-1 characters
 * will be copied.  Always NUL terminates (unless siz == 0).
 * Returns strlen(src); if retval >= siz, truncation occurred.
 */
#ifndef HAVE_STRLCPY
size_t
strlcpy(char *dst, const char *src, size_t siz)
{
	char *d = dst;
	const char *s = src;
	size_t n = siz;

	/* Copy as many bytes as will fit */
	if (n != 0 && --n != 0) {
		do {
			if ((*d++ = *s++) == 0)
				break;
		} while (--n != 0);
	}

	/* Not enough room in dst, add NUL and traverse rest of src */
	if (n == 0) {
		if (siz != 0)
			*d = '\0';		/* NUL-terminate dst */
		while (*s++)
			;
	}

	return(s - src - 1);	/* count does not include NUL */
}
#endif
