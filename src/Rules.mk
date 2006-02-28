
src_d := $(srcdir)/src
src_b := $(builddir)/src

VPATH += $(src_d)

SRC := audioscrobbler.c liberror.c libmpd.c md5.c misc.c mpd.c preferences.c \
		scmpc.c
OBJS := $(patsubst %.c,$(src_b)/%.o,$(SRC))
PROGRAM := $(src_b)/scmpc
DEPENDS := $(src_d)/depends.mk

-include $(DEPENDS)

DEFINES := -DHAVE_CONFIG_H
INCLUDES := -I$(src_d) -I$(src_b)
LDLIBS := $(LIBS)

CLEAN += $(OBJS) $(PROGRAM) $(DEPENDS)
DISTCLEAN += $(src_b)/config.h $(src_b)/stamp-h1

targets: $(DEPENDS) $(PROGRAM)
install: $(PROGRAM)_install
uninstall: $(PROGRAM)_uninstall

.PHONY: depend
depend: $(SRC) $(SRC:.c=.h)
#	touch $@
	$(srcdir)/depends.sh 'src_b' $(patsubst %,$(src_d)/%,$(SRC)) > $(DEPENDS)

$(PROGRAM): $(OBJS)
	$(CC) $(LDFLAGS) $(LDLIBS) $(OBJS) -o $@

$(PROGRAM)_install: $(PROGRAM)
	$(INSTALL) -d $(DESTDIR)$(bindir)
	$(INSTALL) -m 755 $(PROGRAM) $(DESTDIR)$(bindir)/scmpc

.PHONY: $(PROGRAM)_uninstall
$(PROGRAM)_uninstall:
	rm $(DESTDIR)$(bindir)/scmpc
	
# Object specific variables
$(src_b)/preferences.o: DEFINES += -DSYSCONFDIR='"$(sysconfdir)"'
$(src_b)/preferences.o: BUILD_CFLAGS := $(BUILD_CFLAGS:-pedantic=)
