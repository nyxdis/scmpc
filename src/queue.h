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

typedef struct {
	gchar *album;
	gchar *artist;
	gchar *title;
	glong date;
	guint length;
	guint track;
} queue_node;

extern GQueue *queue;

void queue_add_current_song(void);
void queue_cleanup(void);
void queue_clear_n(guint num);
void queue_init(void);
void queue_free_song(gpointer song, G_GNUC_UNUSED gpointer user_data);
void queue_load(void);
gboolean queue_save(gpointer data);

#endif /* HAVE_QUEUE_H */
