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


struct mpd_song {
	char *artist;
	char *album;
	char *title;
	char *filename;
	unsigned int length;
	unsigned int current_post;
	unsigned short track;
	enum { STOPPED, PAUSED, PLAYING, UNKNOWN } mpd_state;
	short crossfade;
	time_t date;
} current_song;

struct mpd_info {
	int sockfd;
	int version[3];
	enum connection_status status;
} *mpd_info;

void mpd_parse(char *buf);
int mpd_connect(void);
void mpd_cleanup(void);
void mpd_write(const char *string);
