# Process this file with autoconf to produce a configure script.
AC_PREREQ(2.63)
AC_INIT([gtetrinet],[0.7.11],[http://bugzilla.gnome.org/enter_bug.cgi?product=gtetrinet],[gtetrinet],[http://gtetrinet.sourceforge.net/])

AC_CONFIG_SRCDIR([src/gtetrinet.c])
AM_CONFIG_HEADER([config.h])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([config])

AC_SUBST([PACKAGE_NAME], ["$PACKAGE_NAME"])
AC_SUBST([PACKAGE_VERSION], ["$PACKAGE_VERSION"])

AM_INIT_AUTOMAKE([1.10 dist-bzip2 no-dist-gzip foreign silent-rules])
AM_MAINTAINER_MODE

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

dnl Add debug flags if building from git.
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
AC_SUBST(GTETRINET_CFLAGS)
AC_SUBST(GTETRINET_LIBS)

dnl Check for libesd
if pkg-config --exists esound ; then
	have_esound=yes
else 
	have_esound=no
fi

if test "x$have_esound" = "xyes"; then
AC_DEFINE(HAVE_ESD, 1, [Define this to enable EsounD support.])
PKG_CHECK_MODULES(ESOUND, esound >= $LIBESD_OPTIONAL)
AC_SUBST(ESOUND_CFLAGS)
AC_SUBST(ESOUND_LIBS)
fi

AM_CONDITIONAL(HAVE_ESOUND, test "x$have_esound" = "xyes")

AC_HEADER_STDC
AC_HEADER_SYS_WAIT

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

dnl configure options
AC_ARG_ENABLE(detach,
[  --enable-detach         Enable page detaching (default=no)],
  [if test "x$enable_detach" = "xyes"; then
    AC_DEFINE(ENABLE_DETACH, 1, [Define this to enable page detaching.])
  fi])

AC_ARG_ENABLE(ipv6,
[  --enable-ipv6           Enable IPv6 support (default=no)],
  [if test "x$enable_ipv6" = "xyes"; then
   AC_DEFINE(USE_IPV6, 1, [Define this to enable IPv6 support.])
  fi])

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
echo "   Configuration result"
echo "   ------------- ------"
echo
echo "   GTranslator version: ${PACKAGE_VERSION}"
echo "   Compilation flags:   ${CFLAGS}"
echo "   IPv6 support:        $enable_ipv6"
echo "   EsounD support:      $have_esound"
echo "   Page detaching:      $enable_detach"
