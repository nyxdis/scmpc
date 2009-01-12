#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.59])
AC_INIT([scmpc], [0.14.0], [angelos@unkreativ.org])
AC_CONFIG_SRCDIR([config.h.in])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([dist-bzip2])

# Checks for programs.
AC_PROG_CC

# Checks for libraries.
AC_CHECK_LIB([argtable2], [arg_parse])

PKG_CHECK_MODULES([confuse], [libconfuse])
AC_SUBST(confuse_LIBS)
AC_SUBST(confuse_CFLAGS)

PKG_CHECK_MODULES([libcurl], [libcurl >= 7.10.0])
AC_SUBST(libdaemon_LIBS)
AC_SUBST(libdaemon_CFLAGS)

AM_PATH_LIBGCRYPT(1.4.0)

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h netdb.h netinet/in.h stdlib.h string.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_TYPE_PID_T
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([gethostbyname select socket strdup strerror strstr])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
