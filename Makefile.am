AM_CPPFLAGS =	@libcurl_CFLAGS@ \
		@confuse_CFLAGS@ \
		@libdaemon_CFLAGS@

bin_PROGRAMS = scmpc
man_MANS = scmpc.1

scmpc_SOURCES =	src/audioscrobbler.c src/audioscrobbler.h \
		src/exception.h \
		src/libmpd.c src/libmpd.h \
		src/md5.c src/md5.h \
		src/misc.c src/misc.h \
		src/mpd.c src/mpd.h \
		src/preferences.c src/preferences.h \
		src/scmpc.c src/scmpc.h

scmpc_LDADD = 	@libcurl_LIBS@ \
		@confuse_LIBS@ \
		@libdaemon_LIBS@

DEFS += -DSYSCONFDIR=\"$(sysconfdir)\" -D_GNU_SOURCE

EXTRA_DIST = scmpc.conf.in scmpc.1.in DEPENDENCIES
