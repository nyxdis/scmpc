/**
 * mpd.c: MPD back end.
 *
 * ==================================================================
 * Copyright (c) 2009-2013 Christoph Mende <mende.christoph@gmail.com>
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

#include "audioscrobbler.h"
#include "mpd.h"
#include "preferences.h"
#include "queue.h"
#include "scmpc.h"

struct mpd_t mpd;

static gboolean mpd_update(void);
static void mpd_schedule_check(void);
static gboolean mpd_parse(GIOChannel *source, GIOCondition condition,
                          gpointer data);

gboolean mpd_connect(void) {
  mpd.conn = mpd_connection_new(prefs.mpd_hostname, prefs.mpd_port,
                                prefs.mpd_timeout * 1000);
  if (!mpd.conn) {
    g_warning("Failed to connect to MPD: out of memory");
    return FALSE;
  } else if (mpd_connection_get_error(mpd.conn) != MPD_ERROR_SUCCESS) {
    g_warning("Failed to connect to MPD: %s",
              mpd_connection_get_error_message(mpd.conn));
    return FALSE;
  } else if (mpd_connection_cmp_server_version(mpd.conn, 0, 14, 0) < 0) {
    g_critical("MPD too old, please upgrade to 0.14 or newer");
    scmpc_shutdown();
    return FALSE;
  } else if (prefs.mpd_password && prefs.mpd_password[0] != '\0' &&
             !mpd_run_password(mpd.conn, prefs.mpd_password)) {
    g_warning("MPD rejected the password: %s",
              mpd_connection_get_error_message(mpd.conn));
    return FALSE;
  } else {
    mpd_command_list_begin(mpd.conn, TRUE);
    mpd_send_status(mpd.conn);
    mpd_send_current_song(mpd.conn);
    mpd_command_list_end(mpd.conn);

    /* drop what is left over from a previous connection */
    if (mpd.status)
      mpd_status_free(mpd.status);
    if (mpd.song)
      mpd_song_free(mpd.song);

    mpd.status = mpd_recv_status(mpd.conn);
    mpd_response_next(mpd.conn);
    mpd.song = mpd_recv_song(mpd.conn);
    mpd_response_finish(mpd.conn);

    if (mpd_connection_get_error(mpd.conn) != MPD_ERROR_SUCCESS ||
        !mpd.status) {
      g_warning("Failed to connect to MPD: %s",
                mpd_connection_get_error_message(mpd.conn));
      return FALSE;
    }

    g_message("Connected to MPD");

    mpd_send_idle_mask(mpd.conn, MPD_IDLE_PLAYER);

    GIOChannel *channel =
        g_io_channel_unix_new(mpd_connection_get_fd(mpd.conn));
    mpd.idle_source = g_io_add_watch(channel, G_IO_IN, mpd_parse, NULL);
    g_io_channel_unref(channel);
    mpd.check_source = 0;

    if (mpd_status_get_state(mpd.status) == MPD_STATE_PLAY && mpd.song) {
      /* the song started before we connected */
      mpd.song_date = get_time() - mpd_status_get_elapsed_time(mpd.status);
      mpd.song_state = SONG_NEW;
      as_now_playing();
      g_timer_start(mpd.song_pos);
      mpd_schedule_check();
    } else {
      mpd.check_source = 0;
      g_timer_stop(mpd.song_pos);
      mpd.song_state = SONG_NEW;
    }

    return TRUE;
  }
}

/**
 * Parse status changes, check for state changes (play/stop/pause) and
 * retrieve the current song on play->play or stop->play.
 * Clean up on play->pause and *->stop
 */
static gboolean mpd_update(void) {
  enum mpd_state prev_state = MPD_STATE_UNKNOWN;

  if (mpd.status) {
    prev_state = mpd_status_get_state(mpd.status);
    mpd_status_free(mpd.status);
  }
  mpd.status = mpd_run_status(mpd.conn);
  if (!mpd.status || !mpd_response_finish(mpd.conn))
    return FALSE;

  if (mpd_status_get_state(mpd.status) == MPD_STATE_PLAY) {
    if (prev_state == MPD_STATE_PLAY || prev_state == MPD_STATE_STOP) {
      struct mpd_song *song = mpd_run_current_song(mpd.conn);
      if (!mpd_response_finish(mpd.conn)) {
        if (song)
          mpd_song_free(song);
        return FALSE;
      }

      // Same song: either a seek, or MPD reporting the start of playback
      // twice. Keep the play time and submission state so the song is
      // scrobbled once. Only a song that played for a while and is back
      // at its start, as when it repeats, counts as a new play.
      if (prev_state == MPD_STATE_PLAY && song && mpd.song &&
          mpd_song_get_id(song) == mpd_song_get_id(mpd.song) &&
          (mpd_status_get_elapsed_time(mpd.status) > 1 ||
           g_timer_elapsed(mpd.song_pos, NULL) < 3)) {
        mpd_song_free(song);
        return TRUE;
      }

      // initialize new song
      if (mpd.song)
        mpd_song_free(mpd.song);
      mpd.song = song;
      g_timer_start(mpd.song_pos);
      mpd.song_date = get_time();
      mpd.song_state = SONG_NEW;

      // submit previous song(s)
      as_check_submit();

      // send now playing at the end so it won't be
      // overwritten by the queue
      as_now_playing();

      // schedule queueing
      mpd_schedule_check();
    } else if (prev_state == MPD_STATE_PAUSE) {
      if (mpd.song_state == SONG_NEW)
        as_now_playing();
      g_timer_continue(mpd.song_pos);
    }
  } else if (mpd_status_get_state(mpd.status) == MPD_STATE_PAUSE &&
             prev_state == MPD_STATE_PLAY) {
    g_timer_stop(mpd.song_pos);
  } else if (mpd_status_get_state(mpd.status) == MPD_STATE_STOP) {
    as_check_submit();
    if (mpd.check_source > 0)
      g_source_remove(mpd.check_source);
    mpd.check_source = 0;
  }
  return TRUE;
}

/**
 * Schedule a check of the current song for submission
 */
static void mpd_schedule_check(void) {
  gushort timeout;

  if (mpd.check_source > 0)
    g_source_remove(mpd.check_source);
  mpd.check_source = 0;

  /* nothing to check for, or too short to be scrobbled (and a 0 second
   * timeout would fire right away) */
  if (!mpd.song || mpd_song_get_duration(mpd.song) < 30)
    return;

  if (mpd_song_get_duration(mpd.song) >= 480)
    timeout = 240;
  else
    timeout = mpd_song_get_duration(mpd.song) * 0.5;

  mpd.check_source = g_timeout_add_seconds(timeout, scmpc_check, NULL);
}

/**
 * Parse mpd responses, this should only be called when "idle" returns
 */
static gboolean mpd_parse(G_GNUC_UNUSED GIOChannel *source,
                          G_GNUC_UNUSED GIOCondition condition,
                          G_GNUC_UNUSED gpointer data) {
  enum mpd_idle events = mpd_recv_idle(mpd.conn, FALSE);

  if (!mpd_response_finish(mpd.conn) ||
      ((events & MPD_IDLE_PLAYER) && !mpd_update())) {
    g_warning("Failed to read MPD response: %s",
              mpd_connection_get_error_message(mpd.conn));
    mpd_disconnect();
    mpd_schedule_reconnect();
    mpd.idle_source = 0; /* returning FALSE removes this watch */
    return FALSE;
  }

  mpd_send_idle_mask(mpd.conn, MPD_IDLE_PLAYER);
  return TRUE;
}

gboolean mpd_reconnect(G_GNUC_UNUSED gpointer data) {
  if (!mpd_connect()) {
    mpd_disconnect();
    return TRUE;
  }

  mpd.reconnect_source = 0;
  return FALSE;
}

void mpd_disconnect(void) {
  if (mpd.conn)
    mpd_connection_free(mpd.conn);
  mpd.conn = NULL;
}

void mpd_schedule_reconnect(void) {
  if (mpd.reconnect_source > 0)
    return;
  mpd.reconnect_source = g_timeout_add_seconds(30, mpd_reconnect, NULL);
}
