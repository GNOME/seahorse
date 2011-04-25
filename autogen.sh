#!/bin/sh
# Run this to generate all the initial makefiles, etc.

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

PKG_NAME="seahorse"

(test -f $srcdir/configure.ac) || {
    echo -n "**Error**: Directory "\`$srcdir\'" does not look like the"
    echo " top-level directory"
    exit 1
}

which gnome-autogen.sh || {
    echo "You need to install gnome-common from the GNOME Git"
    exit 1
}

(gtkdocize --flavour=no-tmpl) < /dev/null > /dev/null 2>&1 || {
    echo
    echo "You must have gtk-doc installed to compile seahorse."
    echo "Install the appropriate package for your distribution,"
    echo "or get the source tarball at http://ftp.gnome.org/pub/GNOME/sources/gtk-doc/"
    DIE=1
} 

REQUIRED_AUTOMAKE_VERSION=1.9 USE_GNOME2_MACROS=1 . gnome-autogen.sh
