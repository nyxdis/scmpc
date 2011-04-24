/**
 * mpd.c: MPD backend.
 *
 * ==================================================================
 * Copyright (c) 2009-2011 Christoph Mende <angelos@gentoo.org>
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


#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/un.h>

#include "misc.h"
#include "audioscrobbler.h"
#include "scmpc.h"
#include "mpd.h"
#include "preferences.h"
#include "queue.h"

static gint server_connect_unix(const gchar *path)
{
	gint sockfd;
	struct sockaddr_un addr;
	guint len;

	if ((sockfd = socket(AF_UNIX, SOCK_STREAM, 0))  < 0)
		return -1;

	addr.sun_family = AF_UNIX;
	g_strlcpy(addr.sun_path, path, strlen(path)+1);
	len = strlen(addr.sun_path) + sizeof(addr.sun_family);

	if (fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFL, 0) | O_NONBLOCK) < 0)
		return -1;

	if (connect(sockfd, (struct sockaddr *)&addr, len) < 0)
		return -1;

	return sockfd;
}

static gint server_connect_tcp(const gchar *host, gint port)
{
	fd_set write_flags;
	gint sockfd, valopt, ret;
	gchar service[5];
	socklen_t len;
	struct addrinfo hints, *result, *rp;
	struct timeval timeout;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	sprintf(service, "%d", port);

	if ((ret = getaddrinfo(host, service, &hints, &result))) {
		scmpc_log(ERROR, "getaddrinfo: %s", gai_strerror(ret));
		freeaddrinfo(result);
		return -1;
	}
	if (!result)
		return -1;

	for (rp = result;rp;rp = rp->ai_next) {
		if ((sockfd = socket(rp->ai_family, rp->ai_socktype,
			rp->ai_protocol)) >= 0) break;

		close(sockfd);
	}

	if (fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFL, 0) | O_NONBLOCK) < 0) {
		freeaddrinfo(result);
		return -1;
	}

	if (connect(sockfd, rp->ai_addr, rp->ai_addrlen) < 0) {
		if (errno == EINPROGRESS) {
			timeout.tv_sec = prefs.mpd_timeout;
			timeout.tv_usec = 0;

			FD_ZERO(&write_flags);
			FD_SET(sockfd, &write_flags);
			if (select(sockfd+1, NULL, &write_flags, NULL, &timeout) > 0) {
				len = sizeof(int);
				getsockopt(sockfd, SOL_SOCKET, SO_ERROR,
					(void*)(&valopt), &len);
				if (valopt) {
					errno = valopt;
					freeaddrinfo(result);
					return -1;
				}
			}
			else {
				errno = ETIMEDOUT;
				freeaddrinfo(result);
				return -1;
			}
		}
		else {
			freeaddrinfo(result);
			return -1;
		}
	}

	freeaddrinfo(result);
	errno = 0;
	return sockfd;
}

gint mpd_write(const gchar *string)
{
	gchar tmp[256];

	/* exit idle mode before sending commands */
	sprintf(tmp, "noidle\n");

	g_strlcat(tmp, string, sizeof tmp);
	g_strlcat(tmp, "\n", sizeof tmp);

	/* re-enter idle mode */
	g_strlcat(tmp, "idle player\n", sizeof tmp);

	if (write(mpd_info.sockfd, tmp, strlen(tmp)) < 0)
		return -1;
	return 0;
}

gint mpd_connect(void)
{
	gchar *tmp;

	mpd_info.status = DISCONNECTED;
	if (!strncmp(prefs.mpd_hostname, "/", 1))
		mpd_info.sockfd = server_connect_unix(prefs.mpd_hostname);
	else
		mpd_info.sockfd = server_connect_tcp(prefs.mpd_hostname,
			prefs.mpd_port);

	if (mpd_info.sockfd < 0) {
		scmpc_log(ERROR, "Failed to connect to MPD: %s", g_strerror(errno));
		return -1;
	}

	if (strlen(prefs.mpd_password) > 0) {
		tmp = g_strdup_printf("password %s\n", prefs.mpd_password);
		if (write(mpd_info.sockfd, tmp, strlen(tmp)) < 0) {
			g_free(tmp);
			scmpc_log(ERROR, "Failed to write to MPD: %s",
				g_strerror(errno));
			return -1;
		}
		g_free(tmp);
	}
	mpd_info.status = CONNECTED;
	return 0;
}

void mpd_parse(gchar *buf)
{
	gchar *saveptr = NULL, *line;

	line = strtok_r(buf, "\n", &saveptr);
	if (!line)
		return;
	do {
		if (!strncmp(line, "ACK", 3)) {
			if (g_strrstr(line, "incorrect password")) {
				scmpc_log(ERROR, "[MPD] Incorrect password");
				mpd_info.status = BADAUTH;
				mpd_info.sockfd = -1;
			} else if (g_strrstr(line, "you don't have permission")) {
				scmpc_log(ERROR, "[MPD] Server is"
						" password-protected");
				mpd_info.status = BADAUTH;
				mpd_info.sockfd = -1;
			} else {
				/* Unknown error */
				scmpc_log(ERROR, "Received unknown ACK from"
						" MPD: %s", &line[10]);
			}
		} else if (!strncmp(line, "changed: player", 15)) {
			if (write(mpd_info.sockfd, "status\n", 7) < 0)
				return;
		} else if (!strncmp(line, "xfade: ", 7)) {
			current_song.xfade = strtol(&line[7], NULL, 10);
		} else if (!strncmp(line, "state: ", 7)) {
			if (!strncmp(&line[7], "play", 4)) {
				if (current_song.mpd_state == PLAYING || current_song.mpd_state == STOPPED) {
					while ((line = strtok_r(NULL, "\n", &saveptr))) {
						if (!strncmp(line, "time: ", 6) && strtol(strtok(&line[6], ":"), NULL, 10) < current_song.xfade + 5) {
							if (write(mpd_info.sockfd, "currentsong\n", 12) < 0)
								return;
							if (queue.length > 0)
								queue.last->finished_playing = TRUE;
						}
					}
				} else if (current_song.mpd_state == PAUSED && current_song.pos) {
					g_timer_continue(current_song.pos);
				}
				current_song.mpd_state = PLAYING;
			} else if (!strncmp(&line[7], "pause", 5)) {
				if (current_song.mpd_state == PLAYING) {
					g_timer_stop(current_song.pos);
				}
				current_song.mpd_state = PAUSED;
			} else if (!strncmp(&line[7], "stop", 4)) {
				current_song.mpd_state = STOPPED;
			}
			if (write(mpd_info.sockfd, "idle player\n", 12) < 0)
				return;
		} else if (!strncmp(line, "file: ", 6)) {
			GTimeVal tv;
			glong ts;

			g_get_current_time(&tv);
			ts = tv.tv_sec;

			g_free(current_song.artist);
			g_free(current_song.title);
			g_free(current_song.album);
			current_song.artist = current_song.title = current_song.album = NULL;
			current_song.track = 0;
			while ((line = strtok_r(NULL, "\n", &saveptr))) {
				if (!strncmp(line, "Artist: ", 8))
					current_song.artist = g_strdup(&line[8]);
				else if (!strncmp(line, "Album: ", 7))
					current_song.album = g_strdup(&line[7]);
				else if (!strncmp(line, "Title: ", 7))
					current_song.title = g_strdup(&line[7]);
				else if (!strncmp(line, "Time: ", 6))
					current_song.length = strtol(&line[6], NULL, 10);
				else if (!strncmp(line, "Track: ", 7))
					current_song.track = strtol(strtok(&line[7], "/"), NULL, 10);
			}
			current_song.date = ts;
			if (current_song.artist && current_song.title && current_song.length >= 30) {
				current_song.song_state = NEW;
				as_now_playing();
				g_timer_start(current_song.pos);
			} else if (current_song.length < 30) {
				scmpc_log(INFO, "Song is too short.");
				current_song.song_state = INVALID;
			} else {
				scmpc_log(INFO, "File is not tagged properly.");
				current_song.song_state = INVALID;
			}
		} else if (!strncmp(line, "OK MPD", 6)) {
			gushort version[2];
			sscanf(line, "%*s %*s %hu.%hu", &version[0], &version[1]);
			if (!version[0] && version[1] < 14) {
				scmpc_log(ERROR, "MPD too old, please upgrade to 0.14");
				mpd_info.status = BADAUTH;
				close(mpd_info.sockfd);
				continue;
			}
			scmpc_log(INFO, "Connected to MPD.");
			current_song.mpd_state = UNKNOWN;
			if (write(mpd_info.sockfd, "status\n", 7) < 0)
				return;
		}
	} while ((line = strtok_r(NULL, "\n", &saveptr)));
}

void mpd_cleanup(void)
{
	if (mpd_info.status == CONNECTED)
		close(mpd_info.sockfd);
	g_free(current_song.artist);
	g_free(current_song.title);
	g_free(current_song.album);
}
