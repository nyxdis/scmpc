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

#include "liberror.h"
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
 * Called when the thread exits. It clears any mpd_errors hanging around, and
 * disconnects from mpd.
 */
static void mpd_thread_cleanup(void *mpd_conn)
{
	mpd_connection **conn = (mpd_connection **)mpd_conn;
	mpd_disconnect(*conn);
}

/**
 * mpd_reconnect()
 *
 * ...
 */
static void mpd_reconnect(mpd_connection **mpd_conn)
{
	extern struct preferences prefs;
	error_t *error = NULL;
	bool new_connection = TRUE;

	if (*mpd_conn != NULL) {
		new_connection = FALSE;
		scmpc_log(ERROR, "Connection error, disconnecting from MPD.");
	}
	
	while (1) {
		if (*mpd_conn != NULL)
			mpd_disconnect(*mpd_conn);
		*mpd_conn = mpd_connect(prefs.mpd_hostname, prefs.mpd_port, 
				prefs.mpd_timeout, &error);
		if (ERROR_IS_SET(error)) {
			error_clear(error); /* Ignore the error, and simply try */
			sleep(30);             /* again until we do reconnect. */
			continue;
		}
		
		if (strlen(prefs.mpd_password) > 0) {
			mpd_send_password(*mpd_conn, prefs.mpd_password, &error);
			if (ERROR_IS_SET(error)) {
				error_clear(error);
				sleep(30);
				continue;
			}
		}

		mpd_check_server(*mpd_conn, &error);
		if (ERROR_IS_SET(error)) {
			if (error == ERROR_OUT_OF_MEMORY)
				continue;
			if (new_connection)
				scmpc_log(ERROR, "Could not check mpd status: %s", error->msg);
			error_clear(error);
			sleep(30);
			continue;
		}
		if (new_connection)
			scmpc_log(INFO, "Connected to mpd.");
		else
			scmpc_log(ERROR, "Successfully reconnected to MPD.");
		break;
	}
}

/**
 * mpd_get_song():
 *
 * Returns a dynamically allocated mpd_song struct filled with the relevant
 * information from mpd. If the player is not playing, everything but
 * state will be empty. Once used, free the memory with mpd_clear_song().
 *
 * Returns null if an error occurs conversing with the deamon, so check
 * mpd_conn->error after using the command.
 */
static mpd_song *mpd_get_song(mpd_connection *mpd_conn, error_t **error)
{
	char *line, *l_buffer, *value;
	mpd_song *current_song;
	error_t *child_error = NULL;
	buffer_t *buffer;
	char command[] = "command_list_begin\n"
	                 "status\n"
	                 "currentsong\n"
	                 "command_list_end\n";

	buffer = buffer_alloc();
	if (buffer == NULL) {
		*error = ERROR_OUT_OF_MEMORY;
		return NULL;
	}
	
	mpd_send_command(mpd_conn, command, &child_error);
	if (ERROR_IS_SET(child_error)) {
		*error = error_set(-1, "Could not send command to mpd server.", 
				child_error);
		return NULL;
	}
	
	mpd_server_response(mpd_conn, "OK\n", buffer, &child_error);
	if (ERROR_IS_SET(child_error)) {
		*error = error_set(-1, "Error occurred while waiting for the server "
				"to reply.", child_error);
		goto error;
	}

	current_song = calloc(1, sizeof(mpd_song));
	if (current_song == NULL) {
		*error = ERROR_OUT_OF_MEMORY;
		goto error;
	}

	line = strtok_r(buffer->buffer, "\n", &l_buffer);
	if (line == NULL) {
		*error = error_set(-1, "Could not parse response to get_song command",
				NULL);
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
				*error = error_set(-1, "Could not parse time values.", NULL);
				goto cleanup;
			} else {
				current_song->current_pos = strtol(time, NULL, 10);
			}

			time = strtok_r(NULL, ":", &t_buffer);
			if (time == NULL) {
				*error = error_set(-1, "Could not parse time values.", NULL);
				goto cleanup;
			} else {
				current_song->length = strtol(time, NULL, 10);
			}
		}
	} while ((line = strtok_r(NULL, "\n", &l_buffer)) != NULL);

cleanup:
	buffer_free(buffer);
	return current_song;
error:
	buffer_free(buffer);
	return NULL;
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
 * audioscrobbler. If it is, it notifies the audioscrobbler thread using its
 * add_song_to_queue() function.
 */
static void new_song_check(mpd_connection *mpd_conn)
{
	mpd_song *song;
	error_t *error = NULL;

	song = mpd_get_song(mpd_conn, &error);
	if (ERROR_IS_SET(error)) {
		scmpc_log(ERROR,"Could not get current song: %s", error->msg);
		error_clear(error);
		/* mpd_clear_song() will do nothing if the song is NULL anyway. */
		goto cleanup;
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
			add_song_to_queue(song->artist, song->title, song->album, 
					song->length);
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
	struct timespec sleeptime;

	/* Sleep for a second. */
	sleeptime.tv_sec = 1;
	sleeptime.tv_nsec = 0;
	
	pthread_cleanup_push(mpd_thread_cleanup, (void *)&mpd_conn);

	while (1) {
		pthread_testcancel();
		if (mpd_conn == NULL || mpd_conn->error_count >= 3 
				|| mpd_conn->status == DISCONNECTED) {
			initialise_song_info(NULL);
			mpd_reconnect(&mpd_conn);
		} else if (mpd_conn->status != BADUSER) {
			new_song_check(mpd_conn);
		}
		nanosleep(&sleeptime, NULL);
	}

	pthread_cleanup_pop(0);
}
