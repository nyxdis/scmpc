/**
 * misc.c: Misc helper functions
 *
 * ==================================================================
 * Copyright (c) 2008 Christoph Mende <angelos@unkreativ.org>
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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <gcrypt.h>

#include "misc.h"
#include "audioscrobbler.h"
#include "preferences.h"

static FILE *log_file;

void open_log(const char *filename)
{
	if(prefs.fork == 0) {
		log_file = stdout;
		return;
	}

	log_file = fopen(filename,"a");
	if(log_file == NULL) {
		fprintf(stderr,"Unable to open log file, logging to stdout\n");
		log_file = stdout;
	}
}

void scmpc_log(enum loglevel level, const char *format, ...)
{
	char *ts;
	time_t t;
	va_list ap;

	if(level > prefs.log_level)
		return;
	
	t = time(NULL);
	ts = malloc(22);
	strftime(ts,22,"%F %T  ",localtime(&t));
	fputs(ts,log_file);
	free(ts);

	va_start(ap,format);
	vfprintf(log_file,format,ap);
	va_end(ap);

	fputs("\n",log_file);
	fflush(log_file);
}

size_t buffer_write(void *input, size_t size, size_t nmemb, void *buf)
{
	free(buf);
	size_t len = size*nmemb;
	if((buffer = malloc(len)) == NULL) return -1;
	strncpy(buffer,input,len);
	return len;
}

char *md5_hash(char *text)
{
	char *result;
	gcry_md_hd_t hd;
	int i;
	unsigned char *tmp;

	gcry_md_open(&hd,GCRY_MD_MD5,0);
	gcry_md_write(hd,text,strlen(text));
	tmp = gcry_md_read(hd,GCRY_MD_MD5);
	result = malloc(32);
	if(result == NULL) return NULL;
	for(i=0;i<16;++i)
		snprintf(result+i*2,3,"%02x",tmp[i]);
	gcry_md_close(hd);

	return result;
}

#ifndef HAVE_ASPRINTF
int asprintf(char **ret, const char *format, ...)
{
	int n, size = 100;
	char *p, *np;
	va_list ap;

	if ((p = malloc((size_t)size)) == NULL) {
		*ret = NULL;
		return -1;
	}

	while (1) {
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
		else	    /* vsnprintf didn't return anything useful. */
			size *= 2;

		if ((np = realloc(p, (size_t)size)) == NULL) {
			free(p);
			*ret = NULL;
			return -1;
		} else {
			p = np;
		}
	}

	*ret = p;
	return (int)strlen(*ret);
}
#endif
