AM_CPPFLAGS =	@libcurl_CFLAGS@ \
		@confuse_CFLAGS@ \
		@glib_CFLAGS@

bin_PROGRAMS = scmpc
man_MANS = scmpc.1

scmpc_SOURCES =	src/audioscrobbler.c src/audioscrobbler.h \
		src/misc.c src/misc.h \
		src/mpd.c src/mpd.h \
		src/preferences.c src/preferences.h \
		src/queue.c src/queue.h \
		src/scmpc.c src/scmpc.h

scmpc_LDADD = 	@libcurl_LIBS@ \
		@confuse_LIBS@ \
		@glib_LIBS@

DEFS += -DSYSCONFDIR=\"$(sysconfdir)\" -D_XOPEN_SOURCE=500

dist-hook: ChangeLog

ChangeLog:
	git2cl > ChangeLog

distclean-local:
	rm -f ChangeLog

EXTRA_DIST = scmpc.conf.example scmpc.1.in ChangeLog
