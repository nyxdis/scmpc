/**
 * queue.h: Song queue handling.
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


struct queue_node {
	struct queue_node *next;
	char *artist;
	char *title;
	char *album;
	unsigned int length;
	unsigned short track;
	time_t date;
};

void queue_add(const char *artist, const char *title, const char *album,
	unsigned int length, unsigned short track);
void queue_load(void);
void queue_remove_songs(struct queue_node *song, struct queue_node *keep_ptr);
void queue_save(void);
