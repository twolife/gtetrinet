# Process this file with autoconf to produce a configure script.
AC_PREREQ([2.64])
AC_INIT([gtetrinet],[0.9.0],[],[gtetrinet],[https://github.com/twolife/gtetrinet])

AC_CONFIG_SRCDIR([src/gtetrinet.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([build-aux])

AM_INIT_AUTOMAKE([1.11 tar-ustar dist-xz no-dist-gzip foreign silent-rules])
AM_MAINTAINER_MODE([enable])

# Enable silent rules by default
AM_SILENT_RULES([yes])

# Checks for programs
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_AWK
AC_PROG_CPP
AC_PROG_MKDIR_P

# Checks for libraries

# Checks for header files
AC_CHECK_HEADERS([libintl.h netdb.h netinet/in.h stdlib.h string.h sys/socket.h sys/time.h unistd.h])

# Check for header files
AC_CHECK_HEADERS([ctype.h dirent.h errno.h locale.h signal.h \
                  stdarg.h stdio.h stdlib.h string.h unistd.h \
                  netdb.h netinet/in.h \
                  sys/poll.h sys/socket.h sys/time.h sys/types.h sys/wait.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T

# Check for library functions
AC_CHECK_FUNCS([atexit memset socket strcasecmp strerror strncasecmp])

# Initialise i18n
AM_GNU_GETTEXT_VERSION([0.19.8])
AM_GNU_GETTEXT([external])

GETTEXT_PACKAGE=gtetrinet
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE", [Define to the gettext domain name.])

# For distcheck, msgfmt from gettext(-runtime) is needed, which is not acted upon by above directives
AC_CHECK_PROGS([found_msgfmt], [msgfmt], [no] )

# Check for pkg-config
PKG_PROG_PKG_CONFIG([0.24])

# Check for dependencies
PKG_CHECK_MODULES([glib],[glib-2.0])
PKG_CHECK_MODULES([gtk],[gtk4])

AC_PATH_PROG(GLIB_COMPILE_SCHEMAS, glib-compile-schemas)

GLIB_GSETTINGS

# configure options
# Check for page detaching support
AC_ARG_ENABLE(detach,
	AS_HELP_STRING([--enable-detach],[Enable page detaching support (default=no)]),
	enable_detach="yes")
AC_MSG_CHECKING([if page detaching support should be built])
AS_IF([test "x$enable_detach" = "xyes"],
	AC_DEFINE([ENABLE_DETACH],[1],[Define this to enable page detaching.]),
	enable_detach="no")

AC_MSG_RESULT($enable_detach)

# Generate files
AC_CONFIG_FILES([
Makefile
doc/Makefile
po/Makefile.in
src/Makefile
src/images/Makefile
themes/Makefile
themes/default/Makefile
themes/small/Makefile
icons/Makefile
])
AC_OUTPUT

# Print configuration information
echo
echo "   Configuration result"
echo "   ------------- ------"
echo
echo "   GTranslator version:              ${PACKAGE_VERSION}"
echo "   Compilation flags:                ${CFLAGS}"
echo "   CPPFLAGS:                         ${CPPFLAGS}"
echo "   msgfmt (required for make dist):  $found_msgfmt"
echo "   Page detaching:                   $enable_detach"
