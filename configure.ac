#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT([scmpc], [0.2.2], [jcoome@gmail.com])

AC_CONFIG_SRCDIR([src/scmpc.h])
AC_CONFIG_HEADER([src/config.h])

AM_INIT_AUTOMAKE([dist-bzip2 foreign])

# Checks for programs.
AC_PROG_CC
AC_PROG_CC_C99
AC_PROG_INSTALL

# Checks for libraries.
AC_CHECK_LIB([argtable2], [arg_parse],, 
	AC_MSG_ERROR([[scmpc requires argtable, which can be found at
	http://argtable.sourceforge.net/]]))
PKG_CHECK_MODULES([confuse], [libconfuse],,
	AC_MSG_ERROR([[scmpc requires confuse, which can be found at
	http://www.nongnu.org/confuse/]]))
PKG_CHECK_MODULES([libdaemon], [libdaemon],,
	AC_MSG_ERROR([[scmpc requires libdaemon, which can be found at
	http://0pointer.de/lennart/projects/libdaemon/]]))
PKG_CHECK_MODULES([libcurl], [libcurl >= 7.10.0],,
	AC_MSG_ERROR([[scmpc requires libcurl >= 7.10.0, which can be found at
	http://curl.haxx.se/libcurl]]))
AC_CHECK_LIB([pthread], [pthread_mutex_init],, 
	AC_MSG_ERROR([[scmpc requires a pthread library]]))


# The AC_SUBST values are used to replace the default values in the
# configuration file. Don't remove them!

AC_MSG_CHECKING([for default MPD host])
AC_ARG_WITH([default-mpd-host],
	    AC_HELP_STRING([--with-default-mpd-host=ARG],
	                   [Default MPD host @<:@localhost@:>@]),
            [DEFAULT_HOST="$withval"],
            [DEFAULT_HOST="localhost"])
AC_MSG_RESULT([$DEFAULT_HOST])
AC_DEFINE_UNQUOTED([DEFAULT_MPD_HOST], ["$DEFAULT_HOST"], [Default MPD host])
AC_SUBST([mpd_host], ["$DEFAULT_HOST"])

AC_MSG_CHECKING([for default MPD port])
AC_ARG_WITH([default-mpd-port],
	    AC_HELP_STRING([--with-default-mpd-port=ARG],
	                   [Default MPD port @<:@6600@:>@]),
            [DEFAULT_PORT="$withval"],
            [DEFAULT_PORT="6600"])
AC_MSG_RESULT([$DEFAULT_PORT])
AC_DEFINE_UNQUOTED([DEFAULT_MPD_PORT], [$DEFAULT_PORT], [Default MPD port])
AC_SUBST([mpd_port], [$DEFAULT_PORT])

AC_MSG_CHECKING([for default MPD timeout])
AC_ARG_WITH([default-mpd-timeout],
	    AC_HELP_STRING([--with-default-mpd-timeout=ARG],
	                   [Default MPD timeout, in seconds @<:@5@:>@]),
            [DEFAULT_TIMEOUT="$withval"],
            [DEFAULT_TIMEOUT="5"])
AC_MSG_RESULT([$DEFAULT_TIMEOUT])
AC_DEFINE_UNQUOTED([DEFAULT_MPD_TIMEOUT], [$DEFAULT_TIMEOUT], 
	[Default MPD timeout])
AC_SUBST([mpd_timeout], [$DEFAULT_TIMEOUT])

AC_MSG_CHECKING([for default cache interval])
AC_ARG_WITH([default-cache-interval],
	    AC_HELP_STRING([--with-default-cache-interval=ARG],
	                   [The interval in minutes to save the unsubmitted song
					   queue. @<:@10@:>@]),
            [DEFAULT_CACHE="$withval"],
            [DEFAULT_CACHE="10"])
AC_MSG_RESULT([$DEFAULT_CACHE])
AC_DEFINE_UNQUOTED([DEFAULT_CACHE_INTERVAL], [$DEFAULT_CACHE], 
	[Default cache interval])
AC_SUBST([cache_interval], [$DEFAULT_CACHE])

AC_MSG_CHECKING([for default maximum song queue length])
AC_ARG_WITH([default-queue-length],
	    AC_HELP_STRING([--with-default-queue-length=ARG],
	                   [Default maximum number of unsubmitted songs to keep in
					   memory. @<:@500@:>@]),
            [DEFAULT_QUEUE="$withval"],
            [DEFAULT_QUEUE="500"])
AC_MSG_RESULT([$DEFAULT_QUEUE])
AC_DEFINE_UNQUOTED([DEFAULT_QUEUE_LENGTH], [$DEFAULT_QUEUE], 
	[Default maximum number of unsubmitted songs to keep in memory])
AC_SUBST([queue_length], [$DEFAULT_QUEUE])

AC_MSG_CHECKING([for default log level])
AC_ARG_WITH([default-log-level],
	    AC_HELP_STRING([--with-default-log-level=ARG],
				[Default logging level @<:@error@:>@. Valid options are:
				'none', 'error', 'info', or 'debug']),
            [DEFAULT_LOG_LEVEL="$withval"],
            [DEFAULT_LOG_LEVEL="error"])
AC_MSG_RESULT([$DEFAULT_LOG_LEVEL])
case $DEFAULT_LOG_LEVEL in
	"none") log_level="NONE" ;;
	"error") log_level="ERROR" ;;
	"info") log_level="INFO" ;;
	"debug") log_level="DEBUG" ;;
	*) log_level="" ;;
esac
if test "x$log_level" = "x"; then
	AC_MSG_ERROR([[Invalid value for default-log-level: $DEFAULT_LOG_LEVEL]])
else
	AC_DEFINE_UNQUOTED([DEFAULT_LOG_LEVEL], [$log_level], [Default log level])
	AC_SUBST([log_level], [$DEFAULT_LOG_LEVEL])
fi

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([fcntl.h netdb.h netinet/in.h stdlib.h string.h sys/socket.h sys/time.h unistd.h])

AC_C_BIGENDIAN(
   	AC_DEFINE(ARCH_IS_BIG_ENDIAN,1,[For md5.c]),
   	AC_DEFINE(ARCH_IS_BIG_ENDIAN,0,[For md5.c]))

# Checks for typedefs, structures, and compiler characteristics.
#AC_HEADER_STDBOOL
#AC_C_CONST
#AC_TYPE_PID_T
#AC_TYPE_SIZE_T
#AC_HEADER_TIME
#AC_STRUCT_TM

# Checks for library functions.
#AC_FUNC_FORK
#AC_FUNC_MALLOC
#AC_FUNC_REALLOC
#AC_FUNC_SELECT_ARGTYPES
#AC_FUNC_STRFTIME
#AC_FUNC_VPRINTF
AC_CHECK_FUNCS([gethostbyname memset select socket strdup strerror strstr \
	strtol strlcat strlcpy asprintf getline])

AC_OUTPUT([scmpc.conf Makefile scmpc.1])
