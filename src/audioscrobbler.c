/**
 * audioscrobbler.c: Audioscrobbler backend.
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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "misc.h"
#include "mpd.h"
#include "preferences.h"
#include "audioscrobbler.h"
#include "queue.h"

static char curl_error_buffer[CURL_ERROR_SIZE];

#define HANDSHAKE_URL \
	"http://post.audioscrobbler.com/?hs=true&p=1.2.1&c=spc&v=%s&u=%s&t=%ld&a=%s"

int as_connection_init(void)
{
	as_conn = calloc(sizeof(struct as_connection),1);
	if(as_conn == NULL) return -1;

	as_conn->last_handshake = 0;
	as_conn->status = DISCONNECTED;
	as_conn->handle = curl_easy_init();
	if(as_conn->handle == NULL) return -1;
	as_conn->headers = curl_slist_append(as_conn->headers,
			"User-Agent: scmpc/" PACKAGE_VERSION);

	curl_easy_setopt(as_conn->handle,CURLOPT_HTTPHEADER,as_conn->headers);
	curl_easy_setopt(as_conn->handle,CURLOPT_WRITEFUNCTION,&buffer_write);
	curl_easy_setopt(as_conn->handle,CURLOPT_ERRORBUFFER,curl_error_buffer);
	curl_easy_setopt(as_conn->handle,CURLOPT_NOSIGNAL,1L);
	curl_easy_setopt(as_conn->handle,CURLOPT_CONNECTTIMEOUT,5L);
	return 0;
}

void as_cleanup(void)
{
	curl_slist_free_all(as_conn->headers);
	curl_easy_cleanup(as_conn->handle);
	free(as_conn->session_id);
	free(as_conn->np_url);
	free(as_conn->submit_url);
	free(as_conn);
}

void as_handshake(void)
{
	char *auth_token, *handshake_url, *line, *saveptr, *tmp;
	time_t timestamp;
	int ret;

	if(as_conn->status == BADAUTH) {
		scmpc_log(INFO,"Refusing handshake, please check your "
			"Audioscrobbler credentials and restart %s",
			PACKAGE_NAME);
		return;
	}

	if(strlen(prefs.as_username) == 0 || (strlen(prefs.as_password) == 0 &&
		strlen(prefs.as_password_hash) == 0)) {
		scmpc_log(INFO,"No username or password specified. "
				"Not connecting to Audioscrobbler.");
		as_conn->status = BADAUTH;
		return;
	}

	timestamp = time(NULL);

	if(difftime(timestamp,as_conn->last_handshake) < 1800) {
		scmpc_log(DEBUG,"Requested handshake, but last handshake "
				"was less than 30 minutes ago.");
		return;
	}

	if(strlen(prefs.as_password_hash) > 0) {
		if(asprintf(&tmp,"%s%ld",prefs.as_password_hash,timestamp) < 0) return;
	} else {
		auth_token = md5_hash(prefs.as_password);
		if(asprintf(&tmp,"%s%ld",auth_token,timestamp) < 0) return;
		free(auth_token);
	}
	auth_token = md5_hash(tmp);
	free(tmp);
	if(asprintf(&handshake_url,HANDSHAKE_URL,PACKAGE_VERSION,prefs.as_username,
		timestamp,auth_token) < 0) return;
	free(auth_token);

	scmpc_log(DEBUG,"handshake_url = %s",handshake_url);

	curl_easy_setopt(as_conn->handle,CURLOPT_WRITEDATA,(void *)buffer);
	curl_easy_setopt(as_conn->handle,CURLOPT_HTTPGET,1);
	curl_easy_setopt(as_conn->handle,CURLOPT_URL,handshake_url);

	ret = curl_easy_perform(as_conn->handle);
	free(handshake_url);

	if(ret != 0) {
		scmpc_log(ERROR,"Could not connect to the Audioscrobbler: %s",
			curl_easy_strerror(ret));
		free(buffer);
		return;
	}

	line = strtok_r(buffer,"\n",&saveptr);
	if(line == NULL) {
		scmpc_log(DEBUG,"Could not parse Audioscrobbler handshake response.");
		free(buffer);
		return;
	}

	if(strncmp(line,"OK",2) == 0) {
		unsigned short int line_no = 1;

		while((line = strtok_r(NULL,"\n",&saveptr)) != NULL) {
			line_no++;
			if(line_no == 2) {
				as_conn->session_id = strdup(line);
			} else if(line_no == 3) {
				as_conn->np_url = strdup(line);
			} else if(line_no == 4) {
				as_conn->submit_url = strdup(line);
				break;
			}
		}
		if(line_no < 4) {
			scmpc_log(DEBUG,"Truncated data from server returned. Found %d"
				" lines, expected 4 lines or more.",line_no);
		} else {
			scmpc_log(INFO,"Connected to Audioscrobbler.");
			as_conn->status = CONNECTED;
			as_conn->last_handshake = time(NULL);
		}
	} else if(strncmp(line,"FAILED",6) == 0) {
		scmpc_log(ERROR,"The Audioscrobbler handshake could not be "
			"completed: %s",&line[7]);
	} else if(strncmp(line,"BADAUTH",7) == 0) {
		scmpc_log(ERROR,"The user details you specified were not "
				"accepted by Audioscrobbler. Please correct "
				"them and restart this program.");
		as_conn->status = BADAUTH;
	} else if(strncmp(line,"BADTIME",7) == 0) {
		scmpc_log(ERROR,"Handshake failed because your system time is "
				"too far off. Please correct your clock.");
	} else {
		scmpc_log(DEBUG,"Could not parse Audioscrobbler handshake response.");
	}
	free(buffer);
}

void as_now_playing(void)
{
	char *querystring, *artist, *album, *title, *line;
	int ret;

	if(as_conn->status != CONNECTED) {
		scmpc_log(INFO,"Not sending Now Playing notification: not connected");
		return;
	}

	artist = curl_easy_escape(as_conn->handle,current_song.artist,0);
	album = curl_easy_escape(as_conn->handle,current_song.album,0);
	title = curl_easy_escape(as_conn->handle,current_song.title,0);

	if(asprintf(&querystring,"s=%s&a=%s&t=%s&b=%s&l=%d&n=%d&m=",
		as_conn->session_id,artist,title,album,current_song.length,
		current_song.track) < 0) return;

	free(artist);
	free(album);
	free(title);

	scmpc_log(DEBUG,"querystring = %s",querystring);

	curl_easy_setopt(as_conn->handle,CURLOPT_WRITEDATA,(void*)buffer);
	curl_easy_setopt(as_conn->handle,CURLOPT_POSTFIELDS,querystring);
	curl_easy_setopt(as_conn->handle,CURLOPT_URL,as_conn->np_url);

	ret = curl_easy_perform(as_conn->handle);
	free(querystring);
	if(ret != 0) {
		scmpc_log(ERROR,"Failed to connect to Audioscrobbler: %s",
			curl_easy_strerror(ret));
		free(buffer);
		return;
	}

	line = strtok(buffer,"\n");
	if(line == NULL)  {
		scmpc_log(INFO,"Could not parse Audioscrobbler submit response.");
	} else if(strncmp(line,"BADSESSION",10) == 0) {
		scmpc_log(INFO,"Received bad session response from "
			"Audioscrobbler, re-handshaking.");
		as_handshake();
		as_now_playing();
	} else if(strncmp(line,"OK",2) == 0) {
		scmpc_log(INFO,"Sent Now Playing notification.");
	} else {
		scmpc_log(DEBUG,"Unknown response from Audioscrobbler while "
			"sending Now Playing notification.");
	}
	free(buffer);
}

static int build_querystring(char **qs, struct queue_node **last_song)
{
	char *artist, *title, *album, *nqs, *tmp;
	int ret, num = 0;
	size_t buffer_length = 1024, current_length = 0;
	struct queue_node *song = queue.first;

	if((*qs = malloc(buffer_length)) == NULL)
		return -1;

	if(asprintf(&tmp,"s=%s",as_conn->session_id) < 0) return -1;
	strcpy(*qs,tmp);
	free(tmp);

	while(song != NULL && num < 10) {
		artist = curl_easy_escape(as_conn->handle,song->artist,0);
		title = curl_easy_escape(as_conn->handle,song->title,0);
		album = curl_easy_escape(as_conn->handle,song->album,0);

		ret = asprintf(&tmp,"&a[%d]=%s&t[%d]=%s&i[%d]=%ld&o[%d]=P"
			"&r[%d]=&l[%d]=%d&b[%d]=%s&n[%d]=&m[%d]=",num,artist,
			num,title,num,song->date,num,num,num,song->length,num,
			album,num,num);
		curl_free(artist); curl_free(title); curl_free(album);
		if(ret < 0) {
			free(*qs);
			*qs = NULL;
			return -1;
		}

		current_length += ret;

		if(current_length > buffer_length) {
			buffer_length *= 2;
			if((nqs = realloc(*qs,buffer_length)) == NULL) {
				free(tmp);
				free(*qs);
				*qs = NULL;
				return -1;
			} else {
				*qs = nqs;
			}
		}

		if(strlen(*qs) + strlen(tmp) > buffer_length) {
			scmpc_log(ERROR,"This song's information is too long."
				"Discarding.");
			free(tmp);
			song = song->next;
			continue;
		}
		free(tmp);
		num++;
		song = song->next;
	}

	*last_song = song;
	return num;
}

int as_submit(void)
{
	char *querystring, *line, *saveptr;
	struct queue_node *last_added;
	int ret, num_songs;
	static char last_failed[512];

	if(queue.first == NULL)
		return -1;
	
	num_songs = build_querystring(&querystring, &last_added);
	if(num_songs <= 0) {
		free(querystring);
		return -1;
	}

	scmpc_log(DEBUG,"querystring = %s",querystring);

	curl_easy_setopt(as_conn->handle, CURLOPT_WRITEDATA, (void *)buffer);
	curl_easy_setopt(as_conn->handle, CURLOPT_POSTFIELDS, querystring);
	curl_easy_setopt(as_conn->handle, CURLOPT_URL, as_conn->submit_url);

	if((ret = curl_easy_perform(as_conn->handle)) != 0) {
		scmpc_log(INFO,"Failed to connect to Audioscrobbler: %s",
			curl_easy_strerror(ret));
	}

	line = strtok_r(buffer,"\n",&saveptr);
	if(line == NULL)
		scmpc_log(INFO,"Could not parse Audioscrobbler submit response.");
	else if(strncmp(line,"FAILED",6) == 0) {
		if(strcmp(last_failed,&line[7]) != 0) {
			memset(last_failed,0,sizeof(last_failed));
			strncpy(last_failed,&line[7],sizeof(last_failed));
			scmpc_log(INFO,"Audioscrobbler returned FAILED: %s",
				&line[7]);
		}
	} else if(strncmp(line,"BADSESSION",10) == 0) {
		last_failed[0] = '\0';
		scmpc_log(INFO,"Received bad session from Audioscrobbler, re-handshaking.");
		as_handshake();
	} else if(strncmp(line,"OK",2) == 0) {
		last_failed[0] = '\0';
		if(num_songs == 1)
			scmpc_log(INFO,"1 song submitted.");
		else
			scmpc_log(INFO,"%d songs submitted.",num_songs);
		queue_remove_songs(queue.first, last_added);
		queue.first = last_added;
	}
	return 0;
}
