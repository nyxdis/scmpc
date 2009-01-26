/**
 * queue.c: Song queue handling.
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
#include <time.h>

#include "misc.h"
#include "queue.h"
#include "preferences.h"

static struct queue_t {
	struct queue_node *first;
	struct queue_node *last;
	int length;
} queue;

void queue_add(const char *artist, const char *title, const char *album,
	unsigned int length, unsigned short track)
{
	struct queue_node *new_song;

	if(artist == NULL || title == NULL || length < 30) {
		scmpc_log(DEBUG,"Invalid song passed to queue_add(). Rejecting.");
		return;
	}

	if((new_song = malloc(sizeof(struct queue_node))) == NULL) return;

	new_song->title = strdup(title);
	new_song->artist = strdup(artist);
	if(album != NULL)
		new_song->album = strdup(album);
	else
		new_song->album = strdup("");
	new_song->length = length;
	new_song->track = track;
	new_song->next = NULL;
	new_song->date = time(NULL);

	/* Queue is empty */
	if(queue.first == NULL) {
		queue.first = queue.last = new_song;
		queue.length = 1;
		scmpc_log(DEBUG,"Song added to queue. Queue length: 1");
		return;
	}

	/* Queue is full, remove the first item and add the new one */
	if(queue.length == prefs.queue_length) {
		struct queue_node *new_first_song = queue.first->next;
		if(new_first_song == NULL) {
			scmpc_log(DEBUG,"Queue is too long, but there is only "
				"one accessible song in the list. New song not added.");
			return;
		}
		queue_remove_songs(queue.first, new_first_song);
		queue.first = new_first_song;
		scmpc_log(INFO,"The queue of songs to be submitted is too long."
				"The oldest song has been removed.");
	}
	queue.last->next = new_song;
	queue.last = new_song;
	queue.length++;
	scmpc_log(DEBUG,"Song added to queue. Queue length: %d",queue.length);
}

void queue_load(void)
{
}

void queue_remove_songs(struct queue_node *song, struct queue_node *keep_ptr)
{
	printf("%p %p\n",song,keep_ptr);
}

void queue_save(void)
{
	printf("%p\n",&queue);
}
