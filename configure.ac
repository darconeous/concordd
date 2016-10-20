AC_PREREQ(2.59)

dnl This is the project name and release verison number.
dnl The release version is stored in the file `.default-version`.
dnl Update that file *after* every release to indicate the version
dnl of the *next* anticipated release.
AC_INIT(
	[concordd],
	m4_esyscmd([printf "%s" `cat .default-version`]),
	[],
	[concordd],
	[https://github.com/darconeous/concordd/]dnl
)

dnl Attempt to include a reasonable git tag description.
SOURCE_VERSION=m4_esyscmd([
	configver="$PACKAGE_VERSION"
	(test -d .git && gitver=`git describe --dirty --match "[0-9].*"` && printf "$gitver") ||
	(test -d .git && gitver=`git describe --always` && printf "$configver-g$gitver") ||
	printf "$configver"
])

AC_CONFIG_AUX_DIR([m4])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_FILES(Makefile src/Makefile doc/Makefile src/concordd/Makefile src/concordctl/Makefile src/common/Makefile src/missing/Makefile src/missing/strlcat/Makefile src/missing/strlcpy/Makefile)

dnl ###########################################################################

AC_DEFUN([REPLACE_MISSING_FUNC], [
    AC_CHECK_FUNC($1, [], [
        nl_cv_missing_$1=yes
        MISSING_CPPFLAGS="${MISSING_CPPFLAGS} "'-include $(top_srcdir)/src/missing/$1/$1.h'
        MISSING_LIBADD="${MISSING_LIBADD} "'$(top_builddir)/src/missing/$1/lib$1.la'
    ])
    AM_CONDITIONAL(m4_toupper(MISSING_$1), [test "${nl_cv_missing_$1}" = "yes"])
])

REPLACE_MISSING_FUNC([strlcpy])
REPLACE_MISSING_FUNC([strlcat])

AC_SUBST(MISSING_CPPFLAGS)
AC_SUBST(MISSING_LIBADD)

dnl ###########################################################################

AM_INIT_AUTOMAKE()

AM_MAINTAINER_MODE

m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

m4_ifdef([AX_CODE_COVERAGE], [AX_CODE_COVERAGE], [AC_SUBST([CODE_COVERAGE_RULES])])

LT_INIT

AC_LIBTOOL_DLOPEN_SELF

AC_PROG_LIBTOOL

AC_CONFIG_SRCDIR([src/version.h])
AC_CONFIG_HEADERS([src/config.h])

test "x$prefix" = xNONE && prefix=$ac_default_prefix
test "x$exec_prefix" = xNONE && exec_prefix='${prefix}'
test "x$pkglibexecdir" = x && pkglibexecdir='${libexecdir}/${PACKAGE_TARNAME}'

AC_DEFINE_UNQUOTED([PREFIX], ["`eval echo "$prefix"`"], [Define to the install prefix])
AC_DEFINE_UNQUOTED([SYSCONFDIR], ["`eval echo "$sysconfdir"`"], [Define to the sub-directory for system settings.])
AC_DEFINE_UNQUOTED([PKGLIBEXECDIR], ["`eval eval eval echo "$pkglibexecdir"`"], [Define to the sub-directory for plugins.])

AC_DEFINE_UNQUOTED([SOURCE_VERSION], ["`eval echo "$SOURCE_VERSION"`"], [Source version])

AC_PROG_CC([],[AC_MSG_ERROR(["A working C compiler is required"])])
dnl AC_PROG_CXX([],[AC_MSG_ERROR(["A working C++ compiler is required"])])

AC_HEADER_STDC
AC_HEADER_TIME

AC_CHECK_HEADERS([unistd.h errno.h stdbool.h], [], [AC_MSG_ERROR(["Missing a required header."])])

AC_CHECK_HEADERS([sys/un.h sys/wait.h pty.h pwd.h execinfo.h asm/sigcontext.h sys/prctl.h])

AC_C_CONST
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T

AC_C_INLINE
AC_C_VOLATILE

dnl Raspberry Pi's clock_gettime() function is in librt.
dnl This should only pick it up if necessary.
AC_SEARCH_LIBS([clock_gettime], [rt])
AC_CHECK_FUNCS([clock_gettime])

AC_CHECK_FUNCS([alloca fgetln memcmp memset strtol strdup strndup strlcpy strlcat stpncpy vsnprintf vsprintf snprintf getdtablesize getloadavg])

NL_DEBUG
NL_CHECK_DBUS([],AC_MSG_ERROR([DBus is required]))
NL_CHECK_READLINE
NL_CHECK_PTS

dnl AC_DEFINE_UNQUOTED([__STDC_LIMIT_MACROS], [1], [Needed by C++])
dnl AC_DEFINE_UNQUOTED([__STDC_CONSTANT_MACROS], [1], [Needed by C++])

AC_DEFINE_UNQUOTED([ASSERT_MACROS_USE_SYSLOG], 1, [Define to 1 to have assertmacros.h use syslog])
AC_DEFINE_UNQUOTED([ASSERT_MACROS_SQUELCH], 0, [Define to 0 to explicitly prevent squelching assert printouts])

AC_OUTPUT

echo ""
echo "= Summary ====================================================================="
echo ""
echo "Using libreadline .............. ${with_readline-???}"
echo ""

if test "x${with_readline}" != "xyes"
then {
	echo "warning: libreadline was disabled or not found, interactive"
	echo "         concordctl command line interface will not be enabled!"
	echo ""
}
fi
