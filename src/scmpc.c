/**
 * scmpc.c: The front end of the program.
 *
 * ==================================================================
 * Copyright (c) 2008 Christoph Mende <angelos@unkreativ.org>
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
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>

#include "misc.h"
#include "audioscrobbler.h"
#include "preferences.h"

/* Static function prototypes */
static int scmpc_is_running(void);
static int scmpc_pid_create(void);
static int scmpc_pid_remove(void);

static void sighandler(int sig);
static void daemonise(void);
static void cleanup(void);
const char *pid_filename(void);

/* Declared in preferences.h */
extern struct preferences prefs;

int main(int argc, char *argv[])
{
	int mpd_sockfd = 0;
	fd_set read_flags;
	pid_t pid;
	struct sigaction sa;
	struct timeval waitd;
	time_t last_queue_save = 0;

	init_preferences(argc,argv);

	/* Open the log file before forking, so that if there is an error, the
	 * user will get some idea what is going on */
	open_log(prefs.log_file);

	/* Check if scmpc is already running */
	if((pid = scmpc_is_running()) > 0) {
		fprintf(stderr,"Daemon is already running with PID: %u\n",pid);
		exit(EXIT_FAILURE);
	}

	/* Daemonise if wanted */
	if(prefs.fork) daemonise();

	/* Signal handler */
	sa.sa_handler = sighandler;
	sigfillset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	sigaction(SIGINT,&sa,NULL);
	sigaction(SIGTERM,&sa,NULL);
	sigaction(SIGQUIT,&sa,NULL);

	as_connection_init();
	//mpd_connect();

	while(1)
	{
		waitd.tv_sec = 1;
		waitd.tv_usec = 0;

		FD_ZERO(&read_flags);
		FD_SET(mpd_sockfd,&read_flags);

		if(select(mpd_sockfd+1,&read_flags,NULL,NULL,&waitd) < 0)
			continue;

		if(FD_ISSET(mpd_sockfd,&read_flags)) {
		}

		if(difftime(time(NULL),last_queue_save) >= prefs.cache_interval * 60)
		{
			queue_save();
			last_queue_save = time(NULL);
		}
	}

	exit(EXIT_SUCCESS);
}

static int scmpc_is_running(void)
{
	FILE *pid_file = fopen(prefs.pid_file,"r");
	pid_t pid;

	if(pid_file == NULL && errno == ENOENT) return 0;

	if(pid_file == NULL) {
		/* Unable to open PID file, returning error */
		scmpc_log(ERROR,"Cannot open pid file (%s) for reading: %s",
				prefs.pid_file,strerror(errno));
		return -1;
	}

	if(fscanf(pid_file,"%u",&pid) < 1) {
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
			printf("Old pid file removed.\n");
			return 0;
		}
	}

	return 0;
}

static int scmpc_pid_create(void)
{
	FILE *pid_file = fopen(prefs.pid_file,"w");
	if(pid_file == NULL) {
		scmpc_log(ERROR, "Cannot open pid file (%s) for writing: "
				"%s\n",prefs.pid_file,strerror(errno));
		return -1;
	}

	fprintf(pid_file,"%u\n",getpid());
	fclose(pid_file);
	return 0;
}

static int scmpc_pid_remove(void)
{
        if(unlink(prefs.pid_file) < 0) {
                scmpc_log(ERROR,"Could not remove pid file: %s",strerror(errno));
                return 1;
	}
	return 0;

}

static void sighandler(int sig)
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
		fprintf(stderr,"Could not fork process.\n");
		exit(EXIT_FAILURE);
	} else if(pid) { /* The parent */
		exit(EXIT_SUCCESS);
	} else { /* The daemon */
		/* Unset the ridiculous umask
		umask(027);*/

		/* Create the PID file */
		if(scmpc_pid_create() < 0)
			exit(EXIT_FAILURE);
	}
}

static void cleanup(void)
{
	queue_save();
	scmpc_pid_remove();
}
