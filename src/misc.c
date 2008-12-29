/**
 * misc.c: Misc helper functions
 *
 * Copyright (c) 2008 Christoph Mende <angelos@unkreativ.org>
 * All rights reserved. Released under the 2-clause BSD license.
 */


#include <stdio.h>

#include "misc.h"

void open_log(const char *filename)
{
	printf("%s\n",filename);
}

void scmpc_log(enum loglevel level, const char *format, ...)
{
	printf("%d\n",level);
	printf("%s\n",format);
}
