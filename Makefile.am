bin_PROGRAMS = scmpc
man_MANS = scmpc.1

scmpc_SOURCES =	src/audioscrobbler.c src/audioscrobbler.h \
		src/mpd.c src/mpd.h \
		src/misc.c src/misc.h \
		src/preferences.c src/preferences.h \
		src/queue.c src/queue.h \
		src/scmpc.c src/scmpc.h

scmpc_LDADD =	$(glib_LIBS) \
		$(confuse_LIBS) \
		$(curl_LIBS) \
		$(libmpdclient_LIBS)

scmpc_CFLAGS =	$(glib_CFLAGS) \
		$(confuse_CFLAGS) \
		$(curl_CFLAGS) \
		$(libmpdclient_CFLAGS)

DEFS += -DSYSCONFDIR=\"$(sysconfdir)\" -D_XOPEN_SOURCE=500

dist-hook: ChangeLog

ChangeLog:
	git2cl > ChangeLog

distclean-local:
	rm -f ChangeLog

EXTRA_DIST = scmpc.conf.example scmpc.1.in ChangeLog README.md
