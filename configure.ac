#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.59])
AC_INIT([scmpc], [0.4.1], [mende.christoph@gmail.com])
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
PKG_CHECK_MODULES([libmpdclient], [libmpdclient >= 2.3])

AC_CONFIG_FILES([Makefile scmpc.1])
AC_OUTPUT
