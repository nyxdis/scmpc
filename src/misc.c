/**
 * misc.c: Misc helper functions
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


#include <stdarg.h>

#include "misc.h"
#include "audioscrobbler.h"
#include "preferences.h"

static FILE *log_file;

void open_log(gconstpointer filename)
{
	if(!prefs.fork) {
		log_file = stdout;
		return;
	}

	log_file = fopen(filename,"a");
	if(log_file == NULL) {
		fputs("Unable to open log file, logging to stdout",stderr);
		log_file = stdout;
	}
}

void scmpc_log(loglevel level, gconstpointer format, ...)
{
	gchar *ts;
	time_t t;
	va_list ap;

	if(level > prefs.log_level)
		return;

	t = time(NULL);
	ts = g_malloc(22);
	strftime(ts,22,"%Y-%m-%d %H:%M:%S  ",localtime(&t));
	fputs(ts,log_file);
	g_free(ts);

	va_start(ap,format);
	vfprintf(log_file,format,ap);
	va_end(ap);

	fputs("\n",log_file);
	fflush(log_file);
}

gsize buffer_write(void *input, gsize size, gsize nmemb, void *buf)
{
	gsize len = size*nmemb;
	buf = NULL; /* suppress warnings about buf being unused */
	buffer = g_strdup(input);
	return len;
}
