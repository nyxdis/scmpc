#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.59])
AC_INIT([scmpc], [0.4.0], [angelos@unkreativ.org])
AC_CONFIG_SRCDIR([config.h.in])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([dist-bzip2 foreign])

# Checks for programs.
AC_PROG_CC
AC_PROG_CC_C99

# Checks for libraries.
PKG_PROG_PKG_CONFIG([0.24])
PKG_CHECK_MODULES([glib], [glib-2.0 >= 2.16])
PKG_CHECK_MODULES([confuse], [libconfuse])
PKG_CHECK_MODULES([curl], [libcurl >= 7.15.4])
PKG_CHECK_MODULES([libmpdclient], [libmpdclient >= 2.4])

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h stdlib.h string.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_PID_T

# Checks for library functions.
AC_FUNC_FORK
AC_CHECK_FUNCS([strcspn strrchr strstr strtol])

AC_CONFIG_FILES([Makefile scmpc.1])
AC_OUTPUT
