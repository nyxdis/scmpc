/**
 * mpd.h: Song retrieval and processing.
 *
 * ==================================================================
 * Copyright (c) 2005-2006 Jonathan Coome.
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


/**
 * mpd_song_info:
 *
 * A struct containing everything necessary to compare the current song
 * and the song retrieved last time.
 */
typedef struct {
	time_t start_time;
	time_t time_at_pause;
	enum { NEW, SUBMITTED, INVALID } submission_state;
	bool just_detected;
} mpd_song_info;

/**
 * mpd_song:
 *
 * A struct containing information about the current song. If mpd_state is
 * anything other than PLAYING, everything else will be (null).
 */
typedef struct {
	char *artist;
	char *title;
	char *album;
	char *filename;
	long length;
	long current_pos;
	enum { STOPPED, PAUSED, PLAYING, UNKNOWN } mpd_state;
	long crossfade;
} mpd_song;


/**
 * mpd_thread()
 *
 * The gateway to the mpd thread. It starts the thread, sets up the connection
 * to mpd, registers the cleanup handlers, and loops checks for a new song
 * every second. It also makes sure the connection is still valid, and attempts
 * to reconnect should anything bad happen.
 */
void *mpd_thread(void *arg);
