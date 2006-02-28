/**
 * misc.h: Miscellaneous functions.
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

enum loglevel {
	NONE,
	ERROR,
	INFO,
	DEBUG
};

enum connection_status {
	CONNECTED,
	DISCONNECTED,
	BADUSER
};

typedef enum {
	FALSE,
	TRUE
} bool;

typedef struct write_buffer_t {
	/* The actual data returned by the server. */
	char *buffer;
	/* How many characters are in the buffer at the moment. */
	size_t len;
	/* How many characters there are available to be filled in the buffer. This
	 * doesn't include the NUL byte. */
	size_t avail;
} buffer_t;

/**
 * file_open()
 *
 * A version of fopen() that also checks that the file you're writing to or
 * reading from isn't a symbolic link, and also provides some more helpful
 * errors if something goes wrong.
 */
FILE *file_open(const char *filename, const char *mode, error_t **error);

/**
 * open_log()
 *
 * Opens the log file, or if prefs.fork is FALSE, sets the log_file to stdout
 * instead. Prints a message to stderr if the log file cannot be opened.
 */
void open_log(const char *filename);

/**
 * close_log()
 *
 * Closes the log file. Basically it's only here to avoid having to tell main.c
 * about log_file.
 */
void close_log(void);

/**
 * scmpc_log()
 *
 * The logging function. This is probably the only way to get information to
 * people when the program has started, especially if it's been daemonise()d.
 * It takes a variable number of arguments and uses printf format strings to
 * write to the log file. It also prepends the date and time before writing the
 * message.
 *
 * There are four different log levels to choose from - see the loglevel enum
 * above. The user can select the level of logging he or she wants, and the
 * function will return without doing anything if the log message has a higher
 * level than that the user chose. (The most important messages have log_level
 * 0, and the least important 4, at the moment.
 */
void scmpc_log(enum loglevel, const char *format, ...);

/**
 * die()
 *
 * Prints a message containing the file and line number to show where it was
 * called, and exits the program. Currently this is an interface to scmpc_log, 
 * but it could be defined differently if it was used somewhere else.
 */
#define die(x) __die(x, __FILE__, __LINE__)
void __die(const char msg[], const char file[], int line);

/**
 * buffer_alloc()
 *
 * Allocates a write buffer to be used by buffer_write().
 */
buffer_t *buffer_alloc(void);

/**
 * buffer_free()
 *
 * Frees the buffer allocated by buffer_alloc()
 */
void buffer_free(buffer_t *buffer);

/**
 * buffer_write()
 *
 * Callback used by curl whenever it receives data from the server.
 */
size_t buffer_write(void *input, size_t size, size_t nmemb, void *buf);

/**
 * alloc_sprintf()
 *
 * Allocates the memory for a string, and then writes to it sprintf-style.
 */
char *alloc_sprintf(int estimated_size, const char *format, ...);

/**
 * read_line_from_file()
 *
 * Reads a line from the file specified. It allocates the memory required, and
 * keeps reading until a newline is read.
 */
char *read_line_from_file(FILE *file);
