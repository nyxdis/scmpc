/**
 * mpd.h: MPD backend
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


#include <glib.h>

struct mpd_song {
	gchar *artist;
	gchar *album;
	gchar *title;
	gchar *filename;
	guint length;
	guint current_post;
	gushort track;
	enum { STOPPED, PAUSED, PLAYING, UNKNOWN } mpd_state;
	enum { NEW, INVALID, CHECK, SUBMITTED } song_state;
	gshort crossfade;
	time_t date;
} current_song;

struct mpd_info {
	gint sockfd;
	guint8 version[3];
	enum connection_status status;
} *mpd_info;

void mpd_parse(gchar *buf);
gint mpd_connect(void);
void mpd_cleanup(void);
gint mpd_write(gconstpointer string);
