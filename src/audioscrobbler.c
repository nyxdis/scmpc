/**
 * audioscrobbler.c: Audioscrobbler backend.
 *
 * Copyright (c) 2008 Christoph Mende <angelos@unkreativ.org>
 * All rights reserved. Released under the 2-clause BSD license.
 *
 * Based on Jonathan Coome's work on scmpc
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

#include "misc.h"
#include "preferences.h"
#include "audioscrobbler.h"
#include "config.h"

char curl_error_buffer[CURL_ERROR_SIZE];

#define HANDSHAKE_URL \
	"http://post.audioscrobbler.com/?hs=true&p=1.2.1&c=spc&v=%s&u=%s&t=%u&a=%s"

void as_connection_init(void)
{
	as_conn = calloc(sizeof(struct as_connection),1);
	if(as_conn == NULL) return;

	as_conn->submit_url = NULL;
	as_conn->np_url = NULL;
	as_conn->last_handshake = 0;
	as_conn->status = DISCONNECTED;
	as_conn->handle = curl_easy_init();
	if(as_conn->handle == NULL) return;
	as_conn->headers = curl_slist_append(as_conn->headers, 
			"User-Agent: scmpc/" PACKAGE_VERSION);
	
	curl_easy_setopt(as_conn->handle,CURLOPT_HTTPHEADER,as_conn->headers);
	curl_easy_setopt(as_conn->handle,CURLOPT_WRITEFUNCTION,&buffer_write);
	curl_easy_setopt(as_conn->handle,CURLOPT_ERRORBUFFER,curl_error_buffer);
	curl_easy_setopt(as_conn->handle,CURLOPT_NOSIGNAL,1L);
	curl_easy_setopt(as_conn->handle,CURLOPT_CONNECTTIMEOUT,5L);
}

void as_connection_cleanup(struct as_connection *as_conn)
{
	curl_slist_free_all(as_conn->headers);
	curl_easy_cleanup(as_conn->handle);
	free(as_conn->submit_url);
	free(as_conn->np_url);
	free(as_conn);
}

void as_handshake(void)
{
	char *auth_token, *buffer, *handshake_url, *line, *saveptr, *tmp;
	time_t timestamp;
	int ret;

	if(prefs.as_username == NULL || prefs.as_password == NULL) {
		scmpc_log(INFO,"No username or password specified. "
				"Not connecting to Audioscrobbler.");
		as_conn->status = BADAUTH;
		return;
	}

	timestamp = time(NULL);

	if(difftime(as_conn->last_handshake,timestamp) < 1800) {
		scmpc_log(DEBUG,"Requested handshake, but last handshake was less than 30 "
				"minutes ago.");
		return;
	}

	if(asprintf(&tmp,"%s%u",md5_hash(prefs.as_password),timestamp) < 0) return;
	auth_token = md5_hash(tmp);
	free(tmp);
	if(asprintf(&handshake_url,HANDSHAKE_URL,PACKAGE_VERSION,prefs.as_username,
		timestamp,auth_token) < 0) return;

	scmpc_log(DEBUG,"handshake_url = %s",handshake_url);

	buffer = malloc(1024);
	if(buffer == NULL) return;

	curl_easy_setopt(as_conn->handle,CURLOPT_WRITEDATA,(void *)buffer);
	curl_easy_setopt(as_conn->handle,CURLOPT_HTTPGET,1);
	curl_easy_setopt(as_conn->handle,CURLOPT_URL,handshake_url);

	ret = curl_easy_perform(as_conn->handle);
	free(handshake_url);

	if(ret != 0) {
		scmpc_log(ERROR,"Could not connect to the Audioscrobbler"
			" server: %s",curl_easy_strerror(ret));
		free(buffer);
	}

	line = strtok_r(buffer,"\n",&saveptr);
	if(line == NULL) {
		scmpc_log(DEBUG,"Could not parse Audioscrobbler handshake response.");
		free(buffer);
	}

	if(strncmp(line,"OK",2) == 0) {
		unsigned short int line_no = 1;

		while((line = strtok_r(NULL,"\n",&saveptr)) != NULL) {
			line_no++;
			if(line_no == 2) {
				as_conn->session_id = strdup(line);
			} else if(line_no == 3) {
				free(as_conn->np_url);
				as_conn->np_url = strdup(line);
			} else if(line_no == 4) {
				free(as_conn->submit_url);
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
			free(buffer);
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
}

int as_now_playing(void)
{
	return 0;
}

int as_submit(void)
{
	return 0;
}

void queue_save(void)
{
}
