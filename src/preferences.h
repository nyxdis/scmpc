/**
 * preferences.h: Preferences parsing
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

struct {
	gchar *mpd_hostname;
	gint mpd_port;
	gint mpd_interval;
	gint mpd_timeout;
	gchar *mpd_password;
	gboolean fork;
	loglevel log_level;
	gchar *config_file;
	gchar *log_file;
	gchar *pid_file;
	gchar *as_username;
	gchar *as_password;
	gchar *as_password_hash;
	gchar *cache_file;
	gint queue_length;
	gint cache_interval;
} prefs;

gint init_preferences(gint argc, gchar *argv[]);
void clear_preferences(void);
