/**
 * libmpd.c: MPD interaction code.
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
 * mpd_connection_t:
 *
 * Contains the very basic stuff needed to pass an mpd connection around.
 * After doing anything, check to see if an error has occurred. If so, 
 * error will be set to 1, and there will be an explanatory message in
 * error_str[].
 *
 * TODO: Perhaps set error to different numbers, depending on what has
 * happened? Maybe use an enum to make it easier to manipulate.
 */
typedef struct {
	int sockd;
	struct timeval timeout;
	enum connection_status status;
	int error_count;
#ifdef MPD_VERSION_CHECK
	int version[3];
#endif
} mpd_connection;

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
 * mpd_connect()
 *
 * Connects to mpd. Returns a pointer to a calloc'ed mpd_connection_t struct
 * for use with other functions. Check to see if the connection worked by
 * checking mpd_connection_t->error.
 *
 * FIXME: It only works with IPv4 addresses at the moment.
 */
mpd_connection *mpd_connect(const char *hostname, int port, int timeout,
		error_t **error);

/**
 * mpd_send_password()
 *
 * Send a password to the server.
 */
void mpd_send_password(mpd_connection *mpd_conn, const char *_password, 
		error_t **error);

/**
 * mpd_disconnect():
 *
 * Disconnects from mpd, and closes the socket. Also frees the mpd_conn
 * struct allocated by mpd_connect().
 */
void mpd_disconnect(mpd_connection *mpd_conn);

/**
 * mpd_command():
 *
 * Sends the specified command to mpd, and returns the number of bytes sent.
 * (WHY?) Raises an mpd_conn->error if not all of the command was sent.
 *
 * Now a static function - nothing outside this library should be using it
 * anyway. It's also now named send_command()
int mpd_command ( mpd_connection *mpd_conn, char command[] );
 */

int mpd_server_response(mpd_connection *mpd_conn, const char *end, 
		buffer_t *buffer, error_t **error);

int mpd_send_command(mpd_connection *mpd_conn, const char *command, 
		error_t **error);

void mpd_check_server(mpd_connection *mpd_conn, error_t **error);
