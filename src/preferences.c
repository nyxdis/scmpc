/**
 * preferences.c: Preferences parsing
 *
 * Copyright (c) 2008 Christoph Mende <angelos@unkreativ.org>
 * All rights reserved. Released under the 2-clause BSD license.
 *
 * Based on Jonathan Coome's work on scmpc
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <confuse.h>

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
		cfg_error(cfg,"'%s' in section '%s' cannot be a nagative value"
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
		cfg_error(cfg,"'%s' in section '%s' cannot be a nagative value.",
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
	char *home, *config_files[3];

	home = getenv("HOME");

	if(home == NULL) {
		config_files[0] = strdup("");
		config_files[1] = strdup("");
	} else {
		if((asprintf(&(config_files[0]),"%s/.scmpcrc",home)) == -1)
			exit(EXIT_FAILURE);
		if((asprintf(&(config_files[1]),"%s/.scmpc/scmpc.conf",home)) == -1) {
			free(config_files[0]);
			exit(EXIT_FAILURE);
		}
	}
	config_files[2] = strdup(SYSCONFDIR "/scmpc.conf");

	for(i=0;i<3;i++)
	{
		if(config_files[i] == NULL)
			exit(EXIT_FAILURE);

		switch(cfg_parse(cfg,config_files[i]))
		{
			case CFG_PARSE_ERROR:
				fprintf(stderr,"%s: This configuration file "
				"contains errors and cannot be parsed.\n",
				config_files[i]);
				free_config_files(config_files);
				return 1;
			case CFG_SUCCESS:
				free_config_files(config_files);
				return 0;
			default:
				return 1;
		}
	}
	free_config_files(config_files);
	return 0;
}

void init_preferences(int argc, char *argv[])
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

	if(parse_files(cfg) != 0) {
		cfg_free(cfg);
		return;
	}

	sec_as = NULL;
	sec_mpd = NULL;
	printf("%p%p\n",(void*)sec_as,(void*)sec_mpd);

	free(prefs.log_file);
	free(prefs.pid_file);
	free(prefs.cache_file);
	free(prefs.mpd_hostname);
	free(prefs.as_username);
	free(prefs.as_password);
	free(prefs.as_password_hash);

	printf("%d %s",argc,argv[0]);
}
