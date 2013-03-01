/**
 * queue.c: Song queue handling.
 *
 * ==================================================================
 * Copyright (c) 2009-2012 Christoph Mende <mende.christoph@gmail.com>
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

#include <mpd/client.h>

#include "queue.h"
#include "preferences.h"
#include "scmpc.h"
#include "mpd.h"

static void queue_add(const gchar *artist, const gchar *title,
	const gchar *album, guint length, gint track, gint64 date);
static void write_element(gpointer data, G_GNUC_UNUSED gpointer user_data);

/**
 * Internal song queue
 */
static GQueue *queue;

void queue_init(void)
{
	queue = g_queue_new();
}

void queue_cleanup(void)
{
	g_queue_foreach(queue, queue_free_song, NULL);
	g_queue_free(queue);
}

/**
 * Add a song to the queue
 */
static void queue_add(const gchar *artist, const gchar *title,
	const gchar *album, guint length, gint track, gint64 date)
{
	queue_node *new_song;

	if (!artist || !title || length < 30) {
		g_debug("Invalid song passed to queue_add(). Rejecting.");
		return;
	}

	new_song = g_malloc(sizeof (queue_node));

	new_song->title = g_strdup(title);
	new_song->artist = g_strdup(artist);
	if (album)
		new_song->album = g_strdup(album);
	else
		new_song->album = g_strdup("");
	new_song->length = length;
	new_song->track = track;
	new_song->date = date;

	/* Queue is full, remove the first item and add the new one */
	if (g_queue_get_length(queue) >= prefs.queue_length) {
		queue_node *song = g_queue_pop_head(queue);
		queue_free_song(song, NULL);
		g_message("The queue of songs to be submitted is too long. "
				"The oldest song has been removed.");
	}

	g_queue_push_tail(queue, new_song);
	g_debug("Song added to queue. Queue length: %d", g_queue_get_length(queue));
}

void queue_add_current_song(void)
{
	const gchar *trackstr = mpd_song_get_tag(mpd.song, MPD_TAG_TRACK, 0);
	guint track = 0;

	if (trackstr)
		track = strtol(trackstr, NULL, 10);

	queue_add(mpd_song_get_tag(mpd.song, MPD_TAG_ARTIST, 0),
			mpd_song_get_tag(mpd.song, MPD_TAG_TITLE, 0),
			mpd_song_get_tag(mpd.song, MPD_TAG_ALBUM, 0),
			mpd_song_get_duration(mpd.song),
			track, mpd.song_date);
	mpd.song_state = SONG_SUBMITTED;
}

void queue_load(void)
{
	gchar line[256], *artist, *album, *title;
	FILE *cache_file;
	gint64 date = 0;
	guint track = 0, length = 0;

	artist = title = album = NULL;
	g_debug("Loading queue.");

	cache_file = fopen(prefs.cache_file, "r");
	if (!cache_file) {
		if (errno != ENOENT)
			g_message("Failed to open cache file for reading: %s",
				g_strerror(errno));
		return;
	}

	while (fgets(line, sizeof line, cache_file)) {
		*strrchr(line, '\n') = 0;
		if (!strncmp(line, "# BEGIN SONG", 12)) {
			artist = title = album = NULL;
			length = 0;
		} else if (!strncmp(line, "artist: ", 8)) {
			g_free(artist);
			artist = g_strdup(&line[8]);
		} else if (!strncmp(line, "title: ", 7)) {
			g_free(title);
			title = g_strdup(&line[7]);
		} else if (!strncmp(line, "album: ", 7)) {
			g_free(album);
			album = g_strdup(&line[7]);
		} else if (!strncmp(line, "date: ", 6)) {
			date = strtol(&line[6], NULL, 10);
		} else if (!strncmp(line, "length: ", 8)) {
			length = strtol(&line[8], NULL, 10);
		} else if (!strncmp(line, "track: ", 7)) {
			track = strtol(&line[7], NULL, 10);
		} else if (!strncmp(line, "# END SONG", 10)) {
			queue_add(artist, title, album, length, track, date);
			g_free(artist); g_free(title); g_free(album);
			artist = title = album = NULL;
		}
	}
	g_free(artist); g_free(title); g_free(album);
	fclose(cache_file);
}

void queue_free_song(gpointer data, G_GNUC_UNUSED gpointer user_data)
{
	queue_node *song = data;
	g_free(song->album);
	g_free(song->artist);
	g_free(song->title);
	g_free(song);
}

gboolean queue_save(G_GNUC_UNUSED gpointer data)
{
	FILE *cache_file = fopen(prefs.cache_file, "w");

	if (!cache_file) {
		g_warning("Failed to open cache file for writing: %s",
			g_strerror(errno));
		return FALSE;
	}

	g_queue_foreach(queue, write_element, cache_file);

	fclose(cache_file);
	g_debug("Cache saved.");
	return TRUE;
}

/**
 * Write a song to the cache file
 */
static void write_element(gpointer data, gpointer user_data)
{
	FILE *cache_file = user_data;
	queue_node *song = data;

	fprintf(cache_file, "# BEGIN SONG\n"
		"artist: %s\n"
		"title: %s\n"
		"album: %s\n"
		"length: %d\n"
		"track: %d\n"
		"date: %" G_GINT64_FORMAT "\n"
		"# END SONG\n\n", song->artist, song->title, song->album,
		song->length, song->track, song->date);
}

void queue_clear_n(guint num)
{
	if (num > g_queue_get_length(queue))
		num = g_queue_get_length(queue);

	for (guint i = 0; i < num; i++) {
		queue_node *song = g_queue_pop_head(queue);
		queue_free_song(song, NULL);
	}
}

guint queue_get_length(void)
{
	return g_queue_get_length(queue);
}

queue_node* queue_peek_head(void)
{
	return g_queue_peek_head(queue);
}

queue_node* queue_peek_nth(guint n)
{
	return g_queue_peek_nth(queue, n);
}
