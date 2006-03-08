/**
 * audioscrobbler.h: The Audioscrobbler interaction code.
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


/**
 * queue_node
 *
 * Everything audioscrobbler needs to know about the song that we're
 * submitting.
 */
typedef struct queue_node_s {
	struct queue_node_s *next;
	char *artist;
	char *title;
	char *album;
	long int length;
	/* The time format will always need 19 characters. */
	char date[20];
} queue_node;

/**
 * as_connection
 *
 * Stores connection information for mpd.
 */
typedef struct {
	CURL *handle;
	struct curl_slist *headers;
	char *submit_url;
	char password[33];
	unsigned long interval;
	long error_count;
	enum connection_status status;
	time_t last_handshake;
} as_connection;

/**
 * scrobbler_thread()
 *
 * ...
 */
void *as_thread(void *arg);

void *cache_thread(void *arg);

/**
 * add_song_to_queue()
 *
 * Add a song to the submission queue. The define is due to the lack of default
 * arguments in C - the mpd thread doesn't need to add the date, but when the
 * cache is being loaded from a file, a new date shouldn't be assigned to the
 * song.
 */
#define add_song_to_queue(a, t, b, l) __add_song_to_queue(a, t, b, l, NULL)
void __add_song_to_queue(const char *artist, const char *title, 
		const char *album, long int length, const char *date);
