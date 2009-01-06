#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.59])
AC_INIT([scmpc], [0.9.90], [angelos@unkreativ.org])
AC_CONFIG_SRCDIR([config.h.in])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([dist-bzip2])

# Checks for programs.
AC_PROG_CC

# Checks for libraries.
AC_CHECK_LIB([argtable2], [arg_parse])

PKG_CHECK_MODULES([confuse], [libconfuse])
AC_SUBST(libconfuse_LIBS)
AC_SUBST(libconfuse_CFLAGS)

PKG_CHECK_MODULES([libcurl], [libcurl >= 7.10.0])
AC_SUBST(libdaemon_LIBS)
AC_SUBST(libdaemon_CFLAGS)

AM_PATH_LIBGCRYPT(1.4.0)

# Checks for header files.
AC_CHECK_HEADERS([stdlib.h string.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_TYPE_PID_T

# Checks for library functions.
AC_FUNC_FORK
AC_CHECK_FUNCS([select strerror])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
