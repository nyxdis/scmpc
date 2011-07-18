/**
 * queue.h: Song queue handling.
 *
 * ==================================================================
 * Copyright (c) 2009-2011 Christoph Mende <angelos@unkreativ.org>
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


#ifndef HAVE_QUEUE_H
#define HAVE_QUEUE_H

#include <glib.h>

/**
 * An element in the song queue
 */
typedef struct {
	gchar *album;
	gchar *artist;
	gchar *title;
	gint64 date;
	guint length;
	guint track;
} queue_node;

/**
 * Add the currently playing song to the queue
 */
void queue_add_current_song(void);

/**
 * Release resources
 */
void queue_cleanup(void);

/**
 * Remove n songs from the queue
 */
void queue_clear_n(guint num);

/**
 * Initialize the queue
 */
void queue_init(void);

/**
 * Free the memory used by a #queue_node
 */
void queue_free_song(gpointer song, G_GNUC_UNUSED gpointer user_data);

/**
 * Load the queue from the cache file
 */
void queue_load(void);

/**
 * Save the queue to the cache file
 */
gboolean queue_save(gpointer data);

/**
 * Get the current queue length
 */
guint queue_get_length(void);

/**
 * Return the first song from the queue, but don't remove it
 */
queue_node* queue_peek_head(void);

/**
 * Return the nth song from the queue, but don't remove it
 */
queue_node* queue_peek_nth(guint n);

#endif /* HAVE_QUEUE_H */
