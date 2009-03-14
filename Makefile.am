AM_CPPFLAGS =	@libcurl_CFLAGS@ \
		@confuse_CFLAGS@ \
		@glib_CFLAGS@

bin_PROGRAMS = scmpc
dist_man_MANS = scmpc.1

scmpc_SOURCES =	src/audioscrobbler.c src/audioscrobbler.h \
		src/misc.c src/misc.h \
		src/mpd.c src/mpd.h \
		src/preferences.c src/preferences.h \
		src/queue.c src/queue.h \
		src/scmpc.c src/scmpc.h

scmpc_LDADD = 	@libcurl_LIBS@ \
		@confuse_LIBS@ \
		@glib_LIBS@

DEFS += -DSYSCONFDIR=\"$(sysconfdir)\" -D_POSIX_SOURCE -D_BSD_SOURCE

EXTRA_DIST = scmpc.conf.example
