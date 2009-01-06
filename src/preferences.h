/**
 * preferences.h: Preferences parsing
 *
 * Copyright (c) 2008 Christoph Mende <angelos@unkreativ.org>
 * All rights reserved. Released under the 2-clause BSD license.
 *
 * Based on Jonathan Coome's work on scmpc
 */


#include <stdbool.h>

struct preferences {
        char *mpd_hostname;
        int mpd_port;
        int mpd_timeout;
        char *mpd_password;
        bool fork;
        enum loglevel log_level;
        char *config_file;
        char *log_file;
        char *pid_file;
        char *as_username;
        char *as_password;
        char *as_password_hash;
        char *cache_file;
        int queue_length;
        int cache_interval;
} prefs;

void init_preferences(int argc, char *argv[]);
