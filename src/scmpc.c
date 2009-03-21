/**
 * scmpc.c: The front end of the program.
 *
 * ==================================================================
 * Copyright (c) 2009 Christoph Mende <angelos@unkreativ.org>
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
#include <poll.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>

#include "misc.h"
#include "audioscrobbler.h"
#include "mpd.h"
#include "preferences.h"
#include "queue.h"
#include "scmpc.h"

/* Static function prototypes */
static gint scmpc_is_running(void);
static gint scmpc_pid_create(void);
static gint scmpc_pid_remove(void);

static void sighandler(gint sig);
static void daemonise(void);
static void cleanup(void);
gconstpointer pid_filename(void);

int main(int argc, char *argv[])
{
	gchar *buf;
	pid_t pid;
	struct pollfd fds[1];
	struct sigaction sa;
	time_t last_queue_save = 0, mpd_last_fail = 0, as_last_fail = 0;

	if(init_preferences(argc,argv) < 0)
		g_error("Config file parsing failed");

	/* Open the log file before forking, so that if there is an error, the
	 * user will get some idea what is going on */
	open_log(prefs.log_file);

	/* Check if scmpc is already running */
	if((pid = scmpc_is_running()) > 0) {
		clear_preferences();
		g_error("Daemon is already running with PID: %ld",(long)pid);
	}

	/* Daemonise if wanted */
	if(prefs.fork)
		daemonise();

	/* Signal handler */
	sa.sa_handler = sighandler;
	sigfillset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	sigaction(SIGINT,&sa,NULL);
	sigaction(SIGTERM,&sa,NULL);
	sigaction(SIGQUIT,&sa,NULL);

	if(as_connection_init() < 0) {
		cleanup();
		exit(EXIT_FAILURE);
	}
	if(mpd_connect() < 0)
		mpd_last_fail = time(NULL);
	as_handshake();
	queue_load();
	last_queue_save = time(NULL);
	current_song.pos = g_timer_new();

	fds[0].events = POLLIN;

	for(;;)
	{
		fds[0].fd = mpd_info.sockfd;
		poll(fds,1,prefs.mpd_interval);

		/* Check for new events on MPD socket */
		if(fds[0].revents & POLLIN) {
			buf = g_malloc0(256);
			if(read(mpd_info.sockfd,buf,255) > 0)
				mpd_parse(buf);
			else {
				scmpc_log(ERROR,"Failed to read from MPD, reconnecting");
				mpd_info.status = DISCONNECTED;
			}
			g_free(buf);
		}

		/* Check if MPD socket disconnected */
		if(fds[0].revents & POLLHUP) {
			mpd_info.status = DISCONNECTED;
			scmpc_log(INFO,"Disconnected from MPD, reconnecting");
			if(mpd_connect() < 0)
				mpd_last_fail = time(NULL);
		}

		/* Check if song is eligible for submission */
		if(current_song.song_state == NEW && (g_timer_elapsed(current_song.pos,NULL) >= 240 || g_timer_elapsed(current_song.pos,NULL) >= current_song.length / 2)) {
			queue_add(current_song.artist,current_song.title,current_song.album,current_song.length,current_song.track,current_song.date);
			current_song.song_state = SUBMITTED;
		}

		/* save queue */
		if(difftime(time(NULL),last_queue_save) >= prefs.cache_interval * 60) {
		        queue_save();
		        last_queue_save = time(NULL);
		}

		/* reconnect to MPD */
		if(mpd_info.status == DISCONNECTED && difftime(time(NULL),mpd_last_fail) >= 1800) {
			mpd_info.sockfd = -1;
			if(mpd_connect() < 0)
				mpd_last_fail = time(NULL);
		}

		/* submit queue */
		if(queue.length > 0 && as_conn.status == CONNECTED && difftime(time(NULL),as_last_fail) >= 600) {
			if(as_submit() == 1)
				as_last_fail = time(NULL);
		}
	}
}

static gint scmpc_is_running(void)
{
	FILE *pid_file = fopen(prefs.pid_file,"r");
	pid_t pid;

	if(pid_file == NULL && errno == ENOENT) return 0;

	if(pid_file == NULL) {
		/* Unable to open PID file, returning error */
		scmpc_log(ERROR,"Cannot open pid file (%s) for reading: %s",
				prefs.pid_file,g_strerror(errno));
		return -1;
	}

	if(fscanf(pid_file,"%d",&pid) < 1) {
		/* Read nothing from pid_file */
		fclose(pid_file);
		if(unlink(prefs.pid_file) < 0) {
			/* Unable to remove invalid PID file, returning error */
			scmpc_log(ERROR,"Invalid pid file %s cannot be removed, "
				"please remove this file or change pid_file in "
				"your configuration.",prefs.pid_file);
			return -1;
		} else {
			/* Invalid PID file removed, start new instance */
			printf("Invalid pid file %s removed.\n",prefs.pid_file);
			return 0;
		}
	}

	fclose(pid_file);

	if(kill(pid,0) == 0) {
		/* scmpc already running */
		return pid;
	} else if(errno == ESRCH) {
		/* no such process */
		if(unlink(prefs.pid_file) < 0) {
			/* Unable to remove invalid pid file, returning error */
			fprintf(stderr,"Old pid file %s cannot be removed,"
				" please remove this file or change pid_file in"
				" your configuration.",prefs.pid_file);
			return -1;
		} else {
			/* Old pid file removed, starting new instance */
			puts("Old pid file removed.");
			return 0;
		}
	}

	return 0;
}

static gint scmpc_pid_create(void)
{
	FILE *pid_file = fopen(prefs.pid_file,"w");
	if(pid_file == NULL) {
		scmpc_log(ERROR, "Cannot open pid file (%s) for writing: "
				"%s\n",prefs.pid_file,g_strerror(errno));
		return -1;
	}

	fprintf(pid_file,"%u\n",getpid());
	fclose(pid_file);
	return 0;
}

static gint scmpc_pid_remove(void)
{
        if(unlink(prefs.pid_file) < 0) {
                scmpc_log(ERROR,"Could not remove pid file: %s",g_strerror(errno));
                return 1;
	}
	return 0;

}

static void sighandler(gint sig)
{
	scmpc_log(INFO,"Caught signal %d, exiting.",sig);
	cleanup();
	exit(EXIT_SUCCESS);
}

static void daemonise(void)
{
	pid_t pid;

	if((pid = fork()) < 0) {
		/* Something went wrong... */
		clear_preferences();
		g_error("Could not fork process.");
	} else if(pid) { /* The parent */
		exit(EXIT_SUCCESS);
	} else { /* The child */
		/* Force sane umask */
		umask(022);

		/* Create the PID file */
		if(scmpc_pid_create() < 0) {
			clear_preferences();
			g_error("Failed to create PID file");
		}
	}
}

static void cleanup(void)
{
	if(prefs.fork) scmpc_pid_remove();
	queue_save();
	g_timer_destroy(current_song.pos);
	clear_preferences();
	as_cleanup();
	mpd_cleanup();
}

void kill_scmpc(void)
{
	FILE *pid_file = fopen(prefs.pid_file,"r");
	pid_t pid;

	if(pid_file == NULL)
		g_error("Unable to open PID file: %s",g_strerror(errno));

	if(fscanf(pid_file,"%d",&pid) < 1)
		g_error("Invalid PID file");

	if(kill(pid,SIGTERM) < 0)
		g_error("Cannot kill running scmpc");

	exit(EXIT_SUCCESS);
}
