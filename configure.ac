# Process this file with autoconf to produce a configure script.

# Autoconf requirements
AC_PREQ([2.69])
AC_INIT([Burro Engine],[0.0],
        [spk121@yahoo.com],
        [burro-engine],
        [https://github.com/spk121/burro-engine])

AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([src/main.c])
AC_CONFIG_AUX_DIR([build_aux])
AM_INIT_AUTOMAKE([foreign tar-ustar subdir-objects])
LT_INIT([win32-dll])
AM_SILENT_RULES([yes])

GLIB_GSETTINGS

AC_CONFIG_HEADERS([config.h])

AC_DEFINE_UNQUOTED([HOST_TYPE], ["$host"],
  [Define to the host's GNU triplet.])
case $host in
  *-*-mingw*)
     AC_DEFINE([HAVE_WINCOREAUDIO], [1],
                                    [Define if you have Windows Core Audio])
    ;;
    *)
	PKG_CHECK_MODULES([LIBPULSE], [libpulse],
                          [AC_DEFINE([HAVE_LIBPULSE], [1],
                                     [Define if you have Pulseaudio's libpulse])])
				     ;;
esac


# Checks for libraries
PKG_PROG_PKG_CONFIG
PKG_CHECK_MODULES([GTK3],[gtk+-3.0 gio-2.0 glib-2.0])
PKG_CHECK_MODULES([VORBISFILE],[vorbisfile])

dnl AC_ARG_WITH([libpulse],
dnl             [AS_HELP_STRING([--with-pulseaudio],
dnl                             [enable support for pulseaudio])],
dnl             [with_libpulse=yes],
dnl             [with_libpulse=no])

dnl LIBPULSE=
dnl AS_IF([test "x$with_libpulse" = xyes],
dnl       [PKG_CHECK_MODULES([LIBPULSE], [libpulse],
dnl                          [AC_DEFINE([HAVE_LIBPULSE], [1],
dnl                                     [Define if you have Pulseaudio's libpulse])],
dnl                          [AC_MSG_FAILURE([--with-pulseaudio was given, but test for Pulseaudio's libpulse failed])])])
################################################################
# Guile support details
GUILE_PKG([2.2])
# GUILE_PROGS
GUILE_FLAGS
# GUILE_SITE_DIR

AC_ARG_VAR([GUILE_SNARF], [guile snarf compilation tool])
_guile_suffix=`echo "$GUILE" | sed -e 's,^.*/guile\(.*\)$,\1,'`
AC_CHECK_PROGS([GUILE_SNARF],[guile-snarf$_guile_suffix guile$_guile_suffix-snarf])
AS_IF([test "x$GUILE_SNARF" = "xno"],
      [AC_MSG_ERROR([Could not find guile-snarf$_guile_suffix tool needed for compilation])])

# Guile has three directories that don't follow the GNU Filesystem
# Heirarchy Standards.  If one follows the GNU FHS, files get installed
# in directories in which Guile does not search for them by default.
AC_ARG_WITH([guilesitedir],
  [use the specified installation path for Guile modules],
  [case "x$withval" in
    xyes|xno) guilesitedir="";;
    *)        guilesitedir="$withval";;
   esac],
  [guilesitedir=""])

pkgdatadir="$datadir/$PACKAGE_TARNAME"
if test "x$guilesitedir" = "x"; then
  guilesitedir="$pkgdatadir"
fi
AC_SUBST([guilesitedir])

AC_ARG_WITH([guileextensiondir],
  [use the specified installation path for Guile binary shared libraries],
  [case "x$withval" in
    xyes|xno) guileextensiondir="";;
    *)        guileextensiondir="$withval";;
   esac],
  [guileextensiondir=""])

if test "x$guileextensiondir" = "x"; then
  guileextensiondir="$libdir"
fi
AC_SUBST([guileextensiondir])

################################################################
# GLIB resource compiler

GLIB_COMPILE_RESOURCES=`$PKG_CONFIG --variable glib_compile_resources gio-2.0`
AC_SUBST(GLIB_COMPILE_RESOURCES)

################################################################
# Finish up

AC_CONFIG_FILES([
 Makefile
 src/Makefile
 test/Makefile
 game/Makefile
 data/Makefile
 data/icons/Makefile
 data/icons/hicolor/Makefile
 data/icons/hicolor/48x48/Makefile
 data/icons/hicolor/48x48/apps/Makefile
 data/icons/hicolor/scalable/Makefile
 data/icons/hicolor/scalable/apps/Makefile
 data/icons/src/Makefile
 ])

AC_CONFIG_FILES([src/uninstalled-env], [chmod +x src/uninstalled-env])
AC_OUTPUT
AC_SUBST([guilesitedir])
