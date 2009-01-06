/**
 * audioscrobbler.h: Audioscrobbler backend.
 *
 * Copyright (c) 2008 Christoph Mende <angelos@unkreativ.org>
 * All rights reserved. Released under the 2-clause BSD license.
 *
 * Based on Jonathan Coome's work on scmpc
 */


#include <curl/curl.h>

struct queue_node {
	struct queue_node *next;
	char *artist;
	char *title;
	char *album;
	int length;
	time_t date;
};

struct as_connection {
	char *submit_url;
	char *np_url;
	char password[33];
	time_t last_handshake;
	char *session_id;
	enum connection_status status;
	CURL *handle;
	struct curl_slist *headers;
} *as_conn;

void queue_add(const char *artist, const char *title, const char *album,
		int length, const char *date);

void as_connection_init(void);

void queue_save(void);
