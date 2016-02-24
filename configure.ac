#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.66])
AC_INIT(swupd-server, 3.0.0, timothy.c.pepper@linux.intel.com)
AM_INIT_AUTOMAKE([foreign -Wall -W subdir-objects])
AM_SILENT_RULES([yes])
AC_PROG_CC
AM_PROG_CC_C_O
AC_LANG(C)
AC_CONFIG_HEADERS([config.h])
PKG_CHECK_MODULES([bsdiff], [bsdiff])
PKG_CHECK_MODULES([glib], [glib-2.0, gthread-2.0])
PKG_CHECK_MODULES([zlib], [zlib])
PKG_CHECK_MODULES([openssl], [libcrypto >= 0.9.8])
AC_CHECK_LIB([magic], [magic_open], [], [AC_MSG_ERROR([the magic library is missing])])
AC_CHECK_PROGS(TAR, tar)

AC_ARG_ENABLE([bzip2],
	      [AS_HELP_STRING([--disable-bzip2],[Do not use bzip2 compression (uses bzip2 by default)])])

AC_ARG_ENABLE([lzma],
	      [AS_HELP_STRING([--disable-lzma],[Do not use lzma compression (uses lzma by default)])])
AC_ARG_ENABLE(
    [stateless],
    AS_HELP_STRING([--disable-stateless],[OS is not stateless, do not ignore configuration files (stateless by default)]),
    AC_DEFINE(SWUPD_WITH_STATELESS,0,[OS is not stateless]),
		AC_DEFINE(SWUPD_WITH_STATELESS,1,[OS is stateless])
)

AS_IF([test "$enable_bzip2" != "no"], [
  AC_CHECK_LIB([bz2], [BZ2_bzBuffToBuffCompress], [], [AC_MSG_ERROR([the libbz2 library is missing])])
  AC_CHECK_PROGS(BZIP2, bzip2)
  AC_DEFINE(SWUPD_WITH_BZIP2,1,[Use bzip2 compression])
])

AS_IF([test "$enable_lzma" != "no"], [
  PKG_CHECK_MODULES([lzma], [liblzma])
  AC_CHECK_PROGS(XZ, xz)
  AC_DEFINE(SWUPD_WITH_LZMA,1,[Use lzma compression])
])
AM_CONDITIONAL([ENABLE_LZMA], [test "$enable_lzma" != "no"])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
