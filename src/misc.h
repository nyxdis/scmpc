/**
 * misc.h: Misc helper functions
 *
 * ==================================================================
 * Copyright (c) 2009-2011 Christoph Mende <mende.christoph@gmail.com>
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


#ifndef HAVE_MISC_H
#define HAVE_MISC_H

#include <glib.h>

/**
 * Possible Last.fm connection statuses
 */
typedef enum {
	DISCONNECTED,
	CONNECTED,
	BADAUTH
} connection_status;

/**
 * Open the log file for writing
 */
void open_log(const gchar *filename);

/**
 * GLib logging handler, should not be called directly
 */
void scmpc_log(const gchar *log_domain, GLogLevelFlags log_level,
		const gchar *message, gpointer user_data);

/**
 * Return the current UNIX timestamp
 */
gint64 get_time(void);

/**
 * Get the seconds that elapsed since the passed timestamp
 */
gint64 elapsed(gint64 since);

/**
 * Write to the cURL data buffer, do not use directly
 */
gsize buffer_write(void *input, gsize size, gsize nmemb, void *buf);

#endif /* HAVE_MISC_H */
