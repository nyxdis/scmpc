
man_d := $(srcdir)/man
man_b := $(builddir)/man
VPATH += $(man_d)

MANPAGE_SRC := scmpc.man
MANPAGE := $(patsubst %.man,$(man_b)/%.1,$(MANPAGE_SRC))

CLEAN += $(MANPAGE)

targets: $(MANPAGE)
install: $(MANPAGE)_install
uninstall: $(MANPAGE)_uninstall

# The INSTALL line is there to create the build directory if it doens't exist,
# e.g $(b) != $(d). I use $(INSTALL) due to mkdir -p not being portable
# (although I probably wouldn't need -p in this case).
$(MANPAGE): $(MANPAGE_SRC)
	@( if [ ! -d $(man_b) ]; then $(INSTALL) -d $(man_b); fi )
	sed -e 's:@sysconfdir@:$(sysconfdir):' $< > $@

.PHONY: $(MANPAGE)_install
$(MANPAGE)_install:
	$(INSTALL) -d $(DESTDIR)$(man1dir)
	$(INSTALL) -m 644 $(MANPAGE) $(DESTDIR)$(man1dir)/scmpc.1

.PHONY: $(MANPAGE)_uninstall
$(MANPAGE)_uninstall:
	rm $(DESTDIR)$(man1dir)/scmpc.1
