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


#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "misc.h"
#include "queue.h"
#include "preferences.h"

void queue_add(const char *artist, const char *title, const char *album,
	unsigned int length, unsigned short track, time_t date)
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
	if(date == 0)
		new_song->date = time(NULL);
	else
		new_song->date = date;

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
	char *line = NULL, *artist, *album, *title;
	unsigned int length = 0;
	unsigned short track = 0;
	FILE *cache_file;
	time_t date = 0;

	artist = title = album = NULL;
	scmpc_log(DEBUG,"Loading queue.");

	if((cache_file = fopen(prefs.cache_file,"r")) == NULL) {
		if(errno != ENOENT)
			scmpc_log(INFO,"Failed to open cache file for reading: %s",
				strerror(errno));
		return;
	}
	
	while(getline(&line,NULL,cache_file) >= 0) {
		if(strncmp(line,"# BEGIN SONG",12) == 0) {
			free(artist); free(title); free(album);
			artist = title = album = NULL;
			length = track = 0;
		} else if(strncmp(line,"artist: ",8) == 0) {
			free(artist);
			artist = strdup(&line[8]);
		} else if(strncmp(line,"title: ",7) == 0) {
			free(title);
			title = strdup(&line[7]);
		} else if(strncmp(line,"album: ",7) == 0) {
			free(album);
			album = strdup(&line[7]);
		} else if(strncmp(line,"date: ",6) == 0) {
			date = atoi(&line[6]);
		} else if(strncmp(line,"length: ",8) == 0) {
			length = atoi(&line[8]);
		} else if(strncmp(line,"track: ",7) == 0) {
			track = atoi(&line[7]);
		} else if(strncmp(line,"# END SONG",10) == 0) {
			queue_add(artist,title,album,length,track,date);
			free(artist); free(title); free(album);
			artist = title = album = NULL;
		}
		free(line);
		line = NULL;
	}
	free(line);
	free(artist); free(title); free(album);
	fclose(cache_file);
}

void queue_remove_songs(struct queue_node *song, struct queue_node *keep_ptr)
{
	struct queue_node *next_song;

	while(song != NULL && song != keep_ptr) {
		free(song->title);
		free(song->artist);
		free(song->album);
		next_song = song->next;
		free(song);
		song = next_song;
		queue.length--;
	}

	if(queue.length == 0)
		queue.first = queue.last = NULL;
}

void queue_save(void)
{
	FILE *cache_file;
	struct queue_node *current_song;

	current_song = queue.first;

	if((cache_file = fopen(prefs.cache_file,"w")) == NULL) {
		scmpc_log(ERROR,"Failed to open cache file for writing: %s",
			strerror(errno));
		return;
	}

	while(current_song != NULL) {
		fprintf(cache_file,"# BEGIN SONG\n"
			"artist: %s\n"
			"title: %s\n"
			"album: %s\n"
			"length: %d\n"
			"track: %d\n"
			"date: %ld\n"
			"# END SONG\n\n",current_song->artist,
			current_song->title,current_song->album,
			current_song->length,current_song->track,
			(long)current_song->date);
		current_song = current_song->next;
	}
	fclose(cache_file);
	scmpc_log(DEBUG,"Cache saved.");
}
