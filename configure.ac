#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.66])
AC_INIT(swupd-server, 3.3.5, tudor.marcu@intel.com)
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

AC_ARG_ENABLE(
	[bsdtar],
	AS_HELP_STRING([--enable-bsdtar],[Use alternative bsdtar command (uses tar by default)]),
	AS_IF([test "x$enable_bsdtar" = "xyes"],
		AC_DEFINE([SWUPD_WITH_BSDTAR], 1, [Use alternative bsdtar]),
		AC_DEFINE([SWUPD_WITH_BSDTAR], 0, [Use default tar command])),
	AC_DEFINE([SWUPD_WITH_BSDTAR], 0, [Use default tar command])
)

AC_ARG_ENABLE(
  [tests],
  [AS_HELP_STRING([--disable-tests], [Do not enable functional test framework (enabled by default)])]
)

have_coverage=no
AC_ARG_ENABLE(coverage, AS_HELP_STRING([--enable-coverage], [enable test coverage]))
if test "$enable_coverage" = "yes" ; then
        AC_CHECK_PROG(lcov_found, [lcov], [yes], [no])
        if test "$lcov_found" = "no" ; then
                AC_MSG_ERROR([*** lcov support requested but the program was not found])
        else
                lcov_version_major="`lcov --version | cut -d ' ' -f 4 | cut -d '.' -f 1`"
                lcov_version_minor="`lcov --version | cut -d ' ' -f 4 | cut -d '.' -f 2`"
                if test "$lcov_version_major" -eq 1 -a "$lcov_version_minor" -lt 10; then
                        AC_MSG_ERROR([*** lcov version is too old. 1.10 required])
                else
			have_coverage=yes
			AC_DEFINE([COVERAGE], [1], [Coverage enabled])
                fi
        fi
fi
AM_CONDITIONAL([COVERAGE], [test "$have_coverage" = "yes"])

AS_IF([test "$enable_tests" != "no"], [
  AC_PATH_PROG([have_bats], [bats])
  AS_IF([test -z "${have_bats}"], [
    AC_MSG_ERROR([Must have the Bash Automated Testing System (bats) installed to run functional tests])
  ])
])
AM_CONDITIONAL([ENABLE_TESTS], [test "$enable_tests" != "no"])

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
AC_REQUIRE_AUX_FILE([tap-driver.sh])
AC_OUTPUT
