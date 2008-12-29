/**
 * audioscrobbler.h: Audioscrobbler backend.
 *
 * Copyright (c) 2008 Christoph Mende <angelos@unkreativ.org>
 * All rights reserved. Released under the 2-clause BSD license.
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
	CURL *handle;
	struct curl_slist *headers;
	char *submit_url;
	char password[33];
	unsigned int interval;
	int error_count;
	enum connection_status status;
	time_t last_handshake;
};

void queue_add(const char *artist, const char *title, const char *album,
		int length, const char *date);

int as_connection_init(void);

void queue_save(void);
