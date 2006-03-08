/**
 * main.c: The front end of the programme.
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

#include <pthread.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* Prerequisite of audioscrobbler.h */
#include <curl/curl.h>

#include <libdaemon/dpid.h>
#include <libdaemon/dlog.h>
#include <libdaemon/dfork.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "liberror.h"
#include "misc.h"
#include "libmpd.h"
#include "mpd.h"
#include "scmpc.h"
#include "audioscrobbler.h"
#include "preferences.h"

/* Static function prototypes */
static void cleanup(void);
static void daemonise(void);
static void *sig_handler(void *arg);
const char *pid_filename(void);

/* Enable other threads the signal the main thread that they want the program
 * to finish, via the end_program() function. */
pthread_mutex_t end_program_mutex;
pthread_cond_t end_program_cond;

/* Submission queue mutex. It's initialised here, because there are three 
 * threads using it... */
extern pthread_mutex_t submission_queue_mutex;

/* Struct to hold the ids of the various threads that are being created. */
struct {
	pthread_t signal;
	pthread_t mpd;
	pthread_t scrobbler;
	pthread_t cache;
} thread_ids;

/* Declared in preferences.h. */
extern struct preferences prefs;

int main(int argc, char *argv[])
{
	pid_t pid;
	sigset_t signal_set;
	pthread_attr_t attr_j, attr_d;
	
	/* Set indetification string for the daemon for both syslog and PID file */
	daemon_log_ident = daemon_ident_from_argv0(argv[0]);
	daemon_pid_file_proc = pid_filename;

	/* Used by some libdaemon functions internally. */
	daemon_log_use = DAEMON_LOG_STDERR;
	
	init_preferences(argc, argv);

	/* Open the log file before forking so that if there's an error, the user
	 * will get some idea that there isn't going to be any logging. */
	open_log(prefs.log_file);
	
	if ((pid = daemon_pid_file_is_running()) >= 0) {
		fprintf(stderr, "Daemon is already running with PID: %u\n", pid);
		exit(EXIT_FAILURE);
	}
	
	if (prefs.fork)
		daemonise();

	/* Thread attributes */
	pthread_attr_init(&attr_j);
	pthread_attr_init(&attr_d);
	pthread_attr_setdetachstate(&attr_j, PTHREAD_CREATE_JOINABLE);
	pthread_attr_setdetachstate(&attr_d, PTHREAD_CREATE_DETACHED);
	
	/* Block all signals */
	sigfillset(&signal_set);
	pthread_sigmask(SIG_BLOCK, &signal_set, NULL);

	/* create the (detached) signal handling thread */
	pthread_create(&(thread_ids.signal), &attr_d, sig_handler, NULL);

	/* Initialise mutexes. */
	pthread_mutex_init(&submission_queue_mutex, NULL);
	pthread_mutex_init(&end_program_mutex, NULL);
	pthread_cond_init(&end_program_cond, NULL);

	/* Create the (detached) mpd checking thread. */
	pthread_create(&(thread_ids.mpd), &attr_d, mpd_thread, NULL);

	/* Create the (joinable) audioscrobbler thread. */
	pthread_create(&(thread_ids.scrobbler), &attr_j, as_thread, NULL);

	/* Create the (joinable) cache saving thread (if required). */
	if (prefs.cache_interval > 0) {
		pthread_create(&(thread_ids.cache), &attr_j, cache_thread, NULL);
	}

	pthread_attr_destroy(&attr_j);
	pthread_attr_destroy(&attr_d);
	
	/* Wait for anything to end the program via the end_program_cond, which
	 * will be set by the end_program() function. */
	pthread_mutex_lock(&end_program_mutex);
	pthread_cond_wait(&end_program_cond, &end_program_mutex);
	pthread_mutex_unlock(&end_program_mutex);
	
	cleanup();

	return EXIT_SUCCESS;
}

static void cleanup(void)
{
	int rc;
	
	pthread_cancel(thread_ids.mpd);
	pthread_cancel(thread_ids.signal);
	
	if (prefs.cache_interval > 0) {
		pthread_cancel(thread_ids.cache);
		rc = pthread_join(thread_ids.cache, NULL);
		if (rc) {
			scmpc_log(DEBUG, "pthread_join(cache) failed: %d", rc);
		}
	}
	
	pthread_cancel(thread_ids.scrobbler);
	rc = pthread_join(thread_ids.scrobbler, NULL);
	if (rc) {
		scmpc_log(DEBUG, "pthread_join(scrobbler) failed: %d", rc);
	}

	if (prefs.fork && daemon_pid_file_remove() != 0)
		scmpc_log(ERROR, "Could not unlink pid file.");
	
	scmpc_log(INFO, "Exiting.\n");
	close_log();

	clear_preferences();
	
	pthread_mutex_destroy(&submission_queue_mutex);
	pthread_mutex_destroy(&end_program_mutex);
	
	pthread_exit(EXIT_SUCCESS);
}

const char *pid_filename(void)
{
	return (const char *)prefs.pid_file;
}

static void daemonise(void)
{
	pid_t pid;

	/* Prepare for return value passing from the initialization procedure of 
	 * the daemon process */
	daemon_retval_init();

	if ((pid = daemon_fork()) < 0) {
		/* Something went wrong... */
		daemon_retval_done();
		fputs("Could not fork process.\n", stderr);
		exit(EXIT_FAILURE);
	} else if (pid) { /* The parent */
		int ret;

		/* Wait for 2 seconds for the return value passed from the daemon 
		 * process */
		if ((ret = daemon_retval_wait(2)) < 0) {
			fputs("Did not recieve return value from daemon process.\n",stderr);
			exit(EXIT_FAILURE);
		}

		if (ret == 0) {
			exit(EXIT_SUCCESS);
		} else {
			fputs("Daemon failed to start.\n", stderr);
			exit(EXIT_FAILURE);
		}
	} else { /* The daemon */
		/* Create the PID file */
		if (daemon_pid_file_create() < 0) {
			scmpc_log(ERROR, "Could not create PID file (%s): %s.",
					prefs.pid_file, strerror(errno));
			/* Send the error condition to the parent process */
			daemon_retval_send(1);
			exit(EXIT_FAILURE);
		}

		/* Send OK to parent process */
		daemon_retval_send(0);
	}
}

/**
 * sig_handler()
 * 
 * Runs in a separate thread, and catches everything. All signals
 * are blocked in other threads, so they only get caught here.
 * When it catches a signal it changes the global variable
 * signal_handled, and then carries on it's merry way. The main()
 * thread checks the signal_handled variable, and cancels all the
 * other threads.
 */
static void *sig_handler(void *arg)
{
	sigset_t signal_set;
	int signal;

	sigfillset(&signal_set);
		
	while (1)
	{
		/* Wait for any and all signals. */
		sigwait(&signal_set, &signal);

		switch (signal)
		{
			case SIGTERM:
			case SIGQUIT:
				end_program();
				break;
			case SIGINT:
				if (prefs.fork)
					scmpc_log(INFO, "SIGINT caught, and ignored.");
				else
					end_program();
				break;
			case SIGHUP:
				scmpc_log(INFO, "SIGHUP caught. Ignoring for now.");
				break;
			case SIGSEGV:
				scmpc_log(ERROR, "Segfault. Exiting (hopefully!).");
				break;
			default:
				scmpc_log(INFO, "Caught signal %d. Ignoring.", signal);
				break;
		}
	}
	return NULL;
}

void end_program(void)
{
	pthread_mutex_lock(&end_program_mutex);
	pthread_cond_signal(&end_program_cond);
	pthread_mutex_unlock(&end_program_mutex);

	/* It might be a while (in relative terms) for the main thread to notice
	 * it's time to cancel the calling thread, during which time the calling
	 * thread may do something stupid. Cancel it now. */
	pthread_exit((void *)1);
}
