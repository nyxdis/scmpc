bin_PROGRAMS = scmpc
scmpc_SOURCES =	src/audioscrobbler.c src/audioscrobbler.h \
		src/misc.c src/misc.h \
		src/md5.c src/md5.h \
		src/mpd.c src/mpd.h \
		src/preferences.c src/preferences.h \
		src/scmpc.c src/scmpc.h

scmpc_LDADD = 	@libcurl_LIBS@ \
		@libconfuse_LIBS@
