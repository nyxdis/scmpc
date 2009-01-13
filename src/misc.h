/**
 * misc.h: Misc helper functions
 *
 * ==================================================================
 * Copyright (c) 2009 Christoph Mende <angelos@unkreativ.org>
 * Based on Jonathan Coome's work on scmpc
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
char *curlinput;

char *md5_hash(char *text);

#ifndef HAVE_ASPRINTF
int asprintf(char **ret, const char *format, ...);
#endif
