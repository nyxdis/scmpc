
examples_d := $(srcdir)/examples
examples_b := $(builddir)/examples

CONFFILE := $(examples_b)/scmpc.conf

DISTCLEAN += $(CONFFILE)

install: $(CONFFILE)_install
uninstall: $(CONFFILE)_uninstall

.PHONY: $(CONFFILE)_install
$(CONFFILE)_install:
	$(INSTALL) -d $(DESTDIR)$(datadir)/scmpc
	$(INSTALL) -m 644 $(CONFFILE) $(DESTDIR)$(datadir)/scmpc/scmpc.conf

.PHONY: $(CONFFILE)_uninstall
$(CONFFILE)_uninstall:
	rm $(DESTDIR)$(datadir)/scmpc/scmpc.conf
	rmdir $(DESTDIR)$(datadir)/scmpc
