/**
 * audioscrobbler.h: Audioscrobbler backend.
 *
 * ==================================================================
 * Copyright (c) 2009-2011 Christoph Mende <angelos@gentoo.org>
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


/* curl/curl.h requires sys/select.h but doesn't include it on FreeBSD */
#include <sys/select.h>
#include <curl/curl.h>
#include <glib.h>

struct {
	gchar *session_id;
	gchar *submit_url;
	gchar *np_url;
	gchar password[33];
	time_t last_auth;
	time_t last_fail;
	connection_status status;
	CURL *handle;
	struct curl_slist *headers;
} as_conn;

gchar *buffer;

gint as_connection_init(void);
gint as_submit(void);
void as_cleanup(void);
void as_authenticate(void);
void as_now_playing(void);
