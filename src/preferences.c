/**
 * preferences.c: Controlling the programme's behaviour.
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
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <argtable2.h>
#include <confuse.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "exception.h"
#include "misc.h"
#include "scmpc.h"
#include "preferences.h"

#define DEFAULT_CONFIG_FILE SYSCONFDIR "/scmpc.conf"
#define DEFAULT_PID_FILE "/var/run/scmpc.pid"
#define DEFAULT_LOG_FILE "/var/log/scmpc.log"
#define DEFAULT_CACHE_FILE "/var/lib/scmpc/scmpc.cache"

extern struct preferences prefs;

/* Function prototypes */
static void set_defaults(void);
static void parse_config_file(void);
static void parse_command_line(int argc, char **argv);

static void set_defaults(void)
{
	prefs.mpd_hostname = strdup(DEFAULT_MPD_HOST);
	prefs.mpd_port = DEFAULT_MPD_PORT;
	prefs.mpd_timeout = DEFAULT_MPD_TIMEOUT;
	prefs.mpd_password = strdup("");
	prefs.as_username = strdup("");
	prefs.as_password = strdup("");
	prefs.fork = TRUE;
	prefs.log_level = DEFAULT_LOG_LEVEL;
	prefs.config_file = strdup(DEFAULT_CONFIG_FILE);
	prefs.log_file = strdup(DEFAULT_LOG_FILE);
	prefs.pid_file = strdup(DEFAULT_PID_FILE);
	prefs.cache_file = strdup(DEFAULT_CACHE_FILE);
	prefs.cache_interval = DEFAULT_CACHE_INTERVAL;
	prefs.queue_length = DEFAULT_QUEUE_LENGTH;
}

int cf_log_level(cfg_t *cfg, cfg_opt_t *opt, const char *value, void *result)
{
	if (strcmp(value, "off") == 0)
		*(enum loglevel *)result = NONE;
	else if (strcmp(value, "error") == 0)
		*(enum loglevel *)result = ERROR;
	else if (strcmp(value, "info") == 0)
		*(enum loglevel *)result = INFO;
	else if (strcmp(value, "debug") == 0)
		*(enum loglevel *)result = DEBUG;
	else {
		cfg_error(cfg, "Invalid value for option '%s': '%s'", cfg_opt_name(opt),
				value);
		return -1;
	}
	return 0;
}

int cf_validate_num(cfg_t *cfg, cfg_opt_t *opt)
{
	int value = cfg_opt_getnint(opt, 0);
	if (value <= 0) {
		cfg_error(cfg, "'%s' in section '%s' cannot be a negative value.", 
				cfg_opt_name(opt), cfg_name(cfg));
		return -1;
	}
	return 0;
}

int cf_validate_num_zero(cfg_t *cfg, cfg_opt_t *opt)
{
	int value = cfg_opt_getnint(opt, 0);
	if (value < 0) {
		cfg_error(cfg, "'%s' in section '%s' cannot be a negative value.", 
				cfg_opt_name(opt), cfg_name(cfg));
		return -1;
	}
	return 0;
}

static void free_config_files(char **config_files)
{
	short int i;
	
	for (i = 0; i < 3; i++) {
		free(config_files[i]);
	}
}

static int parse_files(cfg_t *cfg)
{
	short int i;
	char *config_files[3], *home;

	home = getenv("HOME");
	
	if (home == NULL) {
		config_files[0] = strdup("");
		config_files[1] = strdup("");
	} else {
		if ((asprintf(&(config_files[0]), "%s/.scmpcrc", home)) == -1)
			exit(EXIT_FAILURE);
		if ((asprintf(&(config_files[1]), "%s/.scmpc/scmpc.conf", home))== -1){
			free(config_files[0]);
			exit(EXIT_FAILURE);
		}
	}
	config_files[2] = strdup(prefs.config_file);
	
	for (i = 0; i < 3; i++)
	{
		if (config_files[i] == NULL) {
			fputs("Out of memory.", stderr);
			exit(EXIT_FAILURE);
		}
			
		switch (cfg_parse(cfg, config_files[i]))
		{
			case CFG_PARSE_ERROR:
				fprintf(stderr, "%s: This configuration file contains errors "
						"and cannot be parsed.\n", config_files[i]);
				free_config_files(config_files);
				exit(EXIT_FAILURE);
			case CFG_FILE_ERROR:
				/* fprintf(stderr, "Config file does not exist (%s)\n",
						prefs.config_file); */
				break;
			case CFG_SUCCESS:
				if (prefs.log_level == DEBUG) {
					printf("Successfully loaded configuration from %s\n", 
							config_files[i]);
				}
				free_config_files(config_files);
				return 1;
			default:
				break;
		}
	}
	free_config_files(config_files);
	return 0;
}

static void parse_config_file(void)
{
	cfg_t *cfg, *sec_as, *sec_mpd;
	
	cfg_opt_t mpd_opts[] = {
		CFG_STR("host", DEFAULT_MPD_HOST, CFGF_NONE),
		CFG_INT("port", DEFAULT_MPD_PORT, CFGF_NONE),
		CFG_INT("timeout", DEFAULT_MPD_TIMEOUT, CFGF_NONE),
		CFG_STR("password", "", CFGF_NONE),
		CFG_END()
	};
	cfg_opt_t as_opts[] = {
		CFG_STR("username", "", CFGF_NONE),
		CFG_STR("password", "", CFGF_NONE),
		CFG_STR("password_hash", "", CFGF_NONE),
		CFG_END()
	};
	cfg_opt_t opts[] = {
		CFG_INT_CB("log_level", DEFAULT_LOG_LEVEL, CFGF_NONE, cf_log_level),
		CFG_STR("log_file", DEFAULT_LOG_FILE, CFGF_NONE),
		CFG_STR("pid_file", DEFAULT_PID_FILE, CFGF_NONE),
		CFG_STR("cache_file", DEFAULT_CACHE_FILE, CFGF_NONE),
		CFG_INT("queue_length", DEFAULT_QUEUE_LENGTH, CFGF_NONE),
		CFG_INT("cache_interval", DEFAULT_CACHE_INTERVAL, CFGF_NONE),
		CFG_SEC("mpd", mpd_opts, CFGF_NONE),
		CFG_SEC("audioscrobbler", as_opts, CFGF_NONE),
		CFG_END()
	};

	cfg = cfg_init(opts, CFGF_NONE);
	cfg_set_validate_func(cfg, "queue_length", cf_validate_num);
	cfg_set_validate_func(cfg, "cache_interval", cf_validate_num_zero);
	cfg_set_validate_func(cfg, "mpd|port", cf_validate_num);
	cfg_set_validate_func(cfg, "mpd|timeout", cf_validate_num);

	if (! parse_files(cfg))
		goto parse_config_files_exit;
	
	free(prefs.log_file);
	free(prefs.pid_file);
	free(prefs.cache_file);
	free(prefs.mpd_hostname);
	free(prefs.mpd_password);
	free(prefs.as_username);
	free(prefs.as_password);
	
	prefs.log_level = cfg_getint(cfg, "log_level");
	prefs.log_file = strdup(cfg_getstr(cfg, "log_file"));
	prefs.pid_file = strdup(cfg_getstr(cfg, "pid_file"));
	prefs.cache_file = strdup(cfg_getstr(cfg, "cache_file"));
	prefs.queue_length = cfg_getint(cfg, "queue_length");
	prefs.cache_interval = cfg_getint(cfg, "cache_interval");

	sec_mpd = cfg_getsec(cfg, "mpd");
	prefs.mpd_hostname = strdup(cfg_getstr(sec_mpd, "host"));
	prefs.mpd_port = cfg_getint(sec_mpd, "port");
	prefs.mpd_timeout = cfg_getint(sec_mpd, "timeout");
	prefs.mpd_password = strdup(cfg_getstr(sec_mpd, "password"));

	sec_as = cfg_getsec(cfg, "audioscrobbler");
	prefs.as_username = strdup(cfg_getstr(sec_as, "username"));
	prefs.as_password = strdup(cfg_getstr(sec_as, "password"));
	prefs.as_password_hash = strdup(cfg_getstr(sec_as, "password_hash"));
	
parse_config_files_exit:
	cfg_free(cfg);
}

static void parse_command_line(int argc, char **argv)
{
	struct arg_lit *debug = arg_lit0("d", "debug", "Log everything.");
	struct arg_lit *quiet = arg_lit0("q", "quiet", "Disable logging.");
	struct arg_file *conf_file = arg_file0("f", "config-file", "<config_file>",
			"The location of the configuration file.");
	struct arg_file *pid_file = arg_file0("i", "pid-file", "<pid_file>",
			"The location of the pid file.");
	struct arg_lit *version = arg_lit0("v", "version", "Print the program "
			"version.");
	struct arg_lit *fork = arg_lit0("n", "foreground", "Run the program in the "
			"foreground rather than as a daemon.");
	struct arg_lit *help = arg_lit0("h", "help", "Print this help and exit.");
	struct arg_end *end = arg_end(10);
	void *argtable[] = {
		debug, quiet, conf_file, pid_file, version, fork, help, end
	};
	const char *progname = "scmpc";
	int n_errors, exit_code;

	if (arg_nullcheck(argtable) != 0) {
		fputs("Insufficient memory to parse command line options.", stderr);
		exit_code = EXIT_FAILURE;
		goto exit;
	}

	n_errors = arg_parse(argc, argv, argtable);

	if (help->count > 0) {
		fprintf(stdout, "Usage: %s ", progname);
		arg_print_syntax(stdout, argtable, "\n\n");
		arg_print_glossary(stdout, argtable, "%s\n\t%s\n");
		exit_code = EXIT_SUCCESS;
		goto exit;
	} else if (version->count > 0) {
		printf("%s version %s\n", progname, PACKAGE_VERSION);
		printf("A multithreaded audioscrobbler client for MPD.\n");
		printf("Copyright 2005-2006 Jonathan Coome <jcoome@gmail.com>\n");
		exit_code = EXIT_SUCCESS;
		goto exit;
	} else if (n_errors > 0) {
		arg_print_errors(stderr, end, progname);
		fputs("\nPlease see the --help option for more details.\n", stderr);
		exit_code = EXIT_FAILURE;
		goto exit;
	} else {
		/* This must be at the top, to avoid any options specified in the
		 * config file overriding those on the command line. */
		if (conf_file->count > 0) {
			free(prefs.config_file);
			prefs.config_file = strdup(conf_file->filename[0]);
			parse_config_file();
		}
		if (pid_file->count > 0) {
			free(prefs.pid_file);
			prefs.pid_file = strdup(conf_file->filename[0]);
		}
		if (quiet->count > 0 && debug->count > 0) {
			fprintf(stderr, "Specifying --debug and --quiet at the same time "
					"makes no sense.\n");
			exit(EXIT_FAILURE);
		} else if (quiet->count > 0) {
			prefs.log_level = NONE;
		} else if (debug->count > 0) {
			prefs.log_level = DEBUG;
		}
		if (fork->count > 0) {
			prefs.fork = FALSE;
		}
	}
	arg_freetable(argtable, 8);
	return;

exit:
	arg_freetable(argtable, 8);
	exit(exit_code);
}

void init_preferences(int argc, char **argv)
{
	set_defaults();

	/* Environment variables? MPD_HOST and MPD_PORT, in particular. */
	parse_config_file();
	parse_command_line(argc, argv);
}

void clear_preferences(void)
{
	free(prefs.mpd_hostname);
	free(prefs.mpd_password);
	free(prefs.config_file);
	free(prefs.log_file);
	free(prefs.pid_file);
	free(prefs.cache_file);
	free(prefs.as_username);
	free(prefs.as_password);
	free(prefs.as_password_hash);
}
