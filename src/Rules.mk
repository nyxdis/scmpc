
src_d := $(srcdir)/src
src_b := $(builddir)/src

VPATH += $(src_d)

SRC_FILES := $(wildcard $(src_d)/*.c)
SRC := audioscrobbler.c liberror.c libmpd.c md5.c misc.c mpd.c preferences.c \
		scmpc.c

OBJS := $(patsubst %.c,$(src_b)/%.o,$(SRC))

PROGRAM := $(src_b)/scmpc
DEPENDS := $(src_d)/depends.mk

-include $(DEPENDS)

DEFINES := -DHAVE_CONFIG_H
INCLUDES := -I$(src_d) -I$(src_b)
LDLIBS := $(LIBS)

CLEAN += $(OBJS) $(PROGRAM)
DISTCLEAN += $(src_b)/config.h $(src_b)/stamp-h1
MAINTAINER_CLEAN += $(DEPENDS)

targets: $(PROGRAM)
install: $(PROGRAM)_install
uninstall: $(PROGRAM)_uninstall

.PHONY: depend
depend: $(SRC_FILES) $(SRC_FILES:.c=.h)
	$(srcdir)/depends.sh 'src_b' $(patsubst %,$(src_d)/%,$(SRC_FILES)) > $(DEPENDS)

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
