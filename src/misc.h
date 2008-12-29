/**
 * misc.h: Misc helper functions
 *
 * Copyright (c) 2008 Christoph Mende <angelos@unkreativ.org>
 * All rights reserved. Released under the 2-clause BSD license.
 */


#include <stdbool.h>

enum loglevel {
	NONE = 0,
	ERROR = 1,
	INFO = 2,
	DEBUG = 3
};

enum connection_status {
	CONNECTED,
	DISCONNECTED,
	BADAUTH
};

void open_log(const char *filename);
void scmpc_log(enum loglevel, const char *format, ...);
