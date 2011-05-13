bin_PROGRAMS = scmpc
man_MANS = scmpc.1

scmpc_SOURCES =	src/audioscrobbler.c src/audioscrobbler.h \
		src/misc.c src/misc.h \
		src/mpd.c src/mpd.h \
		src/preferences.c src/preferences.h \
		src/queue.c src/queue.h \
		src/scmpc.c src/scmpc.h

scmpc_LDADD =	$(confuse_LIBS) \
		$(glib_LIBS) \
		$(libcurl_LIBS)

scmpc_CFLAGS =	$(confuse_CFLAGS) \
		$(glib_CFLAGS) \
		$(libcurl_CFLAGS)

DEFS += -DSYSCONFDIR=\"$(sysconfdir)\" -D_XOPEN_SOURCE=500

dist-hook: ChangeLog

ChangeLog:
	git2cl > ChangeLog

distclean-local:
	rm -f ChangeLog

EXTRA_DIST = scmpc.conf.example scmpc.1.in ChangeLog
