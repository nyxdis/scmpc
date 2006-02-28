/**
 * preferences.h: Controlling the programme's behaviour.
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

struct preferences {
	char *mpd_hostname;
	int mpd_port;
	int mpd_timeout;
	char *mpd_password;
	bool fork;
	enum loglevel log_level;
	char *config_file;
	char *log_file;
	char *pid_file;
	char *as_username;
	char *as_password;
	char *cache_file;
	int queue_length;
	int cache_interval;
} prefs;

void init_preferences(int argc, char **argv);
void clear_preferences(void);
