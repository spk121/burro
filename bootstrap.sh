#!/bin/sh
# Run this to generate all the initial makefiles, etc.
# MLG - I cribbed this script from GNOME's default autogen.sh
test -n "$srcdir" || srcdir=`dirname "$0"`
test -n "$srcdir" || srcdir=.

olddir=`pwd`
cd $srcdir

(test -f configure.ac) || {
        echo "*** ERROR: Directory "\`$srcdir\'" does not look like the top-level project directory ***"
        exit 1
}

AUTOCONF_VERSION=2.69 AUTOMAKE_VERSION=1.15 aclocal --install || exit 1
AUTOCONF_VERSION=2.69 AUTOMAKE_VERSION=1.15 autoreconf --verbose --force --install -Wno-portability || exit 1

cd $olddir
