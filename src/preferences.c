/**
 * preferences.c: Preferences parsing
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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <argtable2.h>
#include <confuse.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "misc.h"
#include "preferences.h"

static int cf_log_level(cfg_t *cfg, cfg_opt_t *opt, const char *value, void *result)
{
	if(strncmp(value,"off",3) == 0)
		*(enum loglevel *)result = NONE;
	else if(strncmp(value,"error",5) == 0)
		*(enum loglevel *)result = ERROR;
	else if(strncmp(value,"info",4) == 0)
		*(enum loglevel *)result = INFO;
	else if(strncmp(value,"debug",5) == 0)
		*(enum loglevel *)result = DEBUG;
	else {
		cfg_error(cfg,"Invalid value for option '%s': '%s'",
			cfg_opt_name(opt),value);
		return -1;
	}
	return 0;
}

static int cf_validate_num(cfg_t *cfg, cfg_opt_t *opt)
{
	int value = cfg_opt_getnint(opt,0);
	if(value <= 0) {
		cfg_error(cfg,"'%s' in section '%s' cannot be a negative value"
			" or zero.",
			cfg_opt_name(opt),cfg_name(cfg));
		return -1;
	}
	return 0;
}

static int cf_validate_num_zero(cfg_t *cfg, cfg_opt_t *opt)
{
	int value = cfg_opt_getnint(opt,0);
	if(value < 0) {
		cfg_error(cfg,"'%s' in section '%s' cannot be a negative value.",
			cfg_opt_name(opt),cfg_name(cfg));
		return -1;
	}
	return 0;
}

static void free_config_files(char **config_files)
{
	short int i;
	for(i=0;i<3;i++)
		free(config_files[i]);
}

static int parse_files(cfg_t *cfg)
{
	short int i;
	char *config_files[3], *home;

	home = getenv("HOME");

	if(home == NULL) {
		config_files[0] = strdup("");
		config_files[1] = strdup("");
	} else {
		if((asprintf(&(config_files[0]),"%s/.scmpcrc",home)) == -1)
			return -1;
		if((asprintf(&(config_files[1]),"%s/.scmpc/scmpc.conf",home)) == -1){
			free(config_files[0]);
			return -1;
		}
	}
	config_files[2] = strdup(SYSCONFDIR "/scmpc.conf");

	for(i=0;i<3;i++)
	{
		if(config_files[i] == NULL)
			return -1;

		switch(cfg_parse(cfg,config_files[i]))
		{
			case CFG_PARSE_ERROR:
				fprintf(stderr,"%s: This configuration file "
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

static int parse_config_file(void)
{
	cfg_t *cfg, *sec_as, *sec_mpd;

	cfg_opt_t mpd_opts[] = {
		CFG_STR("host","localhost",CFGF_NONE),
		CFG_INT("port",6600,CFGF_NONE),
		CFG_INT("timeout",5,CFGF_NONE),
		CFG_STR("password","",CFGF_NONE),
		CFG_END()
	};
	cfg_opt_t as_opts[] = {
		CFG_STR("username","",CFGF_NONE),
		CFG_STR("password","",CFGF_NONE),
		CFG_STR("password_hash","",CFGF_NONE),
		CFG_END()
	};
	cfg_opt_t opts[] = {
		CFG_INT_CB("log_level",ERROR,CFGF_NONE,cf_log_level),
		CFG_STR("log_file","/var/log/scmpc.log",CFGF_NONE),
		CFG_STR("pid_file","/var/run/scmpc.pid",CFGF_NONE),
		CFG_STR("cache_file","/var/lib/scmpc/scmpc.cache",CFGF_NONE),
		CFG_INT("queue_length",500,CFGF_NONE),
		CFG_INT("cache_interval",10,CFGF_NONE),
		CFG_SEC("mpd",mpd_opts,CFGF_NONE),
		CFG_SEC("audioscrobbler",as_opts,CFGF_NONE),
		CFG_END()
	};

	cfg = cfg_init(opts,CFGF_NONE);
	cfg_set_validate_func(cfg,"queue_length",cf_validate_num);
	cfg_set_validate_func(cfg,"cache_interval",cf_validate_num_zero);
	cfg_set_validate_func(cfg,"mpd|port",cf_validate_num);
	cfg_set_validate_func(cfg,"mpd|timeout",cf_validate_num);

	if(parse_files(cfg) < 0) {
		cfg_free(cfg);
		return -1;
	}

	free(prefs.log_file);
	free(prefs.pid_file);
	free(prefs.cache_file);
	free(prefs.mpd_hostname);
	free(prefs.mpd_password);
	free(prefs.as_username);
	free(prefs.as_password);
	free(prefs.as_password_hash);

	prefs.log_level = cfg_getint(cfg,"log_level");
	prefs.log_file = strdup(cfg_getstr(cfg,"log_file"));
	prefs.pid_file = strdup(cfg_getstr(cfg,"pid_file"));
	prefs.cache_file = strdup(cfg_getstr(cfg,"cache_file"));
	prefs.queue_length = cfg_getint(cfg,"queue_length");
	prefs.cache_interval = cfg_getint(cfg,"cache_interval");

	sec_mpd = cfg_getsec(cfg,"mpd");
	prefs.mpd_hostname = strdup(cfg_getstr(sec_mpd,"host"));
	prefs.mpd_port = cfg_getint(sec_mpd,"port");
	prefs.mpd_timeout = cfg_getint(sec_mpd,"timeout");
	prefs.mpd_password = strdup(cfg_getstr(sec_mpd,"password"));

	sec_as = cfg_getsec(cfg,"audioscrobbler");
	prefs.as_username = strdup(cfg_getstr(sec_as,"username"));
	prefs.as_password = strdup(cfg_getstr(sec_as,"password"));
	prefs.as_password_hash = strdup(cfg_getstr(sec_as,"password_hash"));

	prefs.fork = 1;

	cfg_free(cfg);
	return 0;
}

static int parse_command_line(int argc, char **argv)
{
	struct arg_lit *debug = arg_lit0("d","debug","Log everything.");
	struct arg_lit *quiet = arg_lit0("q","quiet","Disable logging.");
	struct arg_file *conf_file = arg_file0("f","config-file","<config_file>",
			"The location of the configuration file.");
	struct arg_file *pid_file = arg_file0("i","pid-file","<pid_file>",
			"The location of the pid file.");
	struct arg_lit *version = arg_lit0("v","version","Print the program "
			"version.");
	struct arg_lit *fork = arg_lit0("n","foreground","Run the program in the "
			"foreground rather than as a daemon.");
	struct arg_lit *help = arg_lit0("h","help","Print this help and exit.");
	struct arg_end *end = arg_end(10);
	void *argtable[] = {
		debug, quiet, conf_file, pid_file, version, fork, help, end
	};
	int n_errors;

	if (arg_nullcheck(argtable) != 0) {
		fputs("Insufficient memory to parse command line options.",stderr);
		arg_freetable(argtable,8);
		return -1;
	}

	n_errors = arg_parse(argc,argv,argtable);

	if (help->count > 0) {
		fprintf(stdout,"Usage: %s ",PACKAGE_NAME);
		arg_print_syntax(stdout,argtable,"\n\n");
		arg_print_glossary(stdout,argtable,"%s\n\t%s\n");
		arg_freetable(argtable,8);
		exit(EXIT_SUCCESS);
	} else if (version->count > 0) {
		printf("%s\n",PACKAGE_STRING);
		printf("A multithreaded audioscrobbler client for MPD.\n");
		printf("Copyright 2009 Christoph Mende <angelos@unkreativ.org>\n");
		printf("Based on Jonathan Coome's work on scmpc\n");
		arg_freetable(argtable,8);
		exit(EXIT_SUCCESS);
	} else if (n_errors > 0) {
		arg_print_errors(stderr,end,PACKAGE_NAME);
		fputs("\nPlease see the --help option for more details.\n",stderr);
		arg_freetable(argtable,8);
		return -1;
	} else {
		/* This must be at the top, to avoid any options specified in the
		 * config file overriding those on the command line. */
		if (conf_file->count > 0) {
			free(prefs.config_file);
			prefs.config_file = strdup(conf_file->filename[0]);
			if(parse_config_file() < 0)
				return -1;
		}
		if (pid_file->count > 0) {
			free(prefs.pid_file);
			prefs.pid_file = strdup(pid_file->filename[0]);
		}
		if (quiet->count > 0 && debug->count > 0) {
			fprintf(stderr,"Specifying --debug and --quiet at the same time "
					"makes no sense.\n");
			return -1;
		} else if (quiet->count > 0) {
			prefs.log_level = NONE;
		} else if (debug->count > 0) {
			prefs.log_level = DEBUG;
		}
		if (fork->count > 0) {
			prefs.fork = 0;
		}
	}
	arg_freetable(argtable,8);
	return 0;
}

int init_preferences(int argc, char **argv)
{
	char *tmp, *saveptr;

	if(parse_config_file() < 0)
		return -1;
	if(parse_command_line(argc,argv) < 0)
		return -1;

	tmp = getenv("MPD_HOST");
	if(tmp != NULL) {
		free(prefs.mpd_password);
		free(prefs.mpd_hostname);
		if(strstr(tmp,"@")) {
			prefs.mpd_password = strdup(strtok_r(tmp,"@",&saveptr));
			prefs.mpd_hostname = strdup(strtok_r(NULL,"@",&saveptr));
		} else {
			prefs.mpd_password = strdup("");
			prefs.mpd_hostname = strdup(tmp);
		}
	}
	if(getenv("MPD_PORT") != NULL)
		prefs.mpd_port = atoi(getenv("MPD_PORT"));

	return 0;
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
