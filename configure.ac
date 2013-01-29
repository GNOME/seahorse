AC_PREREQ([2.63])
AC_INIT([seahorse], [3.7.2],
        [http://bugzilla.gnome.org/enter_bug.cgi?product=seahorse],
        [seahorse])

GCK_REQUIRED=3.3.4
GCR_REQUIRED=3.3.4
GTK_REQUIRED=3.4.0
GNUPG_ACCEPTED="1.2 1.4 2.0"
GPGME_REQUIRED=1.0.0
LIBSECRET_REQUIRED=0.5
AVAHI_GLIB_REQUIRED=0.6

# -----------------------------------------------------------------------------
# Basic tools

AM_INIT_AUTOMAKE([1.11 tar-ustar foreign -Wno-portability])
AM_SANITY_CHECK
AC_CONFIG_HEADERS([config.h])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])
AM_MAINTAINER_MODE([enable])
AC_CONFIG_MACRO_DIR([m4])

AC_PROG_CC
AC_PROG_CXX
AC_PROG_INTLTOOL([0.35.0])
AM_PROG_LIBTOOL
AC_PATH_PROG(PKG_CONFIG, pkg-config, no)

GETTEXT_PACKAGE=seahorse
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE", [Name of the gettext domain.])

AC_MSG_CHECKING([for some Win32 platform])
case "$host" in
  *-*-cygwin*|*-*-mingw*)
    platform_win32=yes
    ;;
  *)
    platform_win32=no
    ;;
esac
AC_MSG_RESULT([$platform_win32])
AM_CONDITIONAL(PLATFORM_WIN32, test "$platform_win32" = "yes")

AC_MSG_CHECKING([for native Win32])
case "$host" in
  *-*-mingw*)
    os_win32=yes
    ;;
  *)
    os_win32=no
    ;;
esac
AC_MSG_RESULT([$os_win32])
AM_CONDITIONAL(OS_WIN32, test "$os_win32" = "yes")

if test "$platform_win32" = "yes" ; then
	PLUGIN_LIBTOOL_FLAGS="-module -avoid-version -no-undefined"
else
	PLUGIN_LIBTOOL_FLAGS="-module -avoid-version"
fi
AC_SUBST(PLUGIN_LIBTOOL_FLAGS)

GNOME_CODE_COVERAGE
CFLAGS="$CFLAGS $CODE_COVERAGE_CFLAGS"
LDFLAGS="$LDFLAGS $CODE_COVERAGE_LDFLAGS"

# -----------------------------------------------------------------------------
# Basic Dependencies

PKG_CHECK_MODULES(GTK, gtk+-3.0 >= $GTK_REQUIRED)
AC_SUBST(GTK_CFLAGS)
AC_SUBST(GTK_LIBS)

PKG_CHECK_MODULES(SEAHORSE, gmodule-2.0 gio-2.0 gthread-2.0 gtk+-3.0 >= $GTK_REQUIRED gcr-3 >= $GCR_REQUIRED)
SEAHORSE_CFLAGS="$SEAHORSE_CFLAGS -DGCR_API_SUBJECT_TO_CHANGE -DGCK_API_SUBJECT_TO_CHANGE"

# User documentation
YELP_HELP_INIT

GLIB_GSETTINGS
AM_GLIB_GNU_GETTEXT
AC_PATH_PROG(GLIB_GENMARSHAL, glib-genmarshal)

PKG_CHECK_MODULES(EGG, gtk+-3.0 >= $GTK_REQUIRED)
AC_SUBST(EGG_LIBS)
AC_SUBST(EGG_CFLAGS)

PKG_CHECK_MODULES(EGG_SMCLIENT, gtk+-3.0)
AC_SUBST(EGG_SMCLIENT_LIBS)
AC_SUBST(EGG_SMCLIENT_CFLAGS)

# -----------------------------------------------------------------------------
# Checks for functions

AC_SYS_LARGEFILE
AC_TYPE_OFF_T
AC_FUNC_FSEEKO

AC_CHECK_FUNCS(strsep)

# -----------------------------------------------------------------------------
# GPG / GPGME CHECKS

AC_ARG_ENABLE(pgp,
	AC_HELP_STRING([--enable-pgp], [enable PGP support (default is auto)]))
	
have_gpg=""
have_gpgme=""

if test "$enable_pgp" = "no"; then
	echo "disabling PGP support"
else
	AC_ARG_ENABLE(gpg-check, 
		AC_HELP_STRING([--disable-gpg-check], [check GPG version (default is yes)]),
		DO_CHECK=$enableval, DO_CHECK=yes)
	
	if test	"$DO_CHECK" = "yes"; then
		AC_PATH_PROGS(GNUPG, [gpg gpg2], no)
		AC_DEFINE_UNQUOTED(GNUPG, "$GNUPG", [Path to gpg executable.])
		ok="no"
		if test "$GNUPG" != "no"; then
			AC_MSG_CHECKING(for appropriate GnuPG version)
			gnupg_version=`$GNUPG --version | grep gpg`
			major=`echo $gnupg_version | \
				sed 's/^gpg (GnuPG) \([[0-9]]*\)\.\([[0-9]]*\)\.\([[0-9]]*\).*/\1/'`
			minor=`echo $gnupg_version | \
				sed 's/^gpg (GnuPG) \([[0-9]]*\)\.\([[0-9]]*\)\.\([[0-9]]*\).*/\2/'`
			micro=`echo $gnupg_version | \
				sed 's/^gpg (GnuPG) \([[0-9]]*\)\.\([[0-9]]*\)\.\([[0-9]]*\).*/\3/'`
    
			for ver in $GNUPG_ACCEPTED; do
				if test "$ver" = "$major.$minor"; then
					AC_DEFINE_UNQUOTED(GPG_MAJOR, [$major], [GPG Major Version])
					AC_DEFINE_UNQUOTED(GPG_MINOR, [$minor], [GPG Minor Version])
					AC_DEFINE_UNQUOTED(GPG_MICRO, [$micro], [GPG Micro Version])
					ok="yes"
					break
				fi
			done
		fi
  
		if test "$ok" = "yes"; then
			AC_MSG_RESULT([yes])
			have_gpg="$gnupg_version"
		else
			AC_MSG_RESULT([no])
		fi
	fi


	ok="no"
	AC_PATH_PROG(GPGME_CONFIG, gpgme-config, "failed")
	if test $GPGME_CONFIG != "failed" ; then
      		AC_MSG_CHECKING(for GPGME - version >= $GPGME_REQUIRED)
		req_major=`echo $GPGME_REQUIRED | \
			sed 's/\([[0-9]]*\)\.\([[0-9]]*\)\.\([[0-9]]*\)/\1/'`
		req_minor=`echo $GPGME_REQUIRED | \
			sed 's/\([[0-9]]*\)\.\([[0-9]]*\)\.\([[0-9]]*\)/\2/'`
		req_micro=`echo $GPGME_REQUIRED | \
			sed 's/\([[0-9]]*\)\.\([[0-9]]*\)\.\([[0-9]]*\)/\3/'`
		gpgme_config_version=`$GPGME_CONFIG --version`
		major=`echo $gpgme_config_version | \
			sed 's/\([[0-9]]*\)\.\([[0-9]]*\)\.\([[0-9]]*\).*/\1/'`
		minor=`echo $gpgme_config_version | \
			sed 's/\([[0-9]]*\)\.\([[0-9]]*\)\.\([[0-9]]*\).*/\2/'`
		micro=`echo $gpgme_config_version | \
			sed 's/\([[0-9]]*\)\.\([[0-9]]*\)\.\([[0-9]]*\).*/\3/'`

		if test "$major" -eq "$req_major"; then
			if test "$minor" -ge "$req_minor"; then
				if test "$micro" -ge "$req_micro"; then
					ok="yes"
				fi
			fi
		fi
	fi

	if test $ok = "yes"; then
		GPGME_CFLAGS=`$GPGME_CONFIG --cflags`
		GPGME_LIBS=`$GPGME_CONFIG --libs`
		AC_MSG_RESULT(yes)
		have_gpgme="$gpgme_config_version"
	else
		AC_MSG_RESULT(no)
	fi
fi

if test "$enable_pgp" != "no" -a -n "$have_gpg" -a -n "$have_gpgme"; then
	enable_pgp="yes"
fi

if test "$enable_pgp" = "yes"; then
	if test -z "$have_gpg"; then
		AC_MSG_ERROR([Appropriate version of GnuPG not found. Please install one of versions: $GNUPG_ACCEPTED])
	fi
			
	if test -z "$have_gpgme"; then
		AC_MSG_ERROR(GPGME $GPGME_REQUIRED or later needed)
	fi

	SEAHORSE_CFLAGS="$SEAHORSE_CFLAGS $GPGME_CFLAGS"
	SEAHORSE_LIBS="$SEAHORSE_LIBS $GPGME_LIBS"
        AC_DEFINE(WITH_PGP, 1, [With PGP Support])
fi

AM_CONDITIONAL(WITH_PGP, test "$enable_pgp" = "yes")

# -----------------------------------------------------------------------------
# LDAP [from gpg]

AC_MSG_CHECKING([LDAP keyserver support])
AC_ARG_ENABLE(ldap,
[  --disable-ldap          disable LDAP keyserver interface],
    try_ldap=$enableval, try_ldap=yes)

if test "$enable_pgp" != "yes"; then
	echo "LDAP keyserver interface requires PGP support"
	try_ldap=no
fi

AC_MSG_RESULT($try_ldap)
with_ldap=no

dnl Must check for network library requirements before doing link tests
dnl for ldap, for example. If ldap libs are static (or dynamic and without
dnl ELF runtime link paths), then link will fail and LDAP support won't
dnl be detected.

AC_CHECK_FUNC(gethostbyname, , AC_CHECK_LIB(nsl, gethostbyname,
	[NETLIBS="-lnsl $NETLIBS"]))
AC_CHECK_FUNC(setsockopt, , AC_CHECK_LIB(socket, setsockopt,
	[NETLIBS="-lsocket $NETLIBS"]))
	
dnl Try and link a LDAP test program to weed out unusable LDAP
dnl libraries.  -lldap [-llber [-lresolv]] is for OpenLDAP.  OpenLDAP in
dnl general is terrible with creating weird dependencies.  If all else
dnl fails, the user can play guess-the-dependency by using something
dnl like LDAP_LIBS="-lfoo" ./configure

if test "$try_ldap" = yes ; then
  for MY_LDAPLIBS in ${LDAP_LIBS+"$LDAP_LIBS"} "-lldap -llber" "-lldap" "-lldap -llber -lresolv"; do
    _ldap_save_libs=$LIBS
    LIBS="$MY_LDAPLIBS $NETLIBS $LIBS"

    AC_MSG_CHECKING([whether LDAP via \"$MY_LDAPLIBS\" is present and sane])
    AC_TRY_LINK([#include <ldap.h>],[ldap_open("foobar",1234); ber_free(NULL, 0);],
                [cv_func_ldap_init=yes],[cv_func_ldap_init=no])
    AC_MSG_RESULT([$cv_func_ldap_init])

    if test $cv_func_ldap_init = no; then
      AC_MSG_CHECKING([whether I can make LDAP be sane with lber.h])
      AC_TRY_LINK([#include <lber.h>
#include <ldap.h>],[ldap_open("foobar",1234);],
         [cv_func_ldaplber_init=yes],[cv_func_ldaplber_init=no])
      AC_MSG_RESULT([$cv_func_ldaplber_init])
    fi

    if test "$cv_func_ldaplber_init" = yes ; then
       AC_DEFINE(NEED_LBER_H,1,[Define if the LDAP library requires including lber.h before ldap.h])
    fi

    if test "$cv_func_ldap_init" = yes || \
  	test "$cv_func_ldaplber_init" = yes ; then
       LDAP_LIBS=$MY_LDAPLIBS
    fi

    LIBS=$_ldap_save_libs

    if test "$LDAP_LIBS" != ""; then 
    	with_keyserver=yes
        with_ldap=yes
		AC_DEFINE(WITH_LDAP, 1, [Support for LDAP operations])
    	break
    fi
  done
fi	

AM_CONDITIONAL(WITH_LDAP, test "$with_ldap" = "yes")
SEAHORSE_LIBS="$SEAHORSE_LIBS $LDAP_LIBS"

AC_SUBST(NETLIBS)

# -----------------------------------------------------------------------------
# HKP (libsoup)

# Allow hkp to be disabled 
AC_ARG_ENABLE(hkp,
        AC_HELP_STRING([--disable-hkp],
        [disable HKP keyserver interface]))

# HKP tests
if test "$enable_pgp" != "yes"; then
	echo "HKP keyserver interface requires PGP support"
	enable_hkp=no
fi
 
if test "$enable_hkp" = "no"; then
    echo "disabling HKP keyserver interface"
else
    
    echo "checking for supported versions of libsoup..."

    PKG_CHECK_MODULES(SOUP, libsoup-2.4 >= 2.24.0, 
        [enable_hkp=yes],  
        [enable_hkp=no])

    if test "$enable_hkp" = yes; then
        with_keyserver=yes
        
        AC_DEFINE(WITH_HKP, 1, [Support for HKP operations])
        SEAHORSE_LIBS="$SEAHORSE_LIBS $SOUP_LIBS"
        SEAHORSE_CFLAGS="$SEAHORSE_CFLAGS $SOUP_CFLAGS"
        
        # libsoup is also used for async lookups
        AC_DEFINE(WITH_SOUP, 1, [With libsoup])
    fi
fi

AM_CONDITIONAL(WITH_HKP, test "$enable_hkp" = "yes")

# -----------------------------------------------------------------------------
# KEYSERVER

AM_CONDITIONAL(WITH_KEYSERVER, test "$with_keyserver" = "yes")

if test "$with_keyserver" = "yes"; then
	echo "enabling key server support"
	AC_DEFINE(WITH_KEYSERVER, 1, [Support for key server actions])
else
    with_keyserver=no
	echo "disabling key server support"
fi

# -----------------------------------------------------------------------------
# LIBSECRET

PKG_CHECK_MODULES([LIBSECRET], libsecret-unstable >= $LIBSECRET_REQUIRED,
                  [enable_gnome_keyring=yes],[enable_gnome_keyring=no])

if test "$enable_gnome_keyring" = "no"; then
    AC_MSG_ERROR([libsecret library or headers not found.])
fi

SEAHORSE_CFLAGS="$SEAHORSE_CFLAGS $LIBSECRET_CFLAGS"
SEAHORSE_LIBS="$SEAHORSE_LIBS $LIBSECRET_LIBS"

# -----------------------------------------------------------------------------
# KEY SHARING

# Allow sharing to be disabled
AC_ARG_ENABLE(sharing, 
        AC_HELP_STRING([--disable-sharing],
        [Don't compile in DNS-SD key sharing]))

if test "$enable_pgp" != "yes"; then
	echo "DNS-SD key sharing requires PGP support"
	enable_sharing=no
fi
	
if test "$enable_hkp" = "no"; then
    	echo "DNS-SD key sharing requires HKP support"
	enable_sharing=no
fi

if test "$enable_sharing" = "no"; then
    echo "disabling DNS-SD key sharing"
else

    PKG_CHECK_MODULES(SHARING, avahi-client avahi-glib >= $AVAHI_GLIB_REQUIRED, 
        [enable_sharing=yes],  
        [enable_sharing=no])
		
	if test "$enable_sharing" = "yes"; then
		echo "enabling key sharing"
		
		AC_DEFINE_UNQUOTED(WITH_SHARING, 1, [Compile bonjour key sharing code])
		SEAHORSE_CFLAGS="$SEAHORSE_CFLAGS $SHARING_CFLAGS"
		SEAHORSE_LIBS="$SEAHORSE_LIBS $SHARING_LIBS"
    fi
fi

AM_CONDITIONAL(WITH_SHARING, test "$enable_sharing" = "yes")

# -----------------------------------------------------------------------------
#  PKCS11

AC_ARG_ENABLE(pkcs11,
	AC_HELP_STRING([--disable-pkcs11],
		[Support keys and certificates via PKCS#11]))

if test "$enable_pkcs11" = "no"; then
	echo "disabling pkcs11 support"
	enable_pkcs11="no"
else
	PKG_CHECK_MODULES(GCK, gck-1 >= $GCK_REQUIRED, enable_pkcs11="yes", enable_pkcs11="no")
	if test "$enable_pkcs11" = "yes"; then
		AC_DEFINE(WITH_PKCS11, 1, [gck library available])
		SEAHORSE_CFLAGS="$SEAHORSE_CFLAGS $GCK_CFLAGS $GCR_CFLAGS"
		SEAHORSE_LIBS="$SEAHORSE_LIBS $GCK_LIBS $GCR_LIBS"
	else
		enable_pkcs11="no"
	fi
fi

AM_CONDITIONAL(WITH_PKCS11, test "$enable_pkcs11" = "yes")

# -----------------------------------------------------------------------------
#  SSH

AC_ARG_ENABLE(ssh,
        AC_HELP_STRING([--disable-ssh], 
        [Support for managing SSH keys]))
        
if test "x$enable_ssh" = "xno"; then
    echo "disabling ssh support"
else

    AC_PATH_PROG(SSH_KEYGEN_PATH, ssh-keygen, "failed")
    AC_PATH_PROG(SSH_PATH, ssh, "failed")
    
    if test $SSH_KEYGEN_PATH != "failed" && 
       test $SSH_PATH != "failed"; then
        enable_ssh="yes"
        AC_DEFINE(WITH_SSH, 1, [With SSH Support])
        AC_DEFINE_UNQUOTED(SSH_KEYGEN_PATH, "$SSH_KEYGEN_PATH", [Path to ssh-keygen])
        AC_DEFINE_UNQUOTED(SSH_PATH, "$SSH_PATH", [Path to ssh])
    fi
    
fi

AM_CONDITIONAL(WITH_SSH, test "$enable_ssh" = "yes")

# -----------------------------------------------------------------------------
# Compilation and linking options
#

AC_MSG_CHECKING([for debug mode])
AC_ARG_ENABLE(debug,
              AC_HELP_STRING([--enable-debug=no/default/yes],
              [Turn on or off debugging]))

if test "$enable_debug" != "no"; then
	AC_DEFINE_UNQUOTED(WITH_DEBUG, 1, [Print debug output])
	AC_DEFINE_UNQUOTED(_DEBUG, 1, [In debug mode])
	CFLAGS="$CFLAGS -g"
fi

if test "$enable_debug" = "yes"; then
	debug_status="yes (-g, -O0, debug output, testable)"
	CFLAGS="$CFLAGS -O0"
elif test "$enable_debug" = "no"; then
	debug_status="no (no debug output, not testable, G_DISABLE_ASSERT)"
	AC_DEFINE_UNQUOTED(G_DISABLE_ASSERT, 1, [Disable glib assertions])
else
	debug_status="default (-g, debug output, testable)"
fi

AC_MSG_RESULT($debug_status)

AC_MSG_CHECKING(for more warnings)
if test "$GCC" = "yes"; then
	AC_MSG_RESULT(yes)
	CFLAGS="\
	-Wall \
	-Wchar-subscripts -Wmissing-declarations -Wmissing-prototypes \
	-Wnested-externs -Wpointer-arith \
	-Wcast-align -Wsign-compare \
	$CFLAGS"

	for option in -Wno-strict-aliasing -Wno-sign-compare; do
		SAVE_CFLAGS="$CFLAGS"
		CFLAGS="$CFLAGS $option"
		AC_MSG_CHECKING([whether gcc understands $option])
		AC_TRY_COMPILE([], [],
			has_option=yes,
			has_option=no,)
		if test $has_option = no; then
			CFLAGS="$SAVE_CFLAGS"
		fi
		AC_MSG_RESULT($has_option)
		unset has_option
		unset SAVE_CFLAGS
	done
	unset option
else
	AC_MSG_RESULT(no)
fi

AC_ARG_ENABLE(strict, [
               AS_HELP_STRING([--enable-strict], [Strict code compilation])
             ])

AC_MSG_CHECKING([build strict])

if test "$enable_strict" = "yes"; then
	CFLAGS="$CFLAGS -Werror \
		-DGTK_DISABLE_DEPRECATED \
		-DGDK_DISABLE_DEPRECATED \
		-DG_DISABLE_DEPRECATED \
		-DGDK_PIXBUF_DISABLE_DEPRECATED"
	AC_DEFINE_UNQUOTED(WITH_STRICT, 1, [More strict checks])
	strict_status="yes (-Werror, fatals, no deprecations)"
else
	strict_status="no (non-fatal warnings)"
fi

AC_MSG_RESULT($strict_status)

# -----------------------------------------------------------------------------

AC_SUBST(SEAHORSE_CFLAGS)
AC_SUBST(SEAHORSE_LIBS)

AC_OUTPUT([
Makefile
gkr/Makefile
libegg/Makefile
libseahorse/Makefile
pgp/Makefile
po/Makefile.in
help/Makefile
data/Makefile
data/icons/Makefile
pkcs11/Makefile
src/Makefile
ssh/Makefile
])

echo "
PKCS11 Support:             $enable_pkcs11
PGP Support:                $enable_pgp
  GnuPG Version:            $have_gpg
  GPGME Version:            $have_gpgme
SSH Support:                $enable_ssh
Keyserver Support:          $with_keyserver
  LDAP:                     $with_ldap
  HKP:                      $enable_hkp
  Key Sharing:              $enable_sharing
Debug:                      $debug_status
Strict:                     $strict_status
"
