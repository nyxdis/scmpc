AM_CPPFLAGS =	@libcurl_CFLAGS@ \
		@confuse_CFLAGS@

bin_PROGRAMS = scmpc
dist_man_MANS = scmpc.1

scmpc_SOURCES =	src/audioscrobbler.c src/audioscrobbler.h \
		src/md5.c src/md5.h \
		src/mpd.c src/mpd.h \
		src/misc.c src/misc.h \
		src/preferences.c src/preferences.h \
		src/scmpc.c src/scmpc.h

scmpc_LDADD = 	@libcurl_LIBS@ \
		@confuse_LIBS@

DEFS += -DSYSCONFDIR=\"$(sysconfdir)\" -D_GNU_SOURCE

EXTRA_DIST = scmpc.conf.example
