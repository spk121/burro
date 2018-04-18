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

# Checks for libraries
PKG_PROG_PKG_CONFIG
PKG_CHECK_MODULES([GTK3],[gtk+-3.0 gio-2.0 glib-2.0])
PKG_CHECK_MODULES([VORBISFILE],[vorbisfile])

AC_ARG_WITH([libpulse],
            [AS_HELP_STRING([--with-libpulse],
                            [enable support for pulseaudio])],
            [with_libpulse=yes],
            [with_libpulse=no])
          
LIBPULSE=
AS_IF([test "x$with_pulseaudio" = xyes],
      [PKG_CHECK_MODULES([LIBPULSE], [libpulse],
                         [AC_DEFINE([HAVE_LIBPULSE], [1],
                                    [Define if you have Pulseaudio's libpulse])],
                         [AC_MSG_FAILURE([--with-libpulse was given, but test for Pulseaudio failed])])])
################################################################
# Guile support details
GUILE_PKG([2.2])
GUILE_PROGS
GUILE_FLAGS
GUILE_SITE_DIR

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

if test "$guilesitedir" != "$GUILE_SITE"; then
   # Guile has a different prefix than this module
   AC_MSG_WARN([]
   [The Guile modules will be installed in '${guilesitedir}',]
   [thus, this program will add that directory to the]
   [Guile load path].)
fi
