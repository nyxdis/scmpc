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
#include <sys/select.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <math.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "misc.h"
#include "liberror.h"
#include "libmpd.h"


/*
 * Private functions.
 */

static struct hostent *resolve(const char *hostname)
{
	short int i;
	extern int h_errno;
	struct hostent *he;

	/* Try to resolve the hostname three times. */
	for (i = 0; i < 3; i++)
	{
		he = gethostbyname(hostname);
		if (he == NULL) {
			if (h_errno == TRY_AGAIN && i < 3) {
				sleep (1);
			} else {
				return NULL;
			}
		} else {
			break;
		}
	}
	return he;
}

static int sock_open(void)
{
	int sockd, x;
	
	sockd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockd < 0)
		return 0;

	/* Add non-blocking option without clobbering previous options. */
	x = fcntl(sockd, F_GETFL, 0);
	fcntl(sockd, F_SETFL, x | O_NONBLOCK);

	return sockd;
}

static int server_connect(struct hostent *he, int port, int sockd)
{
	struct sockaddr_in serv_addr;
	int c;
	
	/* FIXME: Doesn't work with IPv6 */
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port   = htons(port);
	memcpy(&serv_addr.sin_addr.s_addr, he->h_addr_list[0], 
	         sizeof(serv_addr.sin_addr.s_addr));
	
	c = connect(sockd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
	if (c < 0 && errno != EINPROGRESS)
		return 0;

	return 1;
}


/*
 * Public Functions
 */

int mpd_server_response(mpd_connection *mpd_conn, const char *end, 
		buffer_t *buffer, error_t **error)
{
#define INPUT_BUFLEN 1024
	fd_set read_flags;
	char input_buffer[INPUT_BUFLEN];
	int ret, bytes_recvd, written;

	/* Read until we get the response we're looking for. */
	while (strstr(buffer->buffer, end) == NULL)
	{
		FD_ZERO(&read_flags);
		FD_SET(mpd_conn->sockd, &read_flags);

		ret = select(mpd_conn->sockd+1, &read_flags, NULL, NULL, 
				&(mpd_conn->timeout));
		if (ret == -1) {
			*error = error_set(-1, "select() failed.", NULL);
			mpd_conn->status = DISCONNECTED;
			return 0;
		} else if (ret == 0) {
			*error = error_set(-2, "Connection timed out.", NULL);
			mpd_conn->error_count++;
			return 0;
		} else if (FD_ISSET(mpd_conn->sockd, &read_flags)) {
			FD_CLR(mpd_conn->sockd, &read_flags);
			memset(input_buffer, '\0', INPUT_BUFLEN);
			
			bytes_recvd = recv(mpd_conn->sockd,input_buffer,INPUT_BUFLEN-1,0);
			if (bytes_recvd <= 0) {
				*error = error_set(-1, "Connection failed.", NULL);
				mpd_conn->status = DISCONNECTED;
				return 0;
			} else {
				written = buffer_write(input_buffer, 1, bytes_recvd, 
						(void *)buffer);
				if (written < bytes_recvd) {
					*error = ERROR_OUT_OF_MEMORY;
					return 0;
				}
				mpd_conn->error_count = 0;
			}
		}
	}
	return 1;
}

int mpd_send_command(mpd_connection *mpd_conn, const char *command, 
		error_t **error)
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
			*error = error_set(-1, "select() failed.", NULL);
			mpd_conn->status = DISCONNECTED;
			return 0;
		} else if (ret == 0) {
			*error = error_set(0, "Connection timed out.", NULL);
			mpd_conn->error_count++;
			return 0;
		} else if (FD_ISSET(mpd_conn->sockd, &write_flags)) {
			FD_CLR(mpd_conn->sockd, &write_flags);
			
			bytes_sent = send(mpd_conn->sockd, command_ptr, command_len, 0);
			if (bytes_sent < 0) {
				error_set(-1, "Connection failed", NULL);
				mpd_conn->status = DISCONNECTED;
				return 0;
			} else {
				command_ptr += bytes_sent;
				command_len -= bytes_sent;
				mpd_conn->error_count = 0;
			}
		}
	}
	return 1;
}

void mpd_send_password(mpd_connection *mpd_conn, const char *_password, 
		error_t **error)
{
	char *password, *cmd, *p;
	const char *_p;
	size_t cmd_size = 0;
	buffer_t *buffer;
	error_t *child_error = NULL;

	buffer = buffer_alloc();
	if (buffer == NULL) {
		*error = ERROR_OUT_OF_MEMORY;
		return;
	}
	
	if (_password == NULL || *_password == '\0')
		return;
	
	password = calloc(2 * strlen(_password) + 1, 1);
	if (password == NULL) {
		*error = ERROR_OUT_OF_MEMORY;
		buffer_free(buffer);
		return;
	}
	
	p = password;
	_p = _password;
	
	while (*_p != '\0') {
		if (*p == '"' || *p == '\\') {
			*p = '\\';
			p++;
		}
		*p = *_p;
		p++; _p++;
	}
	
	cmd_size = strlen(password) + strlen("password ") + 4;
	cmd = calloc(cmd_size, 1);
	if (cmd == NULL) {
		*error = ERROR_OUT_OF_MEMORY;
		goto mpd_send_password_exit2;
	}
	snprintf(cmd, cmd_size, "password \"%s\"\n", password);

	mpd_send_command(mpd_conn, cmd, &child_error);
	if (ERROR_IS_SET(child_error)) {
		*error = error_set(-1, "An error occurred while sending the password "
				"to the server.", child_error);
		goto mpd_send_password_exit1;
	}
	mpd_server_response(mpd_conn, "OK\n", buffer, &child_error);
	if (ERROR_IS_SET(child_error)) {
		error_clear(child_error); /* Already set, but irrelevant. */
		error_set(-1, "Invalid password.", NULL);
		mpd_conn->status = BADUSER;
	}
	
mpd_send_password_exit1:
	buffer_free(buffer);
	free(password);
mpd_send_password_exit2:
	free(cmd);
}

mpd_connection *mpd_connect(const char *hostname, int port, int timeout, 
		error_t **error)
{
	mpd_connection *conn;
	struct hostent *he;
	buffer_t *buffer;
	error_t *child_error = NULL;
	
	buffer = buffer_alloc();
	if (buffer == NULL) {
		*error = ERROR_OUT_OF_MEMORY;
		return NULL;
	}
	
	conn = calloc(sizeof(mpd_connection), 1);
	if (conn == NULL) {
		buffer_free(buffer);
		*error = ERROR_OUT_OF_MEMORY;
		return NULL;
	}
	
	conn->status = DISCONNECTED;
	conn->sockd = sock_open();
	if (conn->sockd == 0) {
		*error = error_set(-1, "Could not allocate socket.", NULL);
		goto mpd_connect_error;
	}

	he = resolve(hostname);
	if (he == NULL) {
		*error = error_set(-2, "Could not resolve hostname.", NULL);
		goto mpd_connect_error;
	}

	if (! server_connect(he, port, conn->sockd)) {
		*error = error_set(-1, "Could not connect to host.", NULL);
		goto mpd_connect_error;
	}

	conn->timeout.tv_sec = timeout;
	conn->timeout.tv_usec = 0;

	mpd_server_response(conn, "\n", buffer, &child_error);
	if (ERROR_IS_SET(child_error)) {
		if (child_error == ERROR_OUT_OF_MEMORY) {
			error_clear(child_error);
			*error = ERROR_OUT_OF_MEMORY;
			goto mpd_connect_error;
		} else {
			*error = error_set(-1, "Error while waiting for a response from "
					"the mpd server.", child_error);
			goto mpd_connect_error;
		}
	}

	if (strcmp(buffer->buffer, "ACK") == 0) {
		child_error = error_set(-1, buffer->buffer, NULL);
		*error = error_set(-1, "Error in communicating with mpd server.", 
				child_error);
		goto mpd_connect_error;
	} else if (strncmp(buffer->buffer, "OK MPD", 6) != 0) { 
		*error = error_set(-1, "Unexpected response from server.", NULL);
		goto mpd_connect_error;
	}
#ifdef MPD_VERSION_CHECK
	conn->version[0] = conn->version[1] = conn->version[2] = 0;
	sscanf(buffer->buffer, "%*s %*s %d.%d.%d", &(conn->version)[0],
			&(conn->version)[1], &(conn->version)[2]);
#endif
	conn->status = CONNECTED;

	buffer_free(buffer);
	return conn;
mpd_connect_error:
	buffer_free(buffer);
	free(conn);
	return NULL;
}

void mpd_check_server(mpd_connection *mpd_conn, error_t **error)
{
	error_t *child_error = NULL;
	buffer_t *buffer;
	
	buffer = buffer_alloc();
	if (buffer == NULL) {
		*error = ERROR_OUT_OF_MEMORY;
		return;
	}
	
	mpd_send_command(mpd_conn, "status\n", &child_error);
	if (ERROR_IS_SET(child_error)) {
		*error = error_set(-1, "Error sending command to mpd_server.", 
				child_error);
		goto mpd_check_server_exit;
	}
	mpd_server_response(mpd_conn, "OK\n", buffer, &child_error);
	if (ERROR_IS_SET(child_error)) {
		if (child_error->code == -2) {
			error_clear(child_error);
			*error = error_set(-2, "You do not have read access to the mpd "
					"server. Please correct your password and restart this "
					"program.", NULL);
		}
		else {
			*error = error_set(-1, "An error occurred when attempting to "
					"contact the mpd server.", child_error);
		}
	}
mpd_check_server_exit:
	buffer_free(buffer);
}

void mpd_disconnect(mpd_connection *mpd_conn)
{
	if (mpd_conn == NULL)
		return;
	
	shutdown(mpd_conn->sockd, SHUT_RDWR);
	close(mpd_conn->sockd);
	free(mpd_conn);
}
