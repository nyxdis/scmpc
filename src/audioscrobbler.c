/**
 * audioscrobbler.c: Audioscrobbler backend.
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


#include <stdlib.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "misc.h"
#include "mpd.h"
#include "preferences.h"
#include "audioscrobbler.h"
#include "queue.h"

static gchar curl_error_buffer[CURL_ERROR_SIZE];

#define HANDSHAKE_URL \
	"http://post.audioscrobbler.com/?hs=true&p=1.2.1&c=spc&v=%s&u=%s&t=%ld&a=%s"

gint as_connection_init(void)
{
	as_conn.handle = curl_easy_init();
	if (!as_conn.handle)
		return -1;
	as_conn.submit_url = as_conn.np_url = as_conn.session_id = NULL;
	as_conn.last_handshake = 0;
	as_conn.status = DISCONNECTED;
	as_conn.headers = curl_slist_append(as_conn.headers,
			"User-Agent: scmpc/" PACKAGE_VERSION);

	curl_easy_setopt(as_conn.handle, CURLOPT_HTTPHEADER, as_conn.headers);
	curl_easy_setopt(as_conn.handle, CURLOPT_WRITEFUNCTION, &buffer_write);
	curl_easy_setopt(as_conn.handle, CURLOPT_ERRORBUFFER, curl_error_buffer);
	curl_easy_setopt(as_conn.handle, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(as_conn.handle, CURLOPT_CONNECTTIMEOUT, 5L);
	curl_easy_setopt(as_conn.handle, CURLOPT_TIMEOUT, 5L);
	return 0;
}

void as_cleanup(void)
{
	curl_slist_free_all(as_conn.headers);
	curl_easy_cleanup(as_conn.handle);
	as_conn.headers = as_conn.handle = NULL;
	g_free(as_conn.session_id);
	g_free(as_conn.np_url);
	g_free(as_conn.submit_url);
}

void as_handshake(void)
{
	gchar *auth_token, *handshake_url, *line, *saveptr = NULL, *tmp;
	GTimeVal tv;
	glong timestamp;
	gint ret;

	if (as_conn.status == BADAUTH) {
		scmpc_log(INFO, "Refusing handshake, please check your "
			"Audioscrobbler credentials and restart %s",
			PACKAGE_NAME);
		return;
	}

	if (!strlen(prefs.as_username) || (!strlen(prefs.as_password) &&
		!strlen(prefs.as_password_hash))) {
		scmpc_log(INFO, "No username or password specified. "
				"Not connecting to Audioscrobbler.");
		as_conn.status = BADAUTH;
		return;
	}

	g_get_current_time(&tv);
	timestamp = tv.tv_sec;

	if (difftime(time(NULL), as_conn.last_handshake) < 1800) {
		scmpc_log(DEBUG, "Requested handshake, but last handshake "
				"was less than 30 minutes ago.");
		return;
	}

	if (strlen(prefs.as_password_hash) > 0) {
		tmp = g_strdup_printf("%s%ld", prefs.as_password_hash, timestamp);
	} else {
		auth_token = g_compute_checksum_for_string(G_CHECKSUM_MD5,
				prefs.as_password, -1);
		tmp = g_strdup_printf("%s%ld", auth_token, timestamp);
		g_free(auth_token);
	}
	auth_token = g_compute_checksum_for_string(G_CHECKSUM_MD5, tmp, -1);
	g_free(tmp);
	handshake_url = g_strdup_printf(HANDSHAKE_URL, PACKAGE_VERSION,
		prefs.as_username, timestamp, auth_token);
	g_free(auth_token);

	scmpc_log(DEBUG, "handshake_url = %s", handshake_url);

	curl_easy_setopt(as_conn.handle, CURLOPT_WRITEDATA, (void *)buffer);
	curl_easy_setopt(as_conn.handle, CURLOPT_HTTPGET, 1);
	curl_easy_setopt(as_conn.handle, CURLOPT_URL, handshake_url);

	ret = curl_easy_perform(as_conn.handle);
	g_free(handshake_url);

	if (ret) {
		scmpc_log(ERROR, "Could not connect to the Audioscrobbler: %s",
			curl_easy_strerror(ret));
		g_free(buffer);
		buffer = NULL;
		return;
	}

	as_conn.last_handshake = time(NULL);
	line = strtok_r(buffer, "\n", &saveptr);
	if (!line) {
		scmpc_log(DEBUG, "Could not parse Audioscrobbler handshake"
				" response.");
		g_free(buffer);
		buffer = NULL;
		return;
	}

	if (!strncmp(line, "OK", 2)) {
		gushort line_no = 1;

		while ((line = strtok_r(NULL, "\n", &saveptr))) {
			line_no++;
			if (line_no == 2) {
				free(as_conn.session_id);
				as_conn.session_id = g_strdup(line);
			} else if (line_no == 3) {
				free(as_conn.np_url);
				as_conn.np_url = g_strdup(line);
			} else if (line_no == 4) {
				free(as_conn.submit_url);
				as_conn.submit_url = g_strdup(line);
				break;
			}
		}
		if (line_no < 4) {
			scmpc_log(DEBUG, "Truncated data from server returned."
					"  Found %d lines, expected 4 lines"
					" or more.", line_no);
		} else {
			scmpc_log(INFO, "Connected to Audioscrobbler.");
			as_conn.status = CONNECTED;
		}
	} else if (!strncmp(line, "FAILED", 6)) {
		scmpc_log(ERROR, "The Audioscrobbler handshake could not be "
			"completed: %s", &line[7]);
	} else if (!strncmp(line, "BADAUTH", 7)) {
		scmpc_log(ERROR, "The user details you specified were not "
				"accepted by Audioscrobbler. Please correct "
				"them and restart this program.");
		as_conn.status = BADAUTH;
	} else if (!strncmp(line, "BADTIME", 7)) {
		scmpc_log(ERROR, "Handshake failed because your system time is "
				"too far off. Please correct your clock.");
	} else {
		scmpc_log(DEBUG, "Could not parse Audioscrobbler handshake"
				" response.");
	}
	g_free(buffer);
	buffer = NULL;
}

void as_now_playing(void)
{
	gchar *querystring, *artist, *album, *title, *line;
	gint ret;

	if (as_conn.status != CONNECTED) {
		scmpc_log(INFO, "Not sending Now Playing notification:"
				" not connected");
		return;
	}

	artist = curl_easy_escape(as_conn.handle, current_song.artist, 0);
	title = curl_easy_escape(as_conn.handle, current_song.title, 0);
	if (current_song.album)
		album = curl_easy_escape(as_conn.handle, current_song.album, 0);
	else
		album = g_strdup("");

	querystring = g_strdup_printf("s=%s&a=%s&t=%s&b=%s&l=%d&n=%d&m=",
		as_conn.session_id, artist, title, album, current_song.length,
		current_song.track);

	curl_free(artist);
	curl_free(title);
	if (current_song.album)
		curl_free(album);
	else
		g_free(album);

	scmpc_log(DEBUG, "querystring = %s", querystring);

	curl_easy_setopt(as_conn.handle, CURLOPT_WRITEDATA, (void*)buffer);
	curl_easy_setopt(as_conn.handle, CURLOPT_POSTFIELDS, querystring);
	curl_easy_setopt(as_conn.handle, CURLOPT_URL, as_conn.np_url);

	ret = curl_easy_perform(as_conn.handle);
	g_free(querystring);
	if (ret) {
		scmpc_log(ERROR, "Failed to connect to Audioscrobbler: %s",
			curl_easy_strerror(ret));
		g_free(buffer);
		buffer = NULL;
		return;
	}

	line = strtok(buffer, "\n");
	if (!line)  {
		scmpc_log(INFO, "Could not parse Audioscrobbler submit"
				" response.");
	} else if (!strncmp(line, "BADSESSION", 10)) {
		scmpc_log(INFO, "Received bad session response from "
			"Audioscrobbler, re-handshaking.");
		as_handshake();
	} else if (!strncmp(line, "OK", 2)) {
		scmpc_log(INFO, "Sent Now Playing notification.");
	} else {
		scmpc_log(DEBUG, "Unknown response from Audioscrobbler while "
			"sending Now Playing notification.");
	}
	g_free(buffer);
	buffer = NULL;
}

static gint build_querystring(gchar **qs, queue_node **last_song)
{
	gchar *artist, *title, *album;
	GString *nqs;
	gint num = 0;
	queue_node *song = queue.first;

	nqs = g_string_new("s=");
	g_string_append(nqs, as_conn.session_id);

	while (song && num < 10) {
		if (!song->finished_playing) {
			song = song->next;
			continue;
		}

		artist = curl_easy_escape(as_conn.handle, song->artist, 0);
		title = curl_easy_escape(as_conn.handle, song->title, 0);
		album = curl_easy_escape(as_conn.handle, song->album, 0);

		g_string_append_printf(nqs, "&a[%d]=%s&t[%d]=%s&i[%d]=%ld&o[%d]=P"
			"&r[%d]=&l[%d]=%d&b[%d]=%s&n[%d]=&m[%d]=", num, artist,
			num, title, num, song->date, num, num, num,
			song->length, num, album, num, num);
		curl_free(artist); curl_free(title); curl_free(album);

		num++;
		song = song->next;
	}

	*qs = g_string_free(nqs, FALSE);
	*last_song = song;
	return num;
}

gint as_submit(void)
{
	gchar *querystring, *line, *saveptr = NULL;
	queue_node *last_added;
	gint ret, num_songs;
	static gchar last_failed[512];

	if (!queue.first)
		return -1;

	num_songs = build_querystring(&querystring, &last_added);
	if (num_songs <= 0) {
		g_free(querystring);
		return -1;
	}

	scmpc_log(DEBUG, "querystring = %s", querystring);

	curl_easy_setopt(as_conn.handle, CURLOPT_WRITEDATA, (void *)buffer);
	curl_easy_setopt(as_conn.handle, CURLOPT_POSTFIELDS, querystring);
	curl_easy_setopt(as_conn.handle, CURLOPT_URL, as_conn.submit_url);

	if ((ret = curl_easy_perform(as_conn.handle))) {
		scmpc_log(INFO, "Failed to connect to Audioscrobbler: %s",
			curl_easy_strerror(ret));
		return 1;
	}
	g_free(querystring);

	line = strtok_r(buffer, "\n", &saveptr);
	if (!line)
		scmpc_log(INFO, "Could not parse Audioscrobbler submit"
				" response.");
	else if (!strncmp(line, "FAILED", 6)) {
		if (strcmp(last_failed, &line[7])) {
			g_strlcpy(last_failed, &line[7], sizeof(last_failed));
			scmpc_log(INFO, "Audioscrobbler returned FAILED: %s",
				&line[7]);
		}
	} else if (!strncmp(line, "BADSESSION", 10)) {
		last_failed[0] = '\0';
		scmpc_log(INFO, "Received bad session from Audioscrobbler,"
				" re-handshaking.");
		as_handshake();
	} else if (!strncmp(line, "OK", 2)) {
		last_failed[0] = '\0';
		if (num_songs == 1)
			scmpc_log(INFO, "1 song submitted.");
		else
			scmpc_log(INFO, "%d songs submitted.", num_songs);
		queue_remove_songs(queue.first, last_added);
		queue.first = last_added;
	}
	return 0;
}
