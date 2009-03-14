#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.59])
AC_INIT([scmpc], [0.14.0], [angelos@unkreativ.org])
AC_CONFIG_SRCDIR([config.h.in])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([dist-bzip2 foreign])

# Checks for programs.
AC_PROG_CC

# Checks for libraries.
PKG_CHECK_MODULES([glib], [glib-2.0 >= 2.16])
AC_SUBST(glib_LIBS)
AC_SUBST(glib_CFLAGS)

PKG_CHECK_MODULES([confuse], [libconfuse])
AC_SUBST(confuse_LIBS)
AC_SUBST(confuse_CFLAGS)

PKG_CHECK_MODULES([libcurl], [libcurl >= 7.15.4])
AC_SUBST(libdaemon_LIBS)
AC_SUBST(libdaemon_CFLAGS)

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h netdb.h stdlib.h string.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_PID_T

# Checks for library functions.
AC_FUNC_FORK
AC_CHECK_FUNCS([memset select socket strtol])

AC_CONFIG_FILES([Makefile scmpc.1])
AC_OUTPUT
