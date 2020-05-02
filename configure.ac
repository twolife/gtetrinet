# Process this file with autoconf to produce a configure script.
AC_PREREQ([2.64])
AC_INIT([gtetrinet],[0.7.11],[http://bugzilla.gnome.org/enter_bug.cgi?product=gtetrinet],[gtetrinet],[http://gtetrinet.sourceforge.net/])

AC_CONFIG_SRCDIR([src/gtetrinet.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([build-aux])

AM_INIT_AUTOMAKE([1.11 tar-ustar dist-xz no-dist-gzip foreign silent-rules])
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
AM_GNU_GETTEXT_VERSION([0.17])
AM_GNU_GETTEXT([external])

GETTEXT_PACKAGE=gtetrinet
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE", [Define to the gettext domain name.])
IT_PROG_INTLTOOL([0.40.0])

# Check for GConf
AC_PATH_PROG(GCONFTOOL, gconftool-2)
AM_GCONF_SOURCE_2

# Check for pkg-config
PKG_PROG_PKG_CONFIG([0.22])

# Add debug flags if building from git.
if test -d $srcdir/.git; then
	CFLAGS="-Wall -W -g3 -ggdb -O0 -w "
	AC_SUBST(CFLAGS)
fi

# Check for required packages
LIBGLIB_REQUIRED=2.32.0
LIBGTK_REQUIRED=3.18.0
LIBCANBERRAGTK_OPTIONAL=0.2
LIBGCONF_REQUIRED=3.0.0

                  #libgnomeui-2.0 >= 2.0
PKG_CHECK_MODULES(GTETRINET,[
                  glib-2.0 >= $LIBGLIB_REQUIRED
                  gtk+-3.0 >= $LIBGTK_REQUIRED
                  gconf-2.0 >= $LIBGCONF_REQUIRED
])

# Check for libcanberra-gtk
if pkg-config --exists "libcanberra-gtk >= $LIBCANBERRAGTK_OPTIONAL"; then
	have_canberragtk=no
else
	have_canberragtk=no
fi

if test "x$have_canberragtk" = "xyes"; then
	AC_DEFINE([HAVE_CANBERRAGTK],[1],[Define this to enable sound support.])
	PKG_CHECK_MODULES([CANBERRAGTK],[libcanberra-gtk >= $LIBCANBERRAGTK_OPTIONAL])
	AC_SUBST(CANBERRAGTK_CFLAGS)
	AC_SUBST(CANBERRAGTK_LIBS)
fi

AM_CONDITIONAL(HAVE_CANBERRAGTK, test "x$have_canberragtk" = "xyes")

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

dnl ---------------------------------------------------------------------------
dnl - Use strict options (default enabled for devs, disabled in releases)
dnl ---------------------------------------------------------------------------
if test -d ".git"; then
	default_strict=yes
else
	default_strict=no
fi

AC_ARG_ENABLE(strict, AS_HELP_STRING([--enable-strict],[Enable strict compilation options]), enable_strict=$enableval,
					enable_strict=$default_strict)
if test x$enable_strict != xno; then
        #CPPFLAGS="$CPPFLAGS -DG_DISABLE_SINGLE_INCLUDES -DGTK_DISABLE_SINGLE_INCLUDES"
        #CPPFLAGS="$CPPFLAGS -DG_DISABLE_DEPRECATED -DGTK_DISABLE_DEPRECATED -DGDK_DISABLE_DEPRECATED"
	CPPFLAGS="$CPPFLAGS"
fi

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
echo "   CPPFLAGS:            ${CPPFLAGS}"
echo "   Sound support:       $have_canberragtk"
echo "   Page detaching:      $enable_detach"
