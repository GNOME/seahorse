#!/bin/sh
# Run this to generate all the initial makefiles, etc.

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

PKG_NAME="seahorse"

(test -f $srcdir/configure.in) || {
    echo -n "**Error**: Directory "\`$srcdir\'" does not look like the"
    echo " top-level directory"
    exit 1
}

which gnome-autogen.sh || {
    echo "You need to install gnome-common from the GNOME CVS"
    exit 1
}
USE_GNOME2_MACROS=1 . gnome-autogen.sh

# This needs a Debian system and the autotools-dev package installed.
# If you don't have it, please do the following in some other dir:
# cvs -z 4 -d :pserver:anoncvs@subversions.gnu.org:/cvsroot/config \
# export -rHEAD config
# and then copy both config.[guess,sub] files to the seahorse2 tree before 
# calling 'make dist' target.
# cp -f /usr/share/automake-1.6/config.[guess,sub] .
