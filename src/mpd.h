/**
 * mpd.c: MPD back end.
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


#include <glib.h>

struct {
	struct mpd_connection *conn;
	struct mpd_status *status;
	struct mpd_song *song;
	GTimer *song_pos;
	gint song_date;
	gboolean song_submitted;
} mpd;

gboolean mpd_connect(void);
void mpd_update(void);
