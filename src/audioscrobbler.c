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

#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <curl/curl.h>
#include <assert.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "exception.h"
#include "misc.h"
#include "scmpc.h"
#include "preferences.h"
#include "audioscrobbler.h"
#include "md5.h"

/********************************
 ** Static function prototypes **
 ********************************/

static void as_thread_cleanup(void *as_conn);

static void queue_remove_songs(struct queue_node *song, 
		struct queue_node *keep_ptr);
static void queue_save(void);
static void queue_load(void);

/**********************
 ** Global variables **
 **********************/

#define CLIENT_ID "spc"
#define HANDSHAKE_URL \
	"http://post.audioscrobbler.com/?hs=true&p=1.1&c=%s&v=%s&u=%s"

extern struct preferences prefs;

pthread_mutex_t submission_queue_mutex;
static struct queue_t {
	struct queue_node *first;
	struct queue_node *last;
	long int length;
} queue;

/* I don't like this, but I don't think I'll get useful error messages from
 * libcurl without it... */
char curl_error_buffer[CURL_ERROR_SIZE];


/********************************************
 ** Audioscrobbler communication functions **
 ********************************************/

static struct as_connection *as_connection_init(void)
{
	struct as_connection *as_conn;
	
	as_conn = calloc(sizeof(struct as_connection), 1);
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
	curl_easy_setopt(as_conn->handle, CURLOPT_ERRORBUFFER, &curl_error_buffer);

	curl_easy_setopt(as_conn->handle, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(as_conn->handle, CURLOPT_CONNECTTIMEOUT, 5L);

	return as_conn;
}

static void as_connection_cleanup(struct as_connection *as_conn)
{ 
	curl_slist_free_all(as_conn->headers);
	curl_easy_cleanup(as_conn->handle);
	free(as_conn->submit_url);
	free(as_conn);
}

static void as_set_password(struct as_connection *as_conn, 
		const char *challenge, const char *password)
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
	return 10;
}

static int build_querystring(char **qs, struct as_connection *as_conn, 
		struct queue_node **last_song)
{
	char *username, *artist, *title, *album, *time, *nqs, *tmp;
	size_t length = 1024;
	int ret, num = 0;
	struct queue_node *song = queue.first;
	
	if ((*qs = malloc(length)) == NULL)
		return -1;
	
	if ((username = curl_escape(prefs.as_username, 0)) == NULL)
		return -1;

	ret = asprintf(&tmp, "u=%s&s=%s", username, as_conn->password);
	curl_free(username);
	if (ret == -1) {
		goto build_querystring_error;
	}

	ret = strlcpy(*qs, tmp, length);
	free(tmp);
	if (ret >= (int)length) {
		scmpc_log(ERROR, "You cannot possibly have a username %d characters "
				"long. Please fix it and restart this program.", ret-38);
		as_conn->status = BADUSER;
		goto build_querystring_error;
	}
	
	while (song != NULL && num < 8) {
		artist = curl_escape(song->artist, 0);
		title = curl_escape(song->title, 0);
		album = curl_escape(song->album, 0);
		time = curl_escape(song->date, 0);

		ret = asprintf(&tmp, "&a[%d]=%s&t[%d]=%s&b[%d]=%s&m[%d]="
				"&l[%d]=%ld&i[%d]=%s", num, artist, num, title, num, album, 
				num, num, song->length, num, time);
		curl_free(artist); curl_free(title); curl_free(album); curl_free(time);
		if (ret == -1) {
			goto build_querystring_error;
		}

		ret = strlcat(*qs, tmp, length);
		if (ret >= (int)length) {
			/* Find the beginning of the truncated string... */
			char *end, *tmp2;
			if (asprintf(&tmp2, "&a[%d]", num) == -1) {
				free(tmp);
				goto build_querystring_error;
			}
			end = strstr(*qs, tmp2);
			/* ... and overwrite it so we can put the whole string there. */
			if (end == NULL) {
				scmpc_log(DEBUG, "Cannot find the start of the truncated "
						"string in build_querystring. Looking for %s in %s.",
						tmp2, *qs);
				free(tmp2); free(tmp);
				goto build_querystring_error;
			} else {
				*end = '\0';
				free(tmp2);
			}
			
			length *= 2;
			if ((nqs = realloc(*qs, length)) == NULL) {
				free(tmp);
				goto build_querystring_error;
			} else {
				*qs = nqs;
			}
			
			ret = strlcat(*qs, tmp, length);
			if (ret >= (int)length) {
				scmpc_log(ERROR, "This song's information is unrealistically "
						"large. Discarding.");
				free(tmp);
				song = song->next;
				continue;
			}
		}
		free(tmp);
		num++;
		song = song->next;
	}

	*last_song = song;
	return num;

build_querystring_error:
	free(*qs);
	*qs = NULL;
	return -1;
}

/**
 * as_handshake()
 *
 * Connects to audioscrobbler, and fills the as_conn struct with the received
 * data.
 */
static void as_handshake(struct as_connection *as_conn)
{
	unsigned long retry_time;
	buffer_t *buffer;
	char *handshake_url, *line, *s_buffer;
	int ret;

	if (strlen(prefs.as_username) == 0 || strlen(prefs.as_password) == 0) {
		scmpc_log(INFO, "No username or password specified. Not connecting to "
				"audioscrobbler.");
		as_conn->status = BADUSER;
		return;
	}

	/* We should wait at least half an hour between handshake attempts. */
	retry_time = (unsigned long)difftime(as_conn->last_handshake, time(NULL));
	if (as_conn->last_handshake != 0 && retry_time < 1800) {
		scmpc_log(DEBUG, "Last handshake was less than 30 minutes ago. "
				"Sleeping for %d seconds.", retry_time);
		as_conn->interval = retry_time;
		return;
	}
	
	buffer = buffer_alloc();
	if (buffer == NULL)
		return;
	
	ret = asprintf(&handshake_url, HANDSHAKE_URL, CLIENT_ID, SCMPC_VERSION,
			prefs.as_username);
	if (ret == -1)
		return;

	curl_easy_setopt(as_conn->handle, CURLOPT_WRITEDATA, (void *)buffer);
	curl_easy_setopt(as_conn->handle, CURLOPT_HTTPGET, TRUE);
	curl_easy_setopt(as_conn->handle, CURLOPT_URL, handshake_url);
	
	ret = curl_easy_perform(as_conn->handle);
	free(handshake_url);
	
	if (ret != 0) {
		scmpc_log(ERROR, "Could not connect to the audioscrobbler server: %s",
				curl_easy_strerror(ret));
		goto as_handshake_error;
	}

	line = strtok_r(buffer->buffer, "\n", &s_buffer);
	if (line == NULL) {
		scmpc_log(DEBUG, "Could not parse audioscrobbler handshake response.");
		goto as_handshake_error;
	}

	if (strncmp(line, "UP", 2) == 0) {
		unsigned short int line_no = 1;
		
		if (strncmp(line, "UPDATE", 6) == 0) {
			scmpc_log(INFO, "There is a new version of the scmpc client "
					"available. See %s for more details.", &line[7]);
		} else if (strncmp(line, "UPTODATE", 8) != 0) {
			scmpc_log(DEBUG, "Could not parse audioscrobbler handshake "
					"response.");
			goto as_handshake_error;
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
	} else {
		scmpc_log(DEBUG, "Could not parse audioscrobbler handshake response.");
	}

as_handshake_error:
	as_conn->error_count++;
	as_conn->interval = 10;
	buffer_free(buffer);
}

/**
 * as_submit_queue()
 *
 * Submits the song to audioscrobbler. 
 */
static void as_submit_queue(struct as_connection *as_conn)
{
	buffer_t *buffer;
	char *querystring, *line, *s_buffer;
	struct queue_node *last_added;
	int ret, num_songs;
	static char last_failed[512];

	if (queue.first == NULL) {
		scmpc_log(DEBUG, "Queue is empty.");
		return;
	}
	
	if ((buffer = buffer_alloc()) == NULL)
		return;
	
	pthread_mutex_lock(&submission_queue_mutex);
	num_songs = build_querystring(&querystring, as_conn, &last_added);
	if (num_songs < 0) {
		goto as_submit_queue_error;
	} else if (num_songs == 0) {
		scmpc_log(DEBUG, "No songs added by build_querystring().");
		goto as_submit_queue_error;
	}
	
	scmpc_log(DEBUG, "querystring = %s", querystring);

	curl_easy_setopt(as_conn->handle, CURLOPT_WRITEDATA, (void *)buffer);
	curl_easy_setopt(as_conn->handle, CURLOPT_POSTFIELDS, querystring);
	curl_easy_setopt(as_conn->handle, CURLOPT_URL, as_conn->submit_url);
	
	ret = curl_easy_perform(as_conn->handle);
	if (ret != 0) {
		scmpc_log(INFO, "Failed to connect to audioscrobbler: %s", 
				curl_easy_strerror(ret));
		as_conn->error_count++;
		goto as_submit_queue_error;
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
		last_failed[0] = '\0';
		/* TODO: "May need to re-handshake"... */
		as_conn->status = BADUSER;
		scmpc_log(ERROR, "Your user details were not accepted by audioscrobbler."
				" Please correct them and restart this program.");
	} else if (strncmp(line, "OK", 2) == 0) {
		last_failed[0] = '\0';
		if (num_songs == 1)
			scmpc_log(INFO, "1 song submitted.");
		else
			scmpc_log(INFO, "%d songs submitted.", num_songs);
		as_conn->interval = check_interval(line, s_buffer);
		queue_remove_songs(queue.first, last_added);
		queue.first = last_added;
	}
	
as_submit_queue_error:
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
	struct as_connection *as_conn;
	unsigned int sleep_length;
	
	curl_global_init(CURL_GLOBAL_NOTHING);
	
	queue.first = queue.last = NULL;
	queue.length = 0;
	as_conn = as_connection_init();
	if (as_conn == NULL)
		end_program();

	queue_load();

	pthread_cleanup_push(as_thread_cleanup, (void *)as_conn);
	
	while (1) {
		pthread_testcancel();
		if (as_conn->error_count >= 3 || as_conn->status == DISCONNECTED) {
			as_handshake(as_conn);
		} else if (queue.length > 0 && as_conn->status != BADUSER ) {
			scmpc_log(DEBUG, "New songs in the queue.");
			as_submit_queue(as_conn);
		}

		if (as_conn->error_count > 0 && (as_conn->error_count % 3) == 0) {
			scmpc_log(INFO, "There have been 3 Audioscrobbler connection "
					"failures in succession. Sleeping for 30 minutes.");
			as_conn->interval = 1800;
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
	as_connection_cleanup((struct as_connection *)as_conn);
	
	queue_save();
	
	pthread_mutex_lock(&submission_queue_mutex);
	queue_remove_songs(queue.first, NULL);
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
	sleep(1);

	while (1) {
		pthread_testcancel();
		queue_save();
		sleep((unsigned int)prefs.cache_interval * 60);
	}
}



/*********************
 ** Queue functions **
 *********************/

/**
 * queue_add()
 *
 * Add a song to the queue awaiting submission. If there are too many songs in
 * the queue, this will remove the first song (the oldest) and add this song to
 * the end.
 */
void queue_add(const char *artist, const char *title, const char *album, 
		long int length, const char *date)
{
	struct queue_node *new_song;
	struct tm *time_broken_down, result;
	time_t time_s;

	if (artist == NULL || title == NULL || length < 30) {
		scmpc_log(DEBUG,"Invalid song passed to queue_add(). Rejecting.");
		return;
	}
	
	new_song = malloc(sizeof(struct queue_node));
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
	if (queue.length == prefs.queue_length) {
		struct queue_node *new_first_song = (queue.first)->next;
		if (new_first_song == NULL) {
			scmpc_log(DEBUG, "Queue is apparently too long, but there is only "
					"one accessible song in the list. New song not added.");
			goto end;
		}
		queue_remove_songs(queue.first, new_first_song);
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
 * queue_remove_songs()
 *
 * Free the memory allocated for songs in the queue. It starts from the
 * beginning or the queue and carries on either until the next song is the one
 * passed as *keep_ptr (so this is the first one that is kept) or this is the
 * last song in the queue.
 */
static void queue_remove_songs(struct queue_node *song, 
		struct queue_node *keep_ptr)
{
	struct queue_node *next_song;

	while (song != NULL && song != keep_ptr) {
		free(song->title);
		free(song->artist);
		free(song->album);
		next_song = song->next;
		free(song);
		song = next_song;
		queue.length--;
	}

	assert(! (queue.length == 0 && song != NULL));
	assert(! (queue.length != 0 && song == NULL));
	
	if (queue.length == 0) {
		queue.first = queue.last = NULL;
	}
}

/**
 * queue_save()
 *
 * Saves the unsubmitted songs queue to the file specified in prefs.cache_file.
 */
static void queue_save(void)
{
	FILE *cache_file;
	struct queue_node *current_song;
	static bool writeable_warned = FALSE;
	enum loglevel warning_level = ERROR;
	struct s_exception e = EXCEPTION_INIT;
	
	pthread_mutex_lock(&submission_queue_mutex);

	current_song = queue.first;

	cache_file = file_open(prefs.cache_file, "w", &e);
	switch (e.code) {
		case 0:
			break;
		case OUT_OF_MEMORY:
			scmpc_log(ERROR, "Out of memory.");
			pthread_mutex_unlock(&submission_queue_mutex);
			end_program();
			return;
		case USER_DEFINED:
			if (! writeable_warned)
				writeable_warned = TRUE;
			else
				warning_level = INFO;
			scmpc_log(warning_level,"Cache file (%s) cannot be opened for "
					"writing: %s", prefs.cache_file, e.msg);
			exception_clear(e);
			goto save_queue_exit;
		default:
			scmpc_log(DEBUG, "Unexpected error from file_open: %s", e.msg);
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
 * queue_load()
 *
 * Loads the unsubmitted song queue from the file specified by
 * prefs.cache_file. Uses queue_add() to add songs.
 */
static void queue_load(void)
{
	char *line = NULL, *artist, *album, *title, *date;
	long length;
	size_t buffer_size;
	FILE *cache_file;
	struct s_exception e = EXCEPTION_INIT;

	artist = title = album = date = NULL;
	length = 0;
	
	scmpc_log(DEBUG, "Loading queue.");
	
	cache_file = file_open(prefs.cache_file, "r", &e);
	switch (e.code) {
		case 0:
			break;
		case OUT_OF_MEMORY:
			scmpc_log(ERROR, "Out of memory.");
			end_program();
			return;
		case FILE_NOT_FOUND:
			return;
		case USER_DEFINED:
			scmpc_log(INFO, "Cache file (%s) cannot be opened for reading: %s",
					prefs.cache_file, e.msg);
			free(e.msg);
			return;
		default:
			scmpc_log(DEBUG, "Unexpected error from file_open: %s", e.msg);
			return;
	}
	
	while (getline(&line, &buffer_size, cache_file) != -1) {
		char *p = strchr(line, '\n');
		if (p != NULL)
			*p = '\0';

		if (strncmp(line, "# BEGIN SONG", 12) == 0) {
			/* Do nothing */
			;
		} else if (strncmp(line, "artist: ", 8) == 0) {
			free(artist);
			artist = strdup(&line[8]);
		} else if (strncmp(line, "title: ", 7) == 0) {
			free(title);
			title = strdup(&line[7]);
		} else if (strncmp(line, "album: ", 7) == 0) {
			free(album);
			album = strdup(&line[7]);
		} else if (strncmp(line, "date: ", 6) == 0) {
			free(date);
			date = strdup(&line[6]);
		} else if (strncmp(line, "length: ", 8) == 0) {
			length = strtol(&line[8], NULL, 10);
		} else if (strncmp(line, "# END SONG", 10) == 0) {
			queue_add(artist, title, album, length, date);
			free(artist); free(title); free(album); free(date);
			artist = title = album = date = NULL;
		}
		free(line);
		line = NULL;
	}
	free(line);

	free(artist); free(title); free(album); free(date);

	fclose(cache_file);
}
