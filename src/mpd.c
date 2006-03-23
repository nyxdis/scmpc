/**
 * mpd.c: Song retrieval and processing.
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

#define _XOPEN_SOURCE 500
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <signal.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

/* Prerequisite of audioscrobbler.h */
#include <curl/curl.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "exception.h"
#include "misc.h"
#include "scmpc.h"
#include "mpd.h"
#include "libmpd.h"
#include "audioscrobbler.h"
#include "preferences.h"

extern struct preferences prefs;

mpd_song_info song_info;

/*********************\
 * Private functions * 
\*********************/

/**
 * mpd_thread_cleanup()
 *
 * Called when the thread exits. It disconnects from MPD and frees the memory
 * allocated for the connection struct.
 */
static void mpd_thread_cleanup(void *conn)
{
	mpd_connection *mpd_conn = (mpd_connection *)conn;
	mpd_disconnect(mpd_conn);
	mpd_connection_cleanup(mpd_conn);
}

/**
 * mpd_reconnect()
 *
 * Attempts to connect to MPD, maybe disconnecting first if the connection
 * failed. It sends the password if there is one specified, and checks to make
 * sure we have read access to the server. Returns 1 on success, and 0 on
 * failure.
 */
static int mpd_reconnect(mpd_connection *mpd_conn)
{
	extern struct preferences prefs;
	struct s_exception e = EXCEPTION_INIT;

	if (mpd_conn->status == CONNECTED) {
		mpd_disconnect(mpd_conn);
	}
	
	mpd_connect(mpd_conn, prefs.mpd_hostname, prefs.mpd_port, 
			prefs.mpd_timeout, &e);
	if (e.code != 0) {
		scmpc_log(DEBUG, "MPD connection error: %s", e.msg);
		goto mpd_reconnect_error;
	}

	if (strlen(prefs.mpd_password) > 0) {
		mpd_send_password(mpd_conn, prefs.mpd_password, &e);
		if (e.code != 0) {
			if (e.code == INVALID_PASSWORD) {
				scmpc_log(ERROR, "Your password was not accepted by MPD. "
						"Please correct it and restart this program.");
			} else {
				scmpc_log(DEBUG, "MPD connection error: %s", e.msg);
			}
			goto mpd_reconnect_error;
		}
	}

	mpd_check_server(mpd_conn, &e);
	if (e.code != 0) {
		if (e.code == INVALID_PASSWORD) {
			scmpc_log(ERROR, "You do not have read access to the MPD server. "
					"Please correct your password and restart this program.");
		}
		goto mpd_reconnect_error;
	}

	scmpc_log(INFO, "Connected to mpd.");
	return 1;

mpd_reconnect_error:
	exception_clear(e);
	return 0;
}

/**
 * mpd_clear_song():
 *
 * Frees all the memory allocated by mpd_get_song().
 */
static void mpd_clear_song(mpd_song *song)
{
	if (song == NULL)
		return;
	
	free(song->artist);
	free(song->album);
	free(song->title);
	free(song->filename);
	free(song);
}

/**
 * mpd_get_song():
 *
 * Returns a dynamically allocated mpd_song struct filled with the relevant
 * information from mpd. If the player is not playing, everything but
 * state will be empty. Once used, free the memory with mpd_clear_song().
 */
static mpd_song *mpd_get_song(mpd_connection *mpd_conn, struct s_exception *e)
{
	char *line, *l_buffer, *value;
	mpd_song *current_song = NULL;
	buffer_t *buffer = NULL;
	struct s_exception ce = EXCEPTION_INIT;
	char command[] = "command_list_begin\n"
	                 "status\n"
	                 "currentsong\n"
	                 "command_list_end\n";

	buffer = buffer_alloc();
	if (buffer == NULL) {
		e->code = OUT_OF_MEMORY;
		goto error;
	}
	
	mpd_send_command(mpd_conn, command, &ce);
	if (ce.code != 0) {
		exception_reraise(e, ce);
		goto error;
	}
	
	mpd_response(mpd_conn, buffer, &ce);
	if (ce.code != 0) {
		exception_reraise(e, ce);
		goto error;
	}

	current_song = calloc(1, sizeof(mpd_song));
	if (current_song == NULL) {
		e->code = OUT_OF_MEMORY;
		goto error;
	}

	line = strtok_r(buffer->buffer, "\n", &l_buffer);
	if (line == NULL) {
		exception_create(e, "Could not parse response to get_song command");
		goto error;
	}

	do {
		if ((value = strstr(line, ": ")) == NULL)
			continue;

		value += 2; /* Move past the ": " part of the string... */
		
		if (strncmp(line, "state", 5) == 0)
		{
			if (strcmp(value, "pause") == 0)
				current_song->mpd_state = PAUSED;
			else if (strcmp(value, "play") == 0)
				current_song->mpd_state = PLAYING;
			else if (strcmp(value, "stop") == 0)
				current_song->mpd_state = STOPPED;
			else
				current_song->mpd_state = UNKNOWN;

			if (current_song->mpd_state != PLAYING 
					&& current_song->mpd_state != PAUSED)
				goto cleanup;
		}
		else if (strncmp(line, "xfade", 5) == 0)
			current_song->crossfade = strtol(value, NULL, 10);
		else if (strncmp(line, "Artist", 6) == 0)
			current_song->artist = strdup(value);
		else if (strncmp(line, "Album", 5) == 0)
			current_song->album = strdup(value);
		else if (strncmp(line, "Title", 5) == 0)
			current_song->title = strdup(value);
		else if (strncmp(line, "file", 4) == 0)
			current_song->filename = strdup(value);
		else if (strncmp(line, "time", 4) == 0)
		{
			char *time, *t_buffer;

			time = strtok_r(value, ":", &t_buffer);
			if (time == NULL) {
				exception_create(e, "Could not parse time values.");
				goto error;
			} else {
				current_song->current_pos = strtol(time, NULL, 10);
			}

			time = strtok_r(NULL, ":", &t_buffer);
			if (time == NULL) {
				exception_create(e, "Could not parse time values.");
				goto error;
			} else {
				current_song->length = strtol(time, NULL, 10);
			}
		}
	} while ((line = strtok_r(NULL, "\n", &l_buffer)) != NULL);

cleanup:
	mpd_conn->error_count = 0;
	buffer_free(buffer);
	return current_song;
error:
	mpd_conn->error_count++;
	buffer_free(buffer);
	mpd_clear_song(current_song);
	return NULL;
}

/**
 * initialise_song_info()
 *
 * Fills in the mpd_song_info struct when a new song is detected. It also
 * performs some checking of whether the song is valid, and if not, sets the
 * song_info.submission_state to INVALID immediately.
 */
static void initialise_song_info(mpd_song *song)
{
	song_info.start_time = time(NULL);
	song_info.time_at_pause = 0;
	song_info.just_detected = FALSE; /* new_song_check() will sort this out. */
	if (song == NULL) {
		/* There is no song... */
		song_info.submission_state = INVALID;
		return;
	}
	
	song_info.submission_state = NEW;

	scmpc_log(DEBUG, "New song detected. [%s - %s]", song->artist, song->title);
	
	if (strncmp("http", song->filename, 4) == 0) {
		song_info.submission_state = INVALID;
		scmpc_log(INFO, "Not submitting: this is a stream.");
	}
	else if (song->length < 30 ) {
		song_info.submission_state = INVALID;
		scmpc_log(INFO, "Not submitting: track length %lds is too short.",
				song->length);
	}
	else if (song->title == NULL || song->artist == NULL) {
		song_info.submission_state = INVALID;
		scmpc_log(INFO, "Not submitting: file is not tagged properly.");
	}
}

/**
 * new_song_check()
 *
 * The main function in the thread. It retrieves the current song and player
 * status, and checks whether a song is valid and should be submitted to
 * audioscrobbler. If it is, it adds the song to the unsubmitted songs queue
 * with the queue_add() function.
 */
static void new_song_check(mpd_connection *mpd_conn)
{
	mpd_song *song;
	struct s_exception e = EXCEPTION_INIT;

	song = mpd_get_song(mpd_conn, &e);
	if (e.code != 0) {
		scmpc_log(ERROR,"Could not get current song: %s", e.msg);
		exception_clear(e);
		return;
	}

	/* Server isn't doing anything interesting... */
	if (song->mpd_state == STOPPED || song->mpd_state == UNKNOWN) {
		initialise_song_info(NULL);
		goto cleanup;
	}
	
	/* Check if the song has just started playing (i.e. it's new.)
	 * For some reason mpd sometimes returns the current position as 1 when
	 * it's just started playing, so this also checks if the position is 1 and
	 * if the song wasn't detected last time around, it's treated as a new song.
	 * (This should stop it being detected twice.) */

	if (song->current_pos == 0 || (song->current_pos == 1 
				&& ! song_info.just_detected)) {
		initialise_song_info(song);
		song_info.just_detected = TRUE;
		goto cleanup;
	}

	song_info.just_detected = FALSE;

	/* If crossfade is set, the new song won't be current until the old one has
	 * finished, i.e. the first time it appears current_pos will be set to
	 * the same value as that of the crossfade. It also checks to 
	 * makes sure this hasn't been detected before by checking the time
	 * difference between the time it was detected, and the crossfade. 
	 * It would still work if it was the first song in the playlist, but it 
	 * looks odd in the logs. */
	if (song->crossfade && song->current_pos == song->crossfade
			&& difftime(time(NULL), song_info.start_time) > 
				(double)song->crossfade) {
		initialise_song_info(song);
		song_info.start_time -= song->crossfade;
		goto cleanup;
	}

	/* Song is not new, so it's already been checked. */
	if (song_info.submission_state != NEW)
		goto cleanup;

	if (song->mpd_state == PAUSED) {
		/* If it wasn't paused before, set this to the time it was paused. */
		if (song_info.time_at_pause == 0)
			song_info.time_at_pause = time(NULL);
		goto cleanup;
	}

	/* The only state left is PLAYING... check if it was paused when last 
	 * checked, and if so adjust the start_time to take into account the
	 * time spent being paused.  */
	if (song_info.time_at_pause > 0) {
		/* Add the length of the pause to the start time, so nothing will know
		 * it was ever paused, and the song length will still be right. */
		song_info.start_time += (time_t)difftime(time(NULL), song_info.time_at_pause);
		song_info.time_at_pause = 0;
		goto cleanup; /* We can submit it next round. */
	}

	/* Is the song half-done, or has it been playing for more than 4 mins?
	 * It must be a valid song, or this section wouldn't have been reached. */
	if ((song->current_pos * 2) > song->length || song->current_pos > 240) {
		double realtime = difftime(time(NULL), song_info.start_time);
		double reported_time = (double)song->current_pos;

		if (fabs(realtime - reported_time) > 2.0) {
			scmpc_log(INFO, "Not submitting song, seeking detected.");
			scmpc_log(DEBUG, "realtime = %f, reported_time = %f", realtime,
					reported_time);
			song_info.submission_state = INVALID;
		} else {
			queue_add(song->artist, song->title, song->album, song->length, NULL);
			song_info.submission_state = SUBMITTED;
		}
	}
	
cleanup:
	mpd_clear_song(song);
	return;
}


/********************\
 * Public functions * 
\********************/

void *mpd_thread(void *arg)
{
	mpd_connection *mpd_conn = NULL;

	mpd_conn = mpd_connection_init();
	if (mpd_conn == NULL)
		end_program();
	
	pthread_cleanup_push(mpd_thread_cleanup, (void *)mpd_conn);

	while (1) {
		unsigned int sleep_length = 1;

		pthread_testcancel();
		if (mpd_conn->error_count >= 3 || mpd_conn->status == DISCONNECTED) {
			initialise_song_info(NULL);
			mpd_reconnect(mpd_conn);
		} else if (mpd_conn->status != BADUSER) {
			new_song_check(mpd_conn);
		}

		if (mpd_conn->error_count > 0 && (mpd_conn->error_count % 3) == 0) {
			scmpc_log(INFO, "There have been 3 MPD connection failures in "
					"succession. Waiting for 1 minute before reconnecting.");
			sleep_length =  60;
		}

		sleep(sleep_length);
			
	}

	pthread_cleanup_pop(0);
}
