
# Standard targets:

all: targets

install: all

uninstall:

install-strip:

.PHONY: clean
clean:
	rm -fr $(CLEAN)

.PHONY: distclean
distclean: clean
	rm -fr $(DISTCLEAN)

.PHONY: mostly-clean
mostly-clean: clean

.PHONY: maintainer-clean
maintainer-clean: distclean
	rm -fr $(MAINTAINER_CLEAN)

.PHONY: dist
dist: depend clean
	@echo Creating $(program_name_version).tar.bz2
	@rm -f ./$(program_name_version).tar.bz2
	@rm -f ./$(program_name_version)
	@ln -s ../$(shell basename $${PWD}) $(program_name_version)
	@tar -cjvf ./$(program_name_version).tar.bz2 --dereference --wildcards \
		--exclude "autom4te.cache" --exclude "*.swp" --exclude "scmpc.conf" \
		--exclude "config.log" --exclude "config.status" --exclude "Makefile" \
		--exclude "config.h" --exclude "stamp-h1" --exclude "*.o" \
		--exclude ".svn" --exclude $(program_name_version).tar.bz2 \
		--exclude $(program_name_version)/$(program_name_version) \
		./$(program_name_version)/
	@rm -f ./$(program_name_version)

check:
	@echo Currently there are no tests for this program...

# Default rules

# Compiling C files to objects
%.o: %.c
	$(CC) $(BUILD_CFLAGS) $(INCLUDES) $(DEFINES) -c $< -o $@

# Linking single objects
%: %.o
	$(CC) $(LDFLAGS) $(LDLIBS) -c $< -o $@

# Subdirectories
include $(srcdir)/src/Rules.mk
include $(srcdir)/man/Rules.mk
include $(srcdir)/examples/Rules.mk
