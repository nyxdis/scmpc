AM_CPPFLAGS =	@libcurl_CFLAGS@ \
		@confuse_CFLAGS@

bin_PROGRAMS = scmpc

scmpc_SOURCES =	src/audioscrobbler.c src/audioscrobbler.h \
		src/misc.c src/misc.h \
		src/preferences.c src/preferences.h \
		src/scmpc.c src/scmpc.h

scmpc_LDADD = 	@libcurl_LIBS@ \
		@confuse_LIBS@

DEFS += -DSYSCONFDIR=\"$(sysconfdir)\"
