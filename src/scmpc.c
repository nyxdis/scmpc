/**
 * scmpc.c: The front end of the program.
 *
 * ==================================================================
 * Copyright (c) 2009-2026 Freydis Mende <freydis.mende@proton.me>
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
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#include <mpd/client.h>

#include "audioscrobbler.h"
#include "misc.h"
#include "mpd.h"
#include "preferences.h"
#include "queue.h"
#include "scmpc.h"

/* Static function prototypes */
static pid_t scmpc_is_running(void);
static gboolean scmpc_pid_create(void);
static void scmpc_pid_remove(void);
static void scmpc_cleanup(void);

static void sighandler(int sig);
static gboolean signal_parse(GIOChannel *source, GIOCondition condition,
                             gpointer data);
static gboolean open_signal_pipe(void);
static void close_signal_pipe(void);
/**
 * Pipe for incoming UNIX signals
 */
static int signal_pipe[2] = {-1, -1};

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

int main(int argc, char *argv[]) {
  pid_t pid;
  struct sigaction sa;

  if (init_preferences(argc, argv) == FALSE) {
    g_printerr("Config file parsing failed\n");
    exit(EXIT_FAILURE);
  }

  /* Open the log file before forking, so that if there is an error, the
   * user will get some idea what is going on */
  open_log(prefs.log_file);

  g_log_set_default_handler(scmpc_log, NULL);

  /* Check if scmpc is already running */
  if ((pid = scmpc_is_running()) > 0) {
    g_critical("Daemon is already running with PID: %ld", (long)pid);
    clear_preferences();
    exit(EXIT_FAILURE);
  } else if (pid < 0 && prefs.fork) {
    g_critical("Cannot tell whether scmpc is already running, not starting");
    clear_preferences();
    exit(EXIT_FAILURE);
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
  sigaction(SIGHUP, &sa, NULL);
  /* a server closing the connection must not kill scmpc mid-write */
  sa.sa_handler = SIG_IGN;
  sigaction(SIGPIPE, &sa, NULL);

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
  mpd.idle_source = 0;
  if (!mpd_connect()) {
    mpd_disconnect();
    mpd_schedule_reconnect();
  }

  // set up main loop events
  loop = g_main_loop_new(NULL, FALSE);

  // save queue
  if (prefs.cache_interval > 0) {
    cache_save_source =
        g_timeout_add_seconds(prefs.cache_interval * 60, queue_save, NULL);
  }

  g_main_loop_run(loop);

  scmpc_cleanup();
}

/**
 * Check if there is a running scmpc instance
 *
 * Returns the pid of the running instance, 0 if there is none, or -1 if
 * the pid file cannot be checked.
 */
static pid_t scmpc_is_running(void) {
  FILE *pid_file = fopen(prefs.pid_file, "r");
  pid_t pid;

  if (!pid_file && errno == ENOENT)
    return 0;

  if (!pid_file) {
    g_warning("Cannot open pid file (%s) for reading: %s", prefs.pid_file,
              g_strerror(errno));
    return -1;
  }

  /* pid 0 or below would address a process group in kill() */
  if (fscanf(pid_file, "%d", &pid) < 1 || pid <= 0) {
    fclose(pid_file);
    if (unlink(prefs.pid_file) < 0) {
      g_warning("Invalid pid file %s cannot be removed, "
                "please remove this file or change pid_file in "
                "your configuration.",
                prefs.pid_file);
      return -1;
    }
    g_message("Invalid pid file %s removed.", prefs.pid_file);
    return 0;
  }

  fclose(pid_file);

  /* EPERM: the process exists but belongs to someone else */
  if (!kill(pid, 0) || errno == EPERM)
    return pid;

  if (errno == ESRCH) {
    if (unlink(prefs.pid_file) < 0) {
      g_warning("Old pid file %s cannot be removed, "
                "please remove this file or change pid_file in "
                "your configuration.",
                prefs.pid_file);
      return -1;
    }
    g_message("Old pid file removed.");
    return 0;
  }

  g_warning("Cannot check pid %ld from %s: %s", (long)pid, prefs.pid_file,
            g_strerror(errno));
  return -1;
}

/**
 * Create the pid file and write the current pid
 */
static gboolean scmpc_pid_create(void) {
  FILE *pid_file = fopen(prefs.pid_file, "w");
  if (!pid_file) {
    g_warning("Cannot open pid file (%s) for writing: %s", prefs.pid_file,
              g_strerror(errno));
    return FALSE;
  }

  fprintf(pid_file, "%u\n", getpid());
  fclose(pid_file);
  return TRUE;
}

/**
 * Delete the pid file
 */
static void scmpc_pid_remove(void) {
  if (unlink(prefs.pid_file) < 0)
    g_warning("Could not remove pid file: %s", g_strerror(errno));
}

/**
 * Handler for UNIX signals
 *
 * Runs in signal context, so it may only use async-signal-safe calls. The
 * main loop picks the signal number up from the pipe.
 */
static void sighandler(int sig) {
  int saved_errno = errno;
  unsigned char byte = (unsigned char)sig;
  /* if this fails the pipe is full and a shutdown is already pending */
  ssize_t ret = write(signal_pipe[1], &byte, 1);

  (void)ret;
  errno = saved_errno;
}

/**
 * Open the pipe for UNIX signals
 */
static gboolean open_signal_pipe(void) {
  GIOChannel *channel;

  if (pipe(signal_pipe) < 0) {
    g_critical("Opening signal pipe failed, signals will not be "
               "caught: %s",
               g_strerror(errno));
    return FALSE;
  }

  /* never block inside the signal handler */
  fcntl(signal_pipe[1], F_SETFL, fcntl(signal_pipe[1], F_GETFL) | O_NONBLOCK);

  channel = g_io_channel_unix_new(signal_pipe[0]);
  signal_source = g_io_add_watch(channel, G_IO_IN, signal_parse, NULL);
  g_io_channel_unref(channel);
  return TRUE;
}

/**
 * Close the pipe for UNIX signals
 */
static void close_signal_pipe(void) {
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
                             G_GNUC_UNUSED gpointer data) {
  gint fd = g_io_channel_unix_get_fd(source);
  unsigned char sig;

  if (read(fd, &sig, 1) != 1) {
    g_message("Reading from signal pipe failed, re-opening pipe.");
    close_signal_pipe();
    open_signal_pipe();
    return FALSE; /* the new pipe has its own watch */
  }

  g_message("Caught signal %d, exiting.", sig);
  scmpc_shutdown();
  return TRUE;
}

/**
 * Fork to background
 */
static void daemonise(void) {
  pid_t pid;
  int fd;

  if ((pid = fork()) < 0) {
    g_critical("Could not fork process: %s", g_strerror(errno));
    clear_preferences();
    exit(EXIT_FAILURE);
  } else if (pid) { /* The parent */
    exit(EXIT_SUCCESS);
  }

  /* The child: leave the terminal's session so closing it does not
   * send us SIGHUP, and let go of the working directory and stdio */
  if (setsid() < 0)
    g_warning("Could not create a new session: %s", g_strerror(errno));
  if (chdir("/") < 0)
    g_warning("Could not change directory to /: %s", g_strerror(errno));
  umask(022);

  fd = open("/dev/null", O_RDWR);
  if (fd >= 0) {
    dup2(fd, STDIN_FILENO);
    dup2(fd, STDOUT_FILENO);
    dup2(fd, STDERR_FILENO);
    if (fd > STDERR_FILENO)
      close(fd);
  }

  if (scmpc_pid_create() == FALSE) {
    g_critical("Failed to create PID file");
    clear_preferences();
    exit(EXIT_FAILURE);
  }
}

void scmpc_shutdown(void) {
  if (g_main_loop_is_running(loop))
    g_main_loop_quit(loop);
}

/**
 * Release resources
 */
static void scmpc_cleanup(void) {
  g_source_remove(signal_source);
  if (prefs.cache_interval > 0)
    g_source_remove(cache_save_source);
  if (mpd.idle_source > 0)
    g_source_remove(mpd.idle_source);
  if (mpd.check_source > 0)
    g_source_remove(mpd.check_source);
  if (mpd.reconnect_source > 0)
    g_source_remove(mpd.reconnect_source);

  if (current_song_eligible_for_submission() && prefs.queue_length > 0)
    queue_add_current_song();
  if (prefs.fork)
    scmpc_pid_remove();
  close_signal_pipe();
  if (prefs.cache_interval > 0)
    queue_save(NULL);
  queue_cleanup();
  if (mpd.song_pos)
    g_timer_destroy(mpd.song_pos);
  clear_preferences();
  as_cleanup();
  if (mpd.conn != NULL)
    mpd_connection_free(mpd.conn);
}

void kill_scmpc(void) {
  FILE *pid_file = fopen(prefs.pid_file, "r");
  pid_t pid;

  if (!pid_file) {
    g_printerr("Unable to open pid file %s: %s\n", prefs.pid_file,
               g_strerror(errno));
    exit(EXIT_FAILURE);
  }

  /* pid 0 or below would signal a whole process group */
  if (fscanf(pid_file, "%d", &pid) < 1 || pid <= 0) {
    fclose(pid_file);
    g_printerr("Invalid pid file %s\n", prefs.pid_file);
    exit(EXIT_FAILURE);
  }
  fclose(pid_file);

  if (kill(pid, SIGTERM) < 0) {
    g_printerr("Cannot kill scmpc with pid %ld: %s\n", (long)pid,
               g_strerror(errno));
    exit(EXIT_FAILURE);
  }

  exit(EXIT_SUCCESS);
}

/**
 * Check if the current song is eligible for submission
 */
static gboolean current_song_eligible_for_submission(void) {
  if (!mpd.song)
    return FALSE;

  return (mpd.song_state != SONG_SUBMITTED &&
          (g_timer_elapsed(mpd.song_pos, NULL) >= 240 ||
           g_timer_elapsed(mpd.song_pos, NULL) >=
               mpd_song_get_duration(mpd.song) * 0.5));
}

gboolean scmpc_check(G_GNUC_UNUSED gpointer data) {
  if (current_song_eligible_for_submission() && prefs.queue_length > 0) {
    queue_add_current_song();
    mpd.check_source = 0; // returning FALSE removes this timeout
    return FALSE;
  }

  // TODO don't return true, reschedule this properly
  return TRUE;
}
