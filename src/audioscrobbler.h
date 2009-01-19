/**
 * audioscrobbler.h: Audioscrobbler backend.
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


#include <curl/curl.h>

struct as_connection {
	char *session_id;
	char *submit_url;
	char *np_url;
	char password[33];
	time_t last_handshake;
	enum connection_status status;
	CURL *handle;
	struct curl_slist *headers;
} *as_conn;

char *buffer;

int as_connection_init(void);
int as_submit(void);
void as_cleanup(void);
void as_handshake(void);
void as_now_playing(void);
