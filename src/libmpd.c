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


#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>

#include "exception.h"
#include "misc.h"
#include "libmpd.h"


/**
 * server_connect()
 *
 * Mostly inspired/taken from the link below. Should work with IPv4 and IPv6
 * addresses transparently. Returns -1 on failure, and the socket descriptor on
 * success.
 * 
 * http://www.ipv6style.jp/en/apps/20030829/index.shtml
 */
static int server_connect(const char *host, int port, struct s_exception *e)
{
	int err, sockd = -1, s_opts, c;
	char *service = NULL;
	struct addrinfo hints, *res, *result;

	if (asprintf(&service, "%d", port) == -1)
		return -1;

	memset(&hints, 0, sizeof(hints)); 
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	err = getaddrinfo(host, service, &hints, &result);
	switch (err) {
		case 0:
			break;
		case EAI_MEMORY:
			freeaddrinfo(result);
			exception_raise(e, OUT_OF_MEMORY);
			return -1;
		case EAI_AGAIN:
		default:
			exception_create_f(e, "Could not get host information: %s",
					gai_strerror(err));
			freeaddrinfo(result);
			return -1;
	}

	/* Attempt to connect using getaddrinfo results */
	for (res = result; res != NULL; res = res->ai_next) {
		sockd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
		if (sockd < 0) {
			/* This one's broken... Try again. */
			continue;
		}

		/* Add non-blocking option without clobbering previous options. */
		s_opts = fcntl(sockd, F_GETFL, 0);
		fcntl(sockd, F_SETFL, s_opts | O_NONBLOCK);

		c = connect(sockd, res->ai_addr, res->ai_addrlen);
		if (c < 0 && errno != EINPROGRESS) {
			close(sockd);
			sockd = -1;
			continue;
		}

		/* Success! */
		break;
	}

	freeaddrinfo(result);
	free(service);

	if (sockd < 0) {
		exception_raise(e, CONNECTION_FAILURE);
		return -1;
	} else {
		return sockd;
	}
}

/**
 * mpd_response()
 *
 * Reads the response from the MPD server, and puts it into the buffer
 * provided. Returns 0 on error, and 1 otherwise.
 */
int mpd_response(mpd_connection *mpd_conn, buffer_t *buffer, 
		struct s_exception *e)
{
	fd_set read_flags;
	char input_buffer[1024], *ack = NULL;
	int ret, bytes_recvd, written;
	bool checked = FALSE;

	/* MPD signals success with OK\n, and failure with ACK <error message>\n.
	 * When connecting initially, it returns OK MPD <version>. This means that
	 * there are four things to check for when looking for the end of the
	 * input: OK or ACK at the start of the string, and OK and ACK following a
	 * newline. The first only need to be checked at the beginning, but the
	 * other two need to be checked after every recv. */
	
	while (strstr(buffer->buffer, "\nOK\n") == NULL &&
			(ack = strstr(buffer->buffer, "\nACK")) == NULL)
	{
		FD_ZERO(&read_flags);
		FD_SET(mpd_conn->sockd, &read_flags);

		ret = select(mpd_conn->sockd+1, &read_flags, NULL, NULL, 
				&(mpd_conn->timeout));
		if (ret == -1) {
			exception_raise(e, CONNECTION_FAILURE);
			return 0;
		} else if (ret == 0) {
			exception_raise(e, CONNECTION_TIMEOUT);
			return 0;
		} else if (FD_ISSET(mpd_conn->sockd, &read_flags)) {
			FD_CLR(mpd_conn->sockd, &read_flags);
			memset(input_buffer, '\0', sizeof(input_buffer));
			
			bytes_recvd = recv(mpd_conn->sockd, input_buffer, 
					sizeof(input_buffer)-1, 0);
			if (bytes_recvd <= 0) {
				exception_raise(e, CONNECTION_FAILURE);
				return 0;
			} else {
				written = buffer_write(input_buffer, 1, bytes_recvd, 
						(void *)buffer);
				if (written < bytes_recvd) {
					exception_raise(e, OUT_OF_MEMORY);
					return 0;
				}
			}
		}

		/* Check the start of the string for ACK or OK. We know the string is
		 * long enough to check if it contains a newline. */
		if (! checked && strchr(buffer->buffer, '\n') != NULL) {
			if (strncmp(buffer->buffer, "OK", 2) == 0) {
				return 1;
			} else if (strncmp(buffer->buffer, "ACK", 3) == 0) {
				exception_create(e, buffer->buffer + 4);
				return 0;
			} else {
				checked = TRUE;
			}
		}
	}
	if (ack != NULL) {
		exception_create(e, ack+5);
		return 0;
	}

	return 1;
}

/**
 * mpd_send_command()
 *
 * Sends a command to the MPD server described by mpd_conn. Returns 0 on
 * failure, and 1 on success.
 */
int mpd_send_command(mpd_connection *mpd_conn, const char *command, 
		struct s_exception *e)
{
	fd_set write_flags;
	int ret, bytes_sent;
	const char *command_ptr;
	int command_len;

	command_ptr = command;
	command_len = strlen(command)*sizeof(char);

	while (command_len > 0)
	{
		FD_ZERO(&write_flags);
		FD_SET(mpd_conn->sockd, &write_flags);

		ret = select(mpd_conn->sockd+1, NULL, &write_flags, NULL, 
				&(mpd_conn->timeout));
		if (ret == -1) {
			exception_raise(e, CONNECTION_FAILURE);
			return 0;
		} else if (ret == 0) {
			exception_raise(e, CONNECTION_TIMEOUT);
			return 0;
		} else if (FD_ISSET(mpd_conn->sockd, &write_flags)) {
			FD_CLR(mpd_conn->sockd, &write_flags);
			
			bytes_sent = send(mpd_conn->sockd, command_ptr, command_len, 0);
			if (bytes_sent < 0) {
				exception_raise(e, CONNECTION_FAILURE);
				return 0;
			} else {
				command_ptr += bytes_sent;
				command_len -= bytes_sent;
			}
		}
	}
	return 1;
}


size_t mpd_escape(char **escaped, const char *string)
{
	const char *src = string;
	char *dest;

	if (src == NULL || *src == '\0') {
		*escaped = NULL;
		return 0;
	}

	/* Allocate double the length of the original string. (Worst case
	 * scenario - everything has to be escaped). */
	if ((*escaped = malloc((strlen(src)+1)*2)) == NULL) {
		*escaped = NULL;
		return 0;
	}

	dest = *escaped;

	while (*src != '\0') {
		/* If the next character of src is " or \, escape it with a \
		 * before-hand in dest. */
		if (*src == '"' || *src == '\\') {
			*dest = '\\';
			dest++;
		}
		*dest = *src;
		*dest++; *src++;
	}
	*dest = '\0';

	return (size_t) (dest - *escaped);
}

int mpd_send_password(mpd_connection *mpd_conn, const char *password,
		struct s_exception *e)
{
	int retval = 0;
	size_t len;
	char *esc_pass = NULL, *command = NULL;
	buffer_t *buffer;
	struct s_exception ce = EXCEPTION_INIT;

	if ((buffer = buffer_alloc()) == NULL) {
		exception_raise(e, OUT_OF_MEMORY);
		return 0;
	}

	len = mpd_escape(&esc_pass, password);
	if (len == 0)
		goto mpd_send_password_error;

	/* 13 = strlen("password \"\"\n")+1; */
	len += 13;
	if ((command = malloc(len)) == NULL) {
		exception_raise(e, OUT_OF_MEMORY);
		goto mpd_send_password_error;
	}
	strlcpy(command, "password \"", len);
	strlcat(command, esc_pass, len);
	if (strlcat(command, "\"\n", len) >= len) {
		exception_create(e, "The buffer is too small to hold the password. "
				"This means that mpd_escape() has returned the wrong number "
				"of characters. This is a bug - please report it.");
		goto mpd_send_password_error;
	}

	mpd_send_command(mpd_conn, command, &ce);
	if (ce.code != 0) {
		exception_reraise(e, ce);
		goto mpd_send_password_error;
	}

	mpd_response(mpd_conn, buffer, &ce);
	switch (ce.code) {
		case 0:
			retval = 1;
			break;
		case USER_DEFINED:
			if (strstr(ce.msg, "incorrect password") != NULL) {
				exception_raise(e, INVALID_PASSWORD);
				exception_clear(ce);
				mpd_conn->status = BADUSER;
			} else {
				exception_reraise(e, ce);
			}
			break;
		default:
			exception_reraise(e, ce);
			break;
	}

mpd_send_password_error:
	buffer_free(buffer);
	free(esc_pass);
	free(command);
	return retval;
}


mpd_connection *mpd_connection_init(void)
{
	mpd_connection *mpd_conn;

	mpd_conn = calloc(sizeof(mpd_connection), 1);
	if (mpd_conn == NULL) {
		return NULL;
	}

	mpd_conn->status = DISCONNECTED;
	mpd_conn->error_count = 0;

	return mpd_conn;
}

void mpd_connection_cleanup(mpd_connection *mpd_conn)
{
	free(mpd_conn);
}

int mpd_connect(mpd_connection *mpd_conn, const char *hostname, int port, 
		int timeout, struct s_exception *e)
{
	buffer_t *buffer = NULL;
	struct s_exception ce = EXCEPTION_INIT;

	mpd_conn->timeout.tv_sec = timeout;
	mpd_conn->timeout.tv_usec = 0;

	mpd_conn->sockd = server_connect(hostname, port, &ce);
	if (ce.code != 0) {
		exception_reraise(e, ce);
		return 0;
	}
	
	buffer = buffer_alloc();
	if (buffer == NULL) {
		exception_raise(e, OUT_OF_MEMORY);
		return 0;
	}
	
	/* When you connect, the MPD server immediately sends back 
	 * "OK MPD <version>" */
	mpd_response(mpd_conn, buffer, &ce);
	switch (ce.code) {
		case 0:
			mpd_conn->status = CONNECTED;
#ifdef MPD_VERSION_CHECK
			conn->version[0] = conn->version[1] = conn->version[2] = 0;
			sscanf(buffer->buffer, "%*s %*s %d.%d.%d", &(conn->version)[0],
					&(conn->version)[1], &(conn->version)[2]);
#endif
			buffer_free(buffer);
			mpd_conn->error_count = 0;
			return 1;
		case USER_DEFINED:
			exception_create_f(e, "Error in connecting to the MPD server: "
						"%s", buffer->buffer);
			break;
		case CONNECTION_TIMEOUT:
			exception_create(e, "Connection timed out while waiting for a "
					"valid response. This may mean that the specified "
					"hostname and port don't point to an MPD server. scmpc "
					"will not attempt to reconnect to it.");
			mpd_conn->status = BADUSER;
			break;
		default:
			exception_reraise(e, ce);
			break;
	}

	buffer_free(buffer);
	mpd_conn->error_count++;
	return 0;
}

void mpd_disconnect(mpd_connection *mpd_conn)
{
	struct s_exception e = EXCEPTION_INIT;

	assert(mpd_conn != NULL);

	mpd_send_command(mpd_conn, "close\n", &e);
	exception_clear(e);

	shutdown(mpd_conn->sockd, SHUT_RDWR);
	close(mpd_conn->sockd);
}

int mpd_check_server(mpd_connection *mpd_conn, struct s_exception *e)
{
	buffer_t *buffer;
	struct s_exception ce = EXCEPTION_INIT;
	
	buffer = buffer_alloc();
	if (buffer == NULL) {
		exception_raise(e, OUT_OF_MEMORY);
		return 0;
	}
	
	mpd_send_command(mpd_conn, "status\n", &ce);
	if (ce.code != 0) {
		exception_reraise(e, ce);
		goto mpd_check_server_exit;
	}
	mpd_response(mpd_conn, buffer, &ce);
	switch (ce.code) {
		case 0:
			break;
		case USER_DEFINED:
			if (strstr(ce.msg, "you don't have permission") != NULL) {
				exception_raise(e, INVALID_PASSWORD);
				exception_clear(ce);
				mpd_conn->status = BADUSER;
			} else {
				exception_reraise(e, ce);
			}
			break;
		default:
			exception_reraise(e, ce);
			break;
	}

mpd_check_server_exit:
	buffer_free(buffer);
	return (e->code == 0);
}


