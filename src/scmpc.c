/**
 * scmpc.c: The front end of the program.
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


#include <errno.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#include <mpd/client.h>

#include "misc.h"
#include "audioscrobbler.h"
#include "preferences.h"
#include "queue.h"
#include "scmpc.h"
#include "mpd.h"

/* Static function prototypes */
static gint scmpc_is_running(void);
static gboolean scmpc_pid_create(void);
static void scmpc_pid_remove(void);
static void scmpc_cleanup(void);

static void sighandler(gint sig);
static gboolean signal_parse(GIOChannel *source, GIOCondition condition,
		gpointer data);
static gboolean open_signal_pipe(void);
static void close_signal_pipe(void);
/**
 * Pipe for incoming UNIX signals
 */
static int signal_pipe[2] = { -1, -1 };

static void daemonise(void);
static gboolean current_song_eligible_for_submission(void);

/**
 * GSource for UNIX signals
 */
static guint signal_source;
/**
 * Timeout source for the queue save interval
 */
static guint cache_save_source;
/**
 * scmpc's main event loop
 */
static GMainLoop *loop;

int main(int argc, char *argv[])
{
	pid_t pid;
	struct sigaction sa;

	if (init_preferences(argc, argv) == FALSE)
		g_error("Config file parsing failed");

	/* Open the log file before forking, so that if there is an error, the
	 * user will get some idea what is going on */
	open_log(prefs.log_file);

	g_log_set_default_handler(scmpc_log, NULL);

	/* Check if scmpc is already running */
	if ((pid = scmpc_is_running()) > 0) {
		clear_preferences();
		g_error("Daemon is already running with PID: %ld", (long)pid);
	}

	/* Daemonise if wanted */
	if (prefs.fork)
		daemonise();

	/* Signal handler */
	open_signal_pipe();
	sa.sa_handler = sighandler;
	sigfillset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGQUIT, &sa, NULL);

	if (as_connection_init() == FALSE) {
		scmpc_cleanup();
		exit(EXIT_FAILURE);
	}
	as_authenticate();

	queue_init();
	queue_load();

	// submit the loaded queue
	as_check_submit();

	mpd.song_pos = g_timer_new();
	if (!mpd_connect()) {
		mpd_disconnect();
		mpd_schedule_reconnect();
	}

	// set up main loop events
	loop = g_main_loop_new(NULL, FALSE);

	// save queue
	cache_save_source = g_timeout_add_seconds(prefs.cache_interval * 60,
			queue_save, NULL);

	g_main_loop_run(loop);

	scmpc_cleanup();
}

/**
 * Check if there is a running scmpc instance
 */
static gint scmpc_is_running(void)
{
	FILE *pid_file = fopen(prefs.pid_file, "r");
	pid_t pid;

	if (!pid_file && errno == ENOENT)
		return 0;

	if (!pid_file) {
		/* Unable to open PID file, returning error */
		g_warning("Cannot open pid file (%s) for reading: %s",
				prefs.pid_file, g_strerror(errno));
		return -1;
	}

	if (fscanf(pid_file, "%d", &pid) < 1) {
		/* Read nothing from pid_file */
		fclose(pid_file);
		if (unlink(prefs.pid_file) < 0) {
			/* Unable to remove invalid PID file, returning error */
			g_warning("Invalid pid file %s cannot be removed, "
				"please remove this file or change pid_file in "
				"your configuration.", prefs.pid_file);
			return -1;
		} else {
			/* Invalid PID file removed, start new instance */
			g_message("Invalid pid file %s removed.",
					prefs.pid_file);
			return 0;
		}
	}

	fclose(pid_file);

	if (!kill(pid, 0)) {
		/* scmpc already running */
		return pid;
	} else if (errno == ESRCH) {
		/* no such process */
		if (unlink(prefs.pid_file) < 0) {
			/* Unable to remove invalid pid file, returning error */
			fprintf(stderr, "Old pid file %s cannot be removed, "
				" please remove this file or change pid_file in"
				" your configuration.", prefs.pid_file);
			return -1;
		} else {
			/* Old pid file removed, starting new instance */
			puts("Old pid file removed.");
			return 0;
		}
	}

	return 0;
}

/**
 * Create the pid file and write the current pid
 */
static gboolean scmpc_pid_create(void)
{
	FILE *pid_file = fopen(prefs.pid_file, "w");
	if (!pid_file) {
		g_warning("Cannot open pid file (%s) for writing: %s",
				prefs.pid_file, g_strerror(errno));
		return FALSE;
	}

	fprintf(pid_file, "%u\n", getpid());
	fclose(pid_file);
	return TRUE;
}

/**
 * Delete the pid file
 */
static void scmpc_pid_remove(void)
{
	if (unlink(prefs.pid_file) < 0)
		g_warning("Could not remove pid file: %s", g_strerror(errno));
}

/**
 * Handler for UNIX signals
 */
static void sighandler(gint sig)
{
	if (write(signal_pipe[1], &sig, 1) < 0) {
		g_message("Writing to signal pipe failed, re-opening pipe.");
		close_signal_pipe();
		open_signal_pipe();
	}
}

/**
 * Open the pipe for UNIX signals
 */
static gboolean open_signal_pipe(void)
{
	GIOChannel *channel;

	if (pipe(signal_pipe) < 0) {
		g_critical("Opening signal pipe failed, signals will not be "
				"caught: %s", g_strerror(errno));
		return FALSE;
	}

	channel = g_io_channel_unix_new(signal_pipe[0]);
	signal_source = g_io_add_watch(channel, G_IO_IN, signal_parse, NULL);
	g_io_channel_unref(channel);
	return TRUE;
}

/**
 * Close the pipe for UNIX signals
 */
static void close_signal_pipe(void)
{
	if (signal_pipe[0] > 0)
		close(signal_pipe[0]);
	if (signal_pipe[1] > 0)
		close(signal_pipe[1]);
	signal_pipe[0] = -1;
	signal_pipe[1] = -1;
}

/**
 * Parse incoming UNIX signals
 */
static gboolean signal_parse(GIOChannel *source,
		G_GNUC_UNUSED GIOCondition condition,
		G_GNUC_UNUSED gpointer data)
{
	gint fd = g_io_channel_unix_get_fd(source);
	gchar sig;
	if (read(fd, &sig, 1) < 0) {
		g_message("Reading from signal pipe failed, re-opening pipe.");
		close_signal_pipe();
		open_signal_pipe();
		return TRUE;
	} else {
		g_message("Caught signal %hhd, exiting.", sig);
		scmpc_shutdown();
		return TRUE;
	}
}

/**
 * Fork to background
 */
static void daemonise(void)
{
	pid_t pid;

	if ((pid = fork()) < 0) {
		/* Something went wrong... */
		clear_preferences();
		g_error("Could not fork process.");
	} else if (pid) { /* The parent */
		exit(EXIT_SUCCESS);
	} else { /* The child */
		/* Force sane umask */
		umask(022);

		/* Create the PID file */
		if (scmpc_pid_create() == FALSE) {
			clear_preferences();
			g_error("Failed to create PID file");
		}
	}
}

void scmpc_shutdown(void)
{
	if (g_main_loop_is_running(loop))
		g_main_loop_quit(loop);
}

/**
 * Release resources
 */
static void scmpc_cleanup(void)
{
	g_source_remove(signal_source);
	g_source_remove(mpd.idle_source);
	if (mpd.check_source > 0)
		g_source_remove(mpd.check_source);
	g_source_remove(cache_save_source);
	if (mpd.reconnect_source > 0)
		g_source_remove(mpd.reconnect_source);

	if (current_song_eligible_for_submission())
		queue_add_current_song();
	if (prefs.fork)
		scmpc_pid_remove();
	close_signal_pipe();
	queue_save(NULL);
	queue_cleanup();
	if (mpd.song_pos)
		g_timer_destroy(mpd.song_pos);
	clear_preferences();
	as_cleanup();
	if (mpd.conn != NULL)
		mpd_connection_free(mpd.conn);
}

void kill_scmpc(void)
{
	FILE *pid_file = fopen(prefs.pid_file, "r");
	pid_t pid;

	if (!pid_file)
		g_critical("Unable to open PID file: %s", g_strerror(errno));

	if (fscanf(pid_file, "%d", &pid) < 1)
		g_critical("Invalid PID file");

	if (kill(pid, SIGTERM) < 0)
		g_critical("Cannot kill running scmpc");

	exit(EXIT_SUCCESS);
}

/**
 * Check if the current song is eligible for submission
 */
static gboolean current_song_eligible_for_submission(void)
{
	if (!mpd.song)
		return FALSE;

	return (mpd.song_state != SONG_SUBMITTED &&
			(g_timer_elapsed(mpd.song_pos, NULL) >= 240 ||
			 g_timer_elapsed(mpd.song_pos, NULL) >=
				mpd_song_get_duration(mpd.song) * 0.5));
}

gboolean scmpc_check(G_GNUC_UNUSED gpointer data)
{
	if (current_song_eligible_for_submission()) {
		queue_add_current_song();
		return FALSE; // remove from main event loop
	}

	// TODO don't return true, reschedule this properly
	return TRUE;
}
