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

typedef struct _queue_node {
	gboolean finished_playing;
	gchar *album;
	gchar *artist;
	gchar *title;
	glong date;
	guint length;
	gchar *track;
	struct _queue_node *next;
} queue_node;

struct {
	queue_node *first;
	queue_node *last;
	gint length;
} queue;

void queue_add(const gchar *artist, const gchar *title, const gchar *album,
	guint length, const gchar *track, glong date);
void queue_add_current_song(void);
void queue_load(void);
void queue_remove_songs(queue_node *song, queue_node *keep_ptr);
gboolean queue_save(gpointer data);

#endif /* HAVE_QUEUE_H */
