/**
 * mpd.c: MPD back end.
 *
 * ==================================================================
 * Copyright (c) 2009-2011 Christoph Mende <angelos@unkreativ.org>
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


#include <mpd/client.h>

#include "mpd.h"
#include "preferences.h"
#include "audioscrobbler.h"
#include "queue.h"
#include "scmpc.h"

static void mpd_update(void);
static void mpd_disconnect(void);
static void mpd_schedule_check(void);

gboolean mpd_connect(void)
{
	mpd.conn = mpd_connection_new(prefs.mpd_hostname, prefs.mpd_port,
			prefs.mpd_timeout * 1000);
	if (mpd_connection_get_error(mpd.conn) != MPD_ERROR_SUCCESS) {
		g_warning("Failed to connect to MPD: %s",
				mpd_connection_get_error_message(mpd.conn));
		return FALSE;
	} else if (mpd_connection_cmp_server_version(mpd.conn, 0, 14, 0) < 0) {
		g_critical("MPD too old, please upgrade to 0.14 or newer");
		scmpc_shutdown();
		return FALSE;
	} else {
		mpd_command_list_begin(mpd.conn, TRUE);
		mpd_send_status(mpd.conn);
		mpd_send_current_song(mpd.conn);
		mpd_command_list_end(mpd.conn);

		mpd.status = mpd_recv_status(mpd.conn);
		mpd_response_next(mpd.conn);
		mpd.song = mpd_recv_song(mpd.conn);
		mpd_response_finish(mpd.conn);

		g_message("Connected to MPD");

		// only send now playing, don't queue the song
		if (mpd_status_get_state(mpd.status) == MPD_STATE_PLAY)
			as_now_playing();
		mpd.song_submitted = TRUE;

		mpd_send_idle_mask(mpd.conn, MPD_IDLE_PLAYER);

		GIOChannel *channel = g_io_channel_unix_new(
				mpd_connection_get_fd(mpd.conn));
		mpd.source = g_io_add_watch(channel, G_IO_IN | G_IO_HUP,
				mpd_parse, NULL);
		g_io_channel_unref(channel);
		mpd.check_source = 0;

		if (mpd_status_get_state(mpd.status) == MPD_STATE_PLAY)
			mpd_schedule_check();

		return TRUE;
	}
}

static void mpd_update(void)
{
	struct mpd_status *prev = mpd.status;

	if (mpd.status)
		mpd_status_free(mpd.status);
	mpd.status = mpd_run_status(mpd.conn);
	mpd_response_finish(mpd.conn);

	if (mpd_status_get_state(mpd.status) == MPD_STATE_PLAY) {
		if (mpd_status_get_state(prev) == MPD_STATE_PLAY ||
				mpd_status_get_state(prev) == MPD_STATE_STOP) {
			// initialize new song
			if (mpd.song)
				mpd_song_free(mpd.song);
			mpd.song = mpd_run_current_song(mpd.conn);
			mpd_response_finish(mpd.conn);
			g_timer_start(mpd.song_pos);
			mpd.song_date = get_time();
			mpd.song_submitted = FALSE;

			// submit previous song(s)
			as_check_submit();

			// send now playing at the end so it won't be
			// overwritten by the queue
			as_now_playing();

			// schedule queueing
			mpd_schedule_check();
		} else if (mpd_status_get_state(prev) == MPD_STATE_PAUSE) {
			g_timer_continue(mpd.song_pos);
		}
	} else if (mpd_status_get_state(mpd.status) == MPD_STATE_PAUSE &&
			mpd_status_get_state(prev) == MPD_STATE_PLAY) {
			g_timer_stop(mpd.song_pos);
	} else if (mpd_status_get_state(mpd.status) == MPD_STATE_STOP) {
		as_check_submit();
		g_source_remove(mpd.check_source);
		mpd.check_source = 0;
	}
}

static void mpd_schedule_check(void)
{
	guint timeout;

	if (mpd.check_source > 0)
		g_source_remove(mpd.check_source);

	if (mpd_song_get_duration(mpd.song) >= 480)
		timeout = 240;
	else
		timeout = mpd_song_get_duration(mpd.song) / 2;

	mpd.check_source = g_timeout_add_seconds(timeout, scmpc_check, NULL);
}

gboolean mpd_parse(G_GNUC_UNUSED GIOChannel *source, GIOCondition condition,
		G_GNUC_UNUSED gpointer data)
{
	if (condition & G_IO_HUP) {
		mpd_disconnect();
		g_message("Disconnected from MPD, reconnecting");
		return TRUE;
	} else if (condition & G_IO_IN) {
		enum mpd_idle events = mpd_recv_idle(mpd.conn, FALSE);

		if (!mpd_response_finish(mpd.conn)) {
			g_warning("Failed to read MPD response: %s",
					mpd_connection_get_error_message(
						mpd.conn));
			mpd_disconnect();
			return FALSE;
		}

		if (events & MPD_IDLE_PLAYER) {
			mpd_update();
		}

		mpd_send_idle_mask(mpd.conn, MPD_IDLE_PLAYER);
		return TRUE;
	} else {
		// this shouldn't happen
		return FALSE;
	}
}

gboolean mpd_reconnect(G_GNUC_UNUSED gpointer data)
{
	if (mpd.connected)
		return TRUE;

	mpd.connected = mpd_connect();
	if (!mpd.connected) {
		mpd_disconnect();
		return TRUE;
	}

	return TRUE;
}

static void mpd_disconnect(void)
{
	if (mpd.conn)
		mpd_connection_free(mpd.conn);
	mpd.connected = FALSE;
	mpd.conn = NULL;
}
