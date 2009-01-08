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

	if(!parse_files(cfg)) {
		cfg_free(cfg);
		return;
	}

	free(prefs.log_file);
	free(prefs.pid_file);
	free(prefs.cache_file);
	free(prefs.mpd_hostname);
	free(prefs.as_username);
	free(prefs.as_password);
	free(prefs.as_password_hash);

	printf("%d %s",argc,argv[0]);
}
