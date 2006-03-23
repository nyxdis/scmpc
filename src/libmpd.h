/**
 * libmpd.h: MPD interaction code.
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
 * mpd_response()
 *
 * Reads the response from the MPD server, and puts it into the buffer
 * provided. Returns 0 on error, and 1 otherwise.
 */
int mpd_response(mpd_connection *mpd_conn, buffer_t *buffer, 
		struct s_exception *e);


/**
 * mpd_send_command()
 *
 * Sends a command to the MPD server described by mpd_conn. Returns 0 on
 * failure, and 1 on success.
 */
int mpd_send_command(mpd_connection *mpd_conn, const char *command, 
		struct s_exception *e);


/**
 * mpd_escape()
 *
 * Escapes any " or \ characters in a string, and puts the new string in the
 * dynamically allocated string *escaped. Returns the size of the escaped
 * string, or 0 if an error occurred.
 */
size_t mpd_escape(char **escaped, const char *string);


/**
 * mpd_send_password()
 *
 * Sends a password to mpd. Returns 0 on error, -1 if MPD doesn't accept the
 * password, and 1 on success.
 */
int mpd_send_password(mpd_connection *mpd_conn, const char *password,
		struct s_exception *e);


/**
 * mpd_connection_init()
 *
 * Allocates the memory for the mpd_conn struct.
 * XXX: Is this really necessary? Why not use a static struct?
 */
mpd_connection *mpd_connection_init(void);


/**
 * mpd_connection_cleanup()
 *
 * Frees the memory allocated by mpd_connection_init().
 */
void mpd_connection_cleanup(mpd_connection *mpd_conn);


/**
 * mpd_connect()
 *
 * Connects to an MPD server. Returns 0 on failure, and 1 on success.
 */
int mpd_connect(mpd_connection *mpd_conn, const char *hostname, int port, 
		int timeout, struct s_exception *e);


/**
 * mpd_disconnect()
 *
 * Disconnects from MPD.
 */
void mpd_disconnect(mpd_connection *mpd_conn);


/**
 * mpd_check_server()
 *
 * Checks the connection to the server by sending it a "status\n" command. If
 * it receives no answer it will assume you need a password but haven't set it.
 * Returns 0 on failure, and 1 on success.
 */
int mpd_check_server(mpd_connection *mpd_conn, struct s_exception *e);
