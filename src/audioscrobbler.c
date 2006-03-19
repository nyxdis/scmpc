/**
 * audioscrobbler.c: The Audioscrobbler interaction code.
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


/*************
 ** Headers **
 *************/
#include <pthread.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <stdio.h>
#include <curl/curl.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "liberror.h"
#include "misc.h"
#include "md5.h"
#include "scmpc.h"
#include "libmpd.h"
#include "preferences.h"
#include "audioscrobbler.h"

/********************************
 ** Static function prototypes **
 ********************************/

static as_connection *as_connection_init(void);
static void as_connection_cleanup(as_connection *as_conn);
static void as_set_password(as_connection *as_conn, const char *challenge, 
		const char *password);
static void as_handshake(as_connection *as_conn);
static void as_submit_queue(as_connection *as_conn);

static void as_thread_cleanup(void *arg);

static void remove_songs_from_queue(queue_node *song, queue_node *keep_ptr);
static void save_queue(void);
static void load_queue(void);

/**********************
 ** Global variables **
 **********************/

#define CLIENT_ID "spc"
#define HANDSHAKE_URL \
	"http://post.audioscrobbler.com/?hs=true&p=1.1&c=%s&v=%s&u=%s"

extern struct preferences prefs;

pthread_mutex_t submission_queue_mutex;
struct {
	queue_node *first;
	queue_node *last;
	long int length;
} queue;

char *curl_error_buffer[CURL_ERROR_SIZE];


/********************************************
 ** Audioscrobbler communication functions **
 ********************************************/

static as_connection *as_connection_init(void)
{
	as_connection *as_conn;
	
	as_conn = calloc(sizeof(as_connection), 1);
	if (as_conn == NULL)
		return NULL;
	
	as_conn->submit_url = NULL;
	as_conn->interval = 1;
	as_conn->error_count = 0;
	as_conn->last_handshake = 0;
	as_conn->status = DISCONNECTED;
	as_conn->handle = curl_easy_init();
	as_conn->headers = curl_slist_append(as_conn->headers, 
			"User-Agent: scmpc/" SCMPC_VERSION);
	
	curl_easy_setopt(as_conn->handle, CURLOPT_HTTPHEADER, as_conn->headers);
	curl_easy_setopt(as_conn->handle, CURLOPT_WRITEFUNCTION, &buffer_write);
	/* curl_easy_setopt(as_conn->curl_handle, CURLOPT_VERBOSE, 1); */
	curl_easy_setopt(as_conn->handle, CURLOPT_ERRORBUFFER, curl_error_buffer);

	curl_easy_setopt(as_conn->handle, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(as_conn->handle, CURLOPT_CONNECTTIMEOUT, 5L);

	return as_conn;
}

static void as_connection_cleanup(as_connection *as_conn)
{ 
	curl_slist_free_all(as_conn->headers);
	curl_easy_cleanup(as_conn->handle);
	free(as_conn->submit_url);
	free(as_conn);
}

static void as_set_password(as_connection *as_conn, const char *challenge, 
		const char *password)
{
	md5_state_t state;
	md5_byte_t digest[16];
	char tmp[3], password_challenge[65];
	unsigned short int i;

	memset(&password_challenge, '\0', 65);
	memset(&(as_conn->password), '\0', 33);
	
	md5_init(&state);
	md5_append(&state, (const md5_byte_t *)password, strlen(password));
	md5_finish(&state, digest);
	for (i = 0; i < 16; i++) {
		sprintf(tmp, "%02x", digest[i]);
		strcat(password_challenge, tmp);
	}

	strcat(password_challenge, challenge);

	md5_init(&state);
	md5_append(&state, (const md5_byte_t *)password_challenge, 64);
	md5_finish(&state, digest);
	for (i = 0; i < 16; i++) {
		sprintf(tmp, "%02x", digest[i]);
		strcat(as_conn->password, tmp);
	}
}

static unsigned long check_interval(char *line, char *strtok_buffer)
{
	while ((line = strtok_r(NULL, "\n", &strtok_buffer)) != NULL) {
		if (strncmp(line, "INTERVAL", 8) == 0) {
			scmpc_log(DEBUG, "Received interval of %ss", &line[9]);
			return (unsigned long)strtol(&line[9], NULL, 10);
		}
	}
	return 10L;
}

/**
 * as_handshake()
 *
 * Connects to audioscrobbler, and fills the as_conn struct with the received
 * data.
 */
static void as_handshake(as_connection *as_conn)
{
	char *handshake_url, *s_buffer, *line;
	unsigned short int line_no = 1;
	int ret;
	unsigned long retry_time = 1;
	buffer_t *buffer;

	scmpc_log(DEBUG, "as_handshake() called");
	
	/* We should wait at least half an hour between handshake attempts. */
	retry_time = (unsigned long)difftime(as_conn->last_handshake, time(NULL));
	if (as_conn->last_handshake != 0 && retry_time < 1800) {
		scmpc_log(DEBUG, "Last handshake was less than 30 minutes ago. "
				"Sleeping for %d seconds.", retry_time);
		as_conn->interval = retry_time;
		return;
	}
	
	if (strlen(prefs.as_username) == 0 || strlen(prefs.as_password) == 0) {
		scmpc_log(INFO, "No username or password specified. Not connecting to "
				"audioscrobbler.");
		as_conn->status = BADUSER;
		return;
	}
	
	buffer = buffer_alloc();
	if (buffer == NULL)
		return;
	
	handshake_url = alloc_sprintf(150, HANDSHAKE_URL, CLIENT_ID, SCMPC_VERSION,
			prefs.as_username);

	curl_easy_setopt(as_conn->handle, CURLOPT_WRITEDATA, (void *)buffer);
	curl_easy_setopt(as_conn->handle, CURLOPT_HTTPGET, TRUE);
	curl_easy_setopt(as_conn->handle, CURLOPT_URL, handshake_url);
	
	ret = curl_easy_perform(as_conn->handle);
	free(handshake_url);
	if (ret != 0) {
		scmpc_log(ERROR, "Could not connect to the audioscrobbler server: %s",
				curl_easy_strerror(ret));
		goto as_handshake_exit;
	}

	line = strtok_r(buffer->buffer, "\n", &s_buffer);
	if (line == NULL) {
		scmpc_log(DEBUG, "Could not parse audioscrobbler handshake response.");
		goto as_handshake_exit;
	}

	if (strncmp(line, "UP", 2) == 0) {
		if (strncmp(line, "UPDATE", 6) == 0) {
			scmpc_log(INFO, "There is a new version of the scmpc client "
					"available. See %s for more details.", &line[7]);
		}
		while ((line = strtok_r(NULL, "\n", &s_buffer)) != NULL) {
			line_no++; /* Because /I/ don't count from 0. ;-) */
			if (line_no == 2) {
				as_set_password(as_conn, line, prefs.as_password);
			} else if (line_no == 3) {
				free(as_conn->submit_url);
				as_conn->submit_url = strdup(line);
				break;
			}
		}
		if (line_no < 3) {
			scmpc_log(DEBUG, "Truncated data from server returned. Found %d "
					"lines, expected 3 lines or more.", line_no);
		} else {
			scmpc_log(INFO, "Connected to Audioscrobbler.");
			as_conn->status = CONNECTED;
			as_conn->error_count = 0;
			as_conn->last_handshake = time(NULL);
			as_conn->interval = check_interval(line, s_buffer);
			buffer_free(buffer);
			return;
		}
	} else if (strncmp(line, "FAILED", 6) == 0) {
		scmpc_log(ERROR, "The Audioscrobbler handshake couldn't be completed: "
				"%s", &line[7]);
		as_conn->interval = check_interval(line, s_buffer);
	} else if (strncmp(line, "BADUSER", 7) == 0) {
		scmpc_log(ERROR, "The user details you specified were not accepted by "
				"Audioscrobbler. Please correct them and restart this program.");
		as_conn->status = BADUSER;
	}

as_handshake_exit:
	as_conn->error_count++;
	scmpc_log(DEBUG, "as_handshake() exiting. error_count = %d", as_conn->error_count);
	/* Every time it fails 3 times in succession, wait for 30 minutes. */
	if (as_conn->error_count % 3 == 0)
		as_conn->interval = 1800;
	else
		as_conn->interval = 10;
	buffer_free(buffer);
}

static char *add_song_to_querystring(char *querystring, queue_node *song, int num)
{
	char *artist, *title, *album, *time, *new_querystring;
	/*
	 * u=&s=&a[0]=&t[0]=&b[0]=&m[0]=&l[0]=&i[0]=
	 */
	artist = curl_escape(song->artist, 0);
	title = curl_escape(song->title, 0);
	album = curl_escape(song->album, 0);
	time = curl_escape(song->date, 0);

	new_querystring = alloc_sprintf(256*(num+1), "%s&a[%d]=%s&t[%d]=%s"
			"&b[%d]=%s&m[%d]=&l[%d]=%ld&i[%d]=%s", querystring, num, artist, 
			num, title, num, album, num, num, song->length, num, time);
	
	curl_free(artist);
	curl_free(title);
	curl_free(album);
	curl_free(time);
	free(querystring);

	return new_querystring;
}

/**
 * as_submit_queue()
 *
 * Submits the song to audioscrobbler. 
 */
static void as_submit_queue(as_connection *as_conn)
{
	queue_node *song;
	short int num_songs = 0;
	char *querystring,  *username, *line, *s_buffer;
	int ret;
	buffer_t *buffer;
	static char last_failed[512];
	
	if (queue.first == NULL)
		return;
	
	buffer = buffer_alloc();
	if (buffer == NULL)
		return;

	username = curl_escape(prefs.as_username, 0);
	if (username == NULL)
		return;

	memset(last_failed, '\0', 512);
	
	querystring = alloc_sprintf(75, "u=%s&s=%s", username, as_conn->password);

	curl_free(username);
	
	pthread_mutex_lock(&submission_queue_mutex);

	song = queue.first;

	while (song != NULL && num_songs < 8) {
		querystring = add_song_to_querystring(querystring, song, num_songs);
		num_songs++;
		song = song->next;
	}

	/* scmpc_log(DEBUG, "querystring = %s", querystring); */

	curl_easy_setopt(as_conn->handle, CURLOPT_WRITEDATA, (void *)buffer);
	curl_easy_setopt(as_conn->handle, CURLOPT_POSTFIELDS, querystring);
	curl_easy_setopt(as_conn->handle, CURLOPT_URL, as_conn->submit_url);

	ret = curl_easy_perform(as_conn->handle);
	if (ret != 0) {
		scmpc_log(INFO, "Failed to connect to audioscrobbler: %s", 
				curl_easy_strerror(ret));
		as_conn->error_count++;
		goto as_submit_queue_exit;
	} else {
		as_conn->error_count = 0;
	}

	line = strtok_r(buffer->buffer, "\n", &s_buffer);
	if (line == NULL) {
		scmpc_log(ERROR, "Could not parse audioscrobbler submit response.");
	} else if (strncmp(line, "FAILED", 6) == 0) {
		if (strcmp(last_failed, &line[7]) != 0) {
			strlcpy(last_failed, &line[7], sizeof(last_failed));
			scmpc_log(INFO, "Audioscrobbler returned FAILED: %s", &line[7]);
		}
		as_conn->interval = check_interval(line, s_buffer);
	} else if (strncmp(line, "BADAUTH", 7) == 0) {
		memset(last_failed, '\0', 512);
		/* TODO: "May need to re-handshake"... */
		as_conn->status = BADUSER;
		scmpc_log(ERROR, "Your user details were not accepted by audioscrobbler."
				" Please correct them and restart this program.");
	} else if (strncmp(line, "OK", 2) == 0) {
		memset(last_failed, '\0', 512);
		if (num_songs == 1)
			scmpc_log(INFO, "1 song submitted.");
		else
			scmpc_log(INFO, "%d songs submitted.", num_songs);
		as_conn->interval = check_interval(line, s_buffer);
		remove_songs_from_queue(queue.first, song);
		queue.first = song;
	}

as_submit_queue_exit:
	free(querystring);
	buffer_free(buffer);
	pthread_mutex_unlock(&submission_queue_mutex);
}


/**********************
 ** Thread functions **
 **********************/

/**
 * as_thread():
 *
 * The entry point to the audioscrobbler thread. It is in charge of
 * handshaking, and submitting the songs in the queue, obeying the INTERVAL
 * specified by the audioscrobbler server.
 */
void *as_thread(void *arg)
{
	as_connection *as_conn;
	unsigned int sleep_length;
	
	curl_global_init(CURL_GLOBAL_NOTHING);
	
	queue.first = queue.last = NULL;
	queue.length = 0;
	as_conn = as_connection_init();
	if (as_conn == NULL)
		end_program();

	load_queue();

	pthread_cleanup_push(as_thread_cleanup, (void *)as_conn);
	
	while (1) {
		pthread_testcancel();
		if (as_conn->error_count >= 3 || as_conn->status == DISCONNECTED) {
			as_handshake(as_conn);
		} else if (queue.length > 0 && as_conn->status != BADUSER ) {
			scmpc_log(DEBUG, "New songs in the queue.");
			as_submit_queue(as_conn);
		}
		/* Sleep for a minimum of 1 second, or the last received INTERVAL. */
		sleep_length = (as_conn->interval == 0) ? 1 : as_conn->interval;
		if (sleep_length > 1)
			scmpc_log(DEBUG, "[AS] Sleeping for %d seconds", sleep_length);
		sleep(sleep_length);
	}

	pthread_cleanup_pop(0);
}

/**
 * as_thread_cleanup()
 *
 * Frees the data allocated when the thread starts, and calls the curl cleanup
 * functions.
 */
static void as_thread_cleanup(void *as_conn)
{
	as_connection_cleanup((as_connection *)as_conn);
	
	save_queue();
	
	pthread_mutex_lock(&submission_queue_mutex);
	remove_songs_from_queue(queue.first, NULL);
	pthread_mutex_unlock(&submission_queue_mutex);

	curl_global_cleanup();
}

/**
 * cache_thread()
 *
 * This very simple thread just saves the submission queue as often as was
 * specified in prefs.cache_interval.
 */
void *cache_thread(void *arg)
{
	while (1) {
		pthread_testcancel();
		save_queue();
		sleep(prefs.cache_interval * 60);
	}
}



/*********************
 ** Queue functions **
 *********************/

/**
 * add_song_to_queue()
 *
 * Add a song to the queue awaiting submission. If there are too many songs in
 * the queue, this will remove the first song (the oldest) and add this song to
 * the end.
 */
void __add_song_to_queue(const char *artist, const char *title, 
		const char *album, long int length, const char *date)
{
	queue_node *new_song;
	struct tm *time_broken_down, result;
	time_t time_s;

	if (artist == NULL || title == NULL || length < 30) {
		scmpc_log(DEBUG,"Invalid song passed to add_song_to_queue. Rejecting.");
		return;
	}
	
	new_song = malloc(sizeof(queue_node));
	if (new_song == NULL)
		return;

	new_song->title = strdup(title);
	new_song->artist = strdup(artist);
	if (album != NULL)
		new_song->album = strdup(album);
	else
		new_song->album = strdup("");
	new_song->length = length;
	new_song->next = NULL;

	if (date == NULL) {
		time(&time_s);
		time_broken_down = gmtime_r(&time_s, &result);
		strftime(new_song->date, 20, "%Y-%m-%d %H:%M:%S", time_broken_down);
	} else {
		strlcpy(new_song->date, date, sizeof(new_song->date));
	}
	
	pthread_mutex_lock(&submission_queue_mutex);

	if (queue.first == NULL) {
		queue.first = queue.last = new_song;
		queue.length = 1;
		scmpc_log(DEBUG, "Song added to queue. Queue length: 1");
		goto end;
	}
	
	/* Queue is too long. Remove the first item before adding this one. */
	if (queue.length + 1 > prefs.queue_length) {
		queue_node *new_first_song = (queue.first)->next;
		if (new_first_song == NULL) {
			scmpc_log(DEBUG, "Queue apparently too long, but there is only "
					"one accessible song in the list. New song not added.");
			goto end;
		}
		remove_songs_from_queue(queue.first, new_first_song);
		queue.first = new_first_song;
		scmpc_log(INFO, "The queue of songs to be submitted is too long. "
				"Removing the first item.");
	}
	(queue.last)->next = new_song;
	queue.last = new_song;
	queue.length++;
	scmpc_log(DEBUG, "Song added to queue. Queue length: %d", queue.length);

end:
	pthread_mutex_unlock(&submission_queue_mutex);
}

/**
 * remove_songs_from_queue()
 *
 * Free the memory allocated for songs in the queue. It starts from the
 * beginning or the queue and carries on either until the next song is the one
 * passed as *keep_ptr (so this is the first one that is kept) or this is the
 * last song in the queue.
 */
static void remove_songs_from_queue(queue_node *song, queue_node *keep_ptr)
{
	queue_node *next_song;

	while (song != NULL && song != keep_ptr) {
		free(song->title);
		free(song->artist);
		free(song->album);
		next_song = song->next;
		free(song);
		song = next_song;
		queue.length--;
	}
}

/**
 * save_queue
 *
 * Saves the unsubmitted songs queue to the file specified in prefs.cache_file.
 */
static void save_queue(void)
{
	FILE *cache_file;
	queue_node *current_song;
	static bool writeable_warned = FALSE;
	enum loglevel warning_level = ERROR;
	error_t *error;
	
	pthread_mutex_lock(&submission_queue_mutex);

	current_song = queue.first;

	cache_file = file_open(prefs.cache_file, "w", &error);
	if (ERROR_IS_SET(error)) {
		if (! writeable_warned)
			writeable_warned = TRUE;
		else
			warning_level = INFO;
		scmpc_log(warning_level,"Cache file (%s) cannot be opened for writing"
				": %s", prefs.cache_file, error->msg);
		error_clear(error);
		goto save_queue_exit;
	}
	
	while (current_song != NULL) {
		fprintf(cache_file, "# BEGIN SONG\n"
				"artist: %s\n"
				"title: %s\n"
				"album: %s\n"
				"length: %ld\n"
				"date: %s\n"
				"# END SONG\n\n", current_song->artist, current_song->title,
				current_song->album, current_song->length, current_song->date);
		current_song = current_song->next;
	}
	fclose(cache_file);

	scmpc_log(DEBUG, "Cache saved.");
	
save_queue_exit:
	pthread_mutex_unlock(&submission_queue_mutex);
}

/**
 * load_queue()
 *
 * Loads the unsubmitted song queue from the file specified by
 * prefs.cache_file. Uses to __add_song_to_queue() to add songs.
 */
static void load_queue(void)
{
	char *line, *artist, *album, *title, *date;
	long length;
	FILE *cache_file;
	error_t *error;

	artist = title = album = date = NULL;
	length = 0;
	
	cache_file = file_open(prefs.cache_file, "r", &error);
	if (ERROR_IS_SET(error)) {
		if (error == ERROR_OUT_OF_MEMORY)
			return;
		scmpc_log(DEBUG, "Cache file (%s) cannot be opened for reading"
				": %s", prefs.cache_file, error->msg);
		error_clear(error);
		return;
	}
	
	while ((line = read_line_from_file(cache_file)) != NULL) {
		if (strncmp(line, "# BEGIN SONG", 12) == 0) {
			artist = title = album = date = NULL;
			length = 0;
		} else if (strncmp(line, "artist: ", 8) == 0) {
			artist = strdup(&line[8]);
		} else if (strncmp(line, "title: ", 7) == 0) {
			title = strdup(&line[7]);
		} else if (strncmp(line, "album: ", 7) == 0) {
			album = strdup(&line[7]);
		} else if (strncmp(line, "date: ", 6) == 0) {
			date = strdup(&line[6]);
		} else if (strncmp(line, "length: ", 8) == 0) {
			length = strtol(&line[8], NULL, 10);
		} else if (strncmp(line, "# END SONG", 10) == 0) {
			__add_song_to_queue(artist, title, album, length, date);
			free(artist); free(title); free(album); free(date);
		}
		free(line);
	}

	fclose(cache_file);
}
