# Process this file with autoconf to produce a configure script.
AC_PREREQ([2.64])
AC_INIT([gtetrinet],[0.7.11],[http://bugzilla.gnome.org/enter_bug.cgi?product=gtetrinet],[gtetrinet],[http://gtetrinet.sourceforge.net/])

AC_CONFIG_SRCDIR([src/gtetrinet.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([build-aux])

AM_INIT_AUTOMAKE([1.11 tar-pax dist-xz no-dist-gzip foreign silent-rules])
AM_MAINTAINER_MODE([enable])

# Enable silent rules by default
AM_SILENT_RULES([yes])

# Check for programs
AC_PROG_CC
AC_PROG_INSTALL

# Check for header files
AC_CHECK_HEADERS([ctype.h dirent.h errno.h locale.h signal.h \
                  stdarg.h stdio.h stdlib.h string.h unistd.h \
                  netdb.h netinet/in.h \
                  sys/poll.h sys/socket.h sys/time.h sys/types.h sys/wait.h])

# Check for library functions
AC_CHECK_FUNCS([memset setlocale])

# Initialise i18n
GETTEXT_PACKAGE=gtetrinet
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE", [Define to the gettext domain name.])
IT_PROG_INTLTOOL([0.40.0])
AM_GLIB_GNU_GETTEXT

# Check for GConf
AC_PATH_PROG(GCONFTOOL, gconftool-2)
AM_GCONF_SOURCE_2

# Check for pkg-config
PKG_PROG_PKG_CONFIG([0.22])

# Add debug flags if building from git.
if test -d $srcdir/.git; then
	CFLAGS="-Wall -W -g3 -ggdb -O0"
	AC_SUBST(CFLAGS)
fi

# Check for required packages
LIBGTK_REQUIRED=2.6.0
LIBGNOME_REQUIRED=2.0.0
LIBGNOMEUI_REQUIRED=2.0.0
LIBESD_OPTIONAL=0.2.36

PKG_CHECK_MODULES(GTETRINET,[
                  gtk+-2.0 >= $LIBGTK_REQUIRED
                  libgnome-2.0 >= $LIBGNOME_REQUIRED
                  libgnomeui-2.0 >= $LIBGNOMEUI_REQUIRED
])

# Check for libesd
if pkg-config --exists esound ; then
	have_esound=yes
else 
	have_esound=no
fi

if test "x$have_esound" = "xyes"; then
	AC_DEFINE([HAVE_ESD],[1],[Define this to enable EsounD support.])
	PKG_CHECK_MODULES([ESOUND],[esound >= $LIBESD_OPTIONAL])
	AC_SUBST(ESOUND_CFLAGS)
	AC_SUBST(ESOUND_LIBS)
fi

AM_CONDITIONAL(HAVE_ESOUND, test "x$have_esound" = "xyes")

AC_CHECK_FUNC(putenv)
AC_CHECK_FUNC(setenv)
if test $ac_cv_func_setenv = no; then
  if test $ac_cv_func_putenv = no; then
    echo "Error: Couldn't find either putenv() or setenv()"
    exit;
  else
    AC_DEFINE(HAVE_PUTENV, 1, [Define this if putenv() is available.])
  fi
 else
  AC_DEFINE(HAVE_SETENV, 1, [Define this if setenv() is available.])
 fi

# configure options
# Check for page detaching support
AC_ARG_ENABLE(detach,
	AS_HELP_STRING([--enable-detach],[Enable page detaching support (default=no)]),
	enable_detach="yes")
AC_MSG_CHECKING([if page detaching support should be built])
if test "x$enable_detach" = "xyes"; then
	AC_DEFINE([ENABLE_DETACH],[1],[Define this to enable page detaching.])
else
	enable_detach="no"
fi
AC_MSG_RESULT($enable_detach)

# Check for IPv6
AC_ARG_ENABLE(ipv6,
	AS_HELP_STRING([--enable-ipv6],[Enable IPv6 support (default=no)]),
        enable_ipv6="yes")
AC_MSG_CHECKING([if IPv6 support should be built])
if test "x$enable_ipv6" = "xyes"; then
	AC_DEFINE([ENABLE_IPV6],[1],[Define this to enable IPv6 support.])
else
	enable_ipv6="no"
fi
AC_MSG_RESULT($enable_ipv6)

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
echo "   GTranslator version: ${PACKAGE_VERSION}"
echo "   Compilation flags:   ${CFLAGS}"
echo "   IPv6 support:        $enable_ipv6"
echo "   EsounD support:      $have_esound"
echo "   Page detaching:      $enable_detach"
