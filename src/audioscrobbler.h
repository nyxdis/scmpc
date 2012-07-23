/**
 * audioscrobbler.h: Audioscrobbler backend.
 *
 * ==================================================================
 * Copyright (c) 2009-2012 Christoph Mende <mende.christoph@gmail.com>
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


#ifndef HAVE_AUDIOSCROBBLER_H
#define HAVE_AUDIOSCROBBLER_H

/* curl/curl.h requires sys/select.h but doesn't include it on FreeBSD */
#include <sys/select.h>
#include <curl/curl.h>
#include <glib.h>

#include "misc.h"

/**
 * Last.fm connection data
 */
struct {
	gchar *session_id;
	gint64 last_auth;
	gint64 last_fail;
	connection_status status;
	CURL *handle;
	struct curl_slist *headers;
} as_conn;

/**
 * cURL data buffer
 */
gchar *buffer;

/**
 * Initialize cURL
 */
gboolean as_connection_init(void);

/**
 * Build Last.fm authentication string and send it
 */
void as_authenticate(void);

/**
 * Check if the queue can be submitted and do it
 */
void as_check_submit(void);

/**
 * Release resources
 */
void as_cleanup(void);

/**
 * Build "Now playing" notification string and send it
 */
void as_now_playing(void);

#endif /* HAVE_AUDIOSCROBBLER_H */
