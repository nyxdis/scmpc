/**
 * preferences.c: Preferences parsing
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


#include <stdlib.h>
#include <string.h>

#include <confuse.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "scmpc.h"
#include "misc.h"
#include "preferences.h"

static gint cf_log_level(cfg_t *cfg, cfg_opt_t *opt, const char *value, void *result)
{
	if (!strncmp(value, "none", 4))
		*(loglevel *)result = NONE;
	else if (!strncmp(value, "error", 5))
		*(loglevel *)result = ERROR;
	else if (!strncmp(value, "info", 4))
		*(loglevel *)result = INFO;
	else if (!strncmp(value, "debug", 5))
		*(loglevel *)result = DEBUG;
	else {
		cfg_error(cfg, "Invalid value for option '%s': '%s'",
			cfg_opt_name(opt), value);
		return -1;
	}
	return 0;
}

static gint cf_validate_num(cfg_t *cfg, cfg_opt_t *opt)
{
	gint value = cfg_opt_getnint(opt, 0);
	if (value <= 0) {
		cfg_error(cfg, "'%s' in section '%s' cannot be a negative value"
			" or zero.",
			cfg_opt_name(opt), cfg_name(cfg));
		return -1;
	}
	return 0;
}

static gint cf_validate_num_zero(cfg_t *cfg, cfg_opt_t *opt)
{
	gint value = cfg_opt_getnint(opt, 0);
	if (value < 0) {
		cfg_error(cfg, "'%s' in section '%s' cannot be a negative value.",
			cfg_opt_name(opt), cfg_name(cfg));
		return -1;
	}
	return 0;
}

static void free_config_files(gchar **config_files)
{
	gshort i;
	for (i=0;i<3;i++)
		g_free(config_files[i]);
}

static gint parse_files(cfg_t *cfg)
{
	gchar *config_files[3];
	const gchar *home;

	if (!prefs.config_file) {
		if (!(home = g_getenv("HOME")))
			home = g_get_home_dir();

		config_files[0] = g_strdup_printf("%s/.scmpcrc", home);
		config_files[1] = g_strdup_printf("%s/.scmpc/scmpc.conf", home);
		config_files[2] = g_strdup(SYSCONFDIR "/scmpc.conf");
	} else {
		config_files[0] = prefs.config_file;
		config_files[1] = g_strdup("");
		config_files[2] = g_strdup("");
	}

	for (int i = 0; i < 3; i++)
	{
		switch(cfg_parse(cfg, config_files[i]))
		{
			case CFG_PARSE_ERROR:
				fprintf(stderr, "%s: This configuration file "
				"contains errors and cannot be parsed.\n",
				config_files[i]);
				free_config_files(config_files);
				return -1;
			case CFG_FILE_ERROR:
				break;
			case CFG_SUCCESS:
				free_config_files(config_files);
				return 0;
			default:
				free_config_files(config_files);
				return -1;
		}
	}
	return 0;
}

static char* expand_tilde(const char *path)
{
	if (path[0] == '~') {
		const char *home = getenv("HOME");
		if (!home)
			home = g_get_home_dir();

		return g_strconcat(home, &path[1], NULL);
	}

	return g_strdup(path);
}

static gint parse_config_file(void)
{
	cfg_t *cfg, *sec_as, *sec_mpd;

	cfg_opt_t mpd_opts[] = {
		CFG_STR("host", "localhost", CFGF_NONE),
		CFG_INT("port", 6600, CFGF_NONE),
		CFG_INT("timeout", 5, CFGF_NONE),
		CFG_INT("interval", 10, CFGF_NONE),
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
		CFG_INT_CB("log_level", ERROR, CFGF_NONE, &cf_log_level),
		CFG_STR("log_file", "/var/log/scmpc.log", CFGF_NONE),
		CFG_STR("pid_file", "/var/run/scmpc.pid", CFGF_NONE),
		CFG_STR("cache_file", "/var/lib/scmpc/scmpc.cache", CFGF_NONE),
		CFG_INT("queue_length", 500, CFGF_NONE),
		CFG_INT("cache_interval", 10, CFGF_NONE),
		CFG_SEC("mpd", mpd_opts, CFGF_NONE),
		CFG_SEC("audioscrobbler", as_opts, CFGF_NONE),
		CFG_END()
	};

	cfg = cfg_init(opts, CFGF_NONE);
	cfg_set_validate_func(cfg, "queue_length", &cf_validate_num);
	cfg_set_validate_func(cfg, "cache_interval", &cf_validate_num_zero);
	cfg_set_validate_func(cfg, "mpd|port", &cf_validate_num);
	cfg_set_validate_func(cfg, "mpd|timeout", &cf_validate_num);
	cfg_set_validate_func(cfg, "mpd|interval", &cf_validate_num);

	if (parse_files(cfg) < 0) {
		cfg_free(cfg);
		return -1;
	}

	g_free(prefs.log_file);
	g_free(prefs.pid_file);
	g_free(prefs.cache_file);
	g_free(prefs.mpd_hostname);
	g_free(prefs.mpd_password);
	g_free(prefs.as_username);
	g_free(prefs.as_password);
	g_free(prefs.as_password_hash);

	prefs.log_level = cfg_getint(cfg, "log_level");
	prefs.log_file = expand_tilde(cfg_getstr(cfg, "log_file"));
	prefs.pid_file = expand_tilde(cfg_getstr(cfg, "pid_file"));
	prefs.cache_file = expand_tilde(cfg_getstr(cfg, "cache_file"));
	prefs.queue_length = cfg_getint(cfg, "queue_length");
	prefs.cache_interval = cfg_getint(cfg, "cache_interval");

	sec_mpd = cfg_getsec(cfg, "mpd");
	prefs.mpd_hostname = g_strdup(cfg_getstr(sec_mpd, "host"));
	prefs.mpd_port = cfg_getint(sec_mpd, "port");
	prefs.mpd_timeout = cfg_getint(sec_mpd, "timeout");
	prefs.mpd_interval = cfg_getint(sec_mpd, "interval") * 1000;
	prefs.mpd_password = g_strdup(cfg_getstr(sec_mpd, "password"));

	sec_as = cfg_getsec(cfg, "audioscrobbler");
	prefs.as_username = g_strdup(cfg_getstr(sec_as, "username"));
	prefs.as_password = g_strdup(cfg_getstr(sec_as, "password"));
	prefs.as_password_hash = g_strdup(cfg_getstr(sec_as, "password_hash"));

	prefs.fork = TRUE;

	cfg_free(cfg);
	return 0;
}

static gint parse_command_line(gint argc, gchar **argv)
{
	GError *error = NULL;
	gchar *pid_file = NULL, *conf_file = NULL;
	gboolean dokill = FALSE, debug = FALSE, quiet = FALSE, version = FALSE, fork = TRUE;
	GOptionEntry entries[] = {
		{ "debug", 'd', 0, G_OPTION_ARG_NONE, &debug, "Log everything.", NULL },
		{ "kill", 'k', 0, G_OPTION_ARG_NONE, &dokill, "Kill the running scmpc", NULL },
		{ "quiet", 'q', 0, G_OPTION_ARG_NONE, &quiet, "Disable logging.", NULL },
		{ "config-file", 'f', 0, G_OPTION_ARG_FILENAME, &conf_file, "The location of the configuration file.", "<config_file>" },
		{ "pid-file", 'i', 0, G_OPTION_ARG_FILENAME, &pid_file, "The location of the pid file.", "<pid_file>" },
		{ "version", 'v', 0, G_OPTION_ARG_NONE, &version, "Print the program version.", NULL },
		{ "foreground", 'n', G_OPTION_FLAG_REVERSE, G_OPTION_ARG_NONE, &fork, "Run the program in the foreground rather than as a daemon.", NULL },
		{ NULL, 0, 0, 0, NULL, NULL, NULL }
	};

	GOptionContext *context = g_option_context_new(NULL);
	g_option_context_add_main_entries(context, entries, NULL);
	if (!g_option_context_parse(context, &argc, &argv, &error)) {
		g_print("%s\n", error->message);
		g_option_context_free(context);
		return -1;
	}
	g_option_context_free(context);

	if (version) {
		puts(PACKAGE_STRING);
		puts("An Audioscrobbler client for MPD.");
		puts("Copyright 2009-2011 Christoph Mende <angelos@unkreativ.org>");
		puts("Based on Jonathan Coome's work on scmpc");
		exit(EXIT_SUCCESS);
	}
	/* This must be at the top, to avoid any options specified in the
	 * config file overriding those on the command line. */
	if (conf_file) {
		g_free(prefs.config_file);
		prefs.config_file = g_strdup(conf_file);
		if (parse_config_file() < 0)
			return -1;
	}
	if (pid_file) {
		g_free(prefs.pid_file);
		prefs.pid_file = g_strdup(pid_file);
	}
	if (quiet && debug) {
		fputs("Specifying --debug and --quiet at the same time makes no sense.", stderr);
		return -1;
	} else if (quiet)
		prefs.log_level = NONE;
	else if (debug)
		prefs.log_level = DEBUG;
	if (!fork)
		prefs.fork = FALSE;
	if (dokill)
		kill_scmpc();
	g_free(pid_file);
	g_free(conf_file);
	return 0;
}

gint init_preferences(gint argc, gchar **argv)
{
	gchar *tmp, *saveptr;

	prefs.config_file = NULL;
	if (parse_config_file() < 0)
		return -1;
	if (parse_command_line(argc, argv) < 0)
		return -1;

	tmp = getenv("MPD_HOST");
	if (tmp) {
		g_free(prefs.mpd_password);
		g_free(prefs.mpd_hostname);
		if (g_strrstr(tmp, "@")) {
			prefs.mpd_password = g_strdup(strtok_r(tmp, "@", &saveptr));
			prefs.mpd_hostname = g_strdup(strtok_r(NULL, "@", &saveptr));
		} else {
			prefs.mpd_password = g_strdup("");
			prefs.mpd_hostname = g_strdup(tmp);
		}
	}
	if (getenv("MPD_PORT"))
		prefs.mpd_port = strtol(getenv("MPD_PORT"), NULL, 10);

	return 0;
}

void clear_preferences(void)
{
	g_free(prefs.mpd_hostname);
	g_free(prefs.mpd_password);
	g_free(prefs.config_file);
	g_free(prefs.log_file);
	g_free(prefs.pid_file);
	g_free(prefs.cache_file);
	g_free(prefs.as_username);
	g_free(prefs.as_password);
	g_free(prefs.as_password_hash);
}
