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

TESTS = tests/test_misc tests/test_queue tests/test_audioscrobbler
check_PROGRAMS = $(TESTS)

test_cflags = $(scmpc_CFLAGS) -I$(srcdir)/src

tests_test_misc_SOURCES = tests/test_misc.c src/misc.c
tests_test_misc_CFLAGS = $(test_cflags)
tests_test_misc_LDADD = $(scmpc_LDADD)

tests_test_queue_SOURCES = tests/test_queue.c src/queue.c src/misc.c
tests_test_queue_CFLAGS = $(test_cflags)
tests_test_queue_LDADD = $(scmpc_LDADD)

# includes src/audioscrobbler.c to reach its static functions
tests_test_audioscrobbler_SOURCES = tests/test_audioscrobbler.c \
		src/queue.c src/misc.c
tests_test_audioscrobbler_CFLAGS = $(test_cflags)
tests_test_audioscrobbler_LDADD = $(scmpc_LDADD)
EXTRA_tests_test_audioscrobbler_DEPENDENCIES = src/audioscrobbler.c

DEFS += -DSYSCONFDIR=\"$(sysconfdir)\" -D_XOPEN_SOURCE=500

dist-hook: ChangeLog

ChangeLog:
	git -C $(srcdir) log --date=short \
		--format='%ad  %an  <%ae>%n%n%w(72,8,8)%s%n' > $@

distclean-local:
	rm -f ChangeLog

EXTRA_DIST = scmpc.conf.example scmpc.1.in ChangeLog README.md
