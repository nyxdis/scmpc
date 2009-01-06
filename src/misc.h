/**
 * misc.h: Misc helper functions
 *
 * Copyright (c) 2008 Christoph Mende <angelos@unkreativ.org>
 * All rights reserved. Released under the 2-clause BSD license.
 *
 * Based on Jonathan Coome's work on scmpc
 */


enum loglevel
{
	NONE,
	ERROR,
	INFO,
	DEBUG
};

enum connection_status {
	CONNECTED,
	DISCONNECTED,
	BADAUTH
};

void open_log(const char *filename);
void scmpc_log(enum loglevel, const char *format, ...);

/* used by curl */
size_t buffer_write(void *input, size_t size, size_t nmemb, void *buf);

char *md5_hash(char *text);

#ifndef HAVE_ASPRINTF
int asprintf(char **ret, const char *format, ...);
#endif
