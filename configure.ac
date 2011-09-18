dnl Process this file with autoconf to produce a configure script.

AC_PREREQ(2.52)

dnl ****************************************************************************
dnl MAIN SEAHORSE VERSION (update after release)

AC_INIT(seahorse, 3.1.92)

GCR_REQUIRED=3.1.5

dnl ****************************************************************************

AC_CONFIG_SRCDIR(src/seahorse-main.c)
AM_INIT_AUTOMAKE(AC_PACKAGE_NAME, AC_PACKAGE_VERSION)
AM_CONFIG_HEADER(config.h)
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])
AM_MAINTAINER_MODE([enable])
AC_CONFIG_MACRO_DIR([m4])

AC_PROG_CC
AC_PROG_CXX
AC_PROG_INTLTOOL([0.35.0])
AM_PROG_LIBTOOL
GNOME_DOC_INIT

dnl Get the off_t size right
AC_SYS_LARGEFILE
AC_TYPE_OFF_T
AC_FUNC_FSEEKO

AC_CHECK_FUNCS(setresuid setresgid)
AC_CHECK_FUNCS(strsep)

AC_PATH_PROG(PKG_CONFIG, pkg-config, no)

GTK_REQUIRED=2.90.0

PKG_CHECK_MODULES(GTK, gtk+-3.0 >= $GTK_REQUIRED)
AC_SUBST(GTK_CFLAGS)
AC_SUBST(GTK_LIBS)

PKG_CHECK_MODULES(SEAHORSE, gmodule-2.0 gio-2.0 gthread-2.0 gtk+-3.0 >= $GTK_REQUIRED)

GLIB_GSETTINGS

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

dnl **************************************************************
dnl GPG / GPGME CHECKS
dnl **************************************************************

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
		accepted_versions="1.2 1.4 2.0"
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
    
			for ver in $accepted_versions; do
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
	min_gpgme_version=1.0.0
	AC_PATH_PROG(GPGME_CONFIG, gpgme-config, "failed")
	if test $GPGME_CONFIG != "failed" ; then
      		AC_MSG_CHECKING(for GPGME - version >= $min_gpgme_version)
		req_major=`echo $min_gpgme_version | \
			sed 's/\([[0-9]]*\)\.\([[0-9]]*\)\.\([[0-9]]*\)/\1/'`
		req_minor=`echo $min_gpgme_version | \
			sed 's/\([[0-9]]*\)\.\([[0-9]]*\)\.\([[0-9]]*\)/\2/'`
		req_micro=`echo $min_gpgme_version | \
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
		AC_MSG_ERROR([Appropriate version of GnuPG not found. Please install one of versions: $accepted_versions])
	fi
			
	if test -z "$have_gpgme"; then
		AC_MSG_ERROR(GPGME $min_gpgme_version or later needed)
	fi

	SEAHORSE_CFLAGS="$SEAHORSE_CFLAGS $GPGME_CFLAGS"
	SEAHORSE_LIBS="$SEAHORSE_LIBS $GPGME_LIBS"
        AC_DEFINE(WITH_PGP, 1, [With PGP Support])
fi

AM_CONDITIONAL(WITH_PGP, test "$enable_pgp" = "yes")

dnl ******************************************************************************
dnl LDAP [from gpg]
dnl ******************************************************************************

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


dnl ****************************************************************************
dnl HKP (libsoup)
dnl ****************************************************************************

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

    PKG_CHECK_MODULES(SOUP, libsoup-2.4, 
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


dnl ***************
dnl KEYSERVER
dnl ***************

AM_CONDITIONAL(WITH_KEYSERVER, test "$with_keyserver" = "yes")

if test "$with_keyserver" = "yes"; then
	echo "enabling key server support"
	AC_DEFINE(WITH_KEYSERVER, 1, [Support for key server actions])
else
    with_keyserver=no
	echo "disabling key server support"
fi


dnl ****************************************************************************
dnl GNOME-KEYRING
dnl ****************************************************************************

PKG_CHECK_MODULES([GNOME_KEYRING], gnome-keyring-1 >= 2.25.5,
                  [enable_gnome_keyring=yes],[enable_gnome_keyring=no])

if test "$enable_gnome_keyring" = "no"; then
    AC_MSG_ERROR([gnome-keyring library or headers not found.])
fi

SEAHORSE_CFLAGS="$SEAHORSE_CFLAGS $GNOME_KEYRING_CFLAGS"
SEAHORSE_LIBS="$SEAHORSE_LIBS $GNOME_KEYRING_LIBS"
        

dnl *******************
dnl KEY SHARING 
dnl *******************

AVAHI_GLIB_REQUIRED=0.6

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

dnl ****************************************************************************
dnl  PKCS11
dnl ****************************************************************************

AC_ARG_ENABLE(pkcs11,
	AC_HELP_STRING([--disable-pkcs11],
		[Support keys and certificates via PKCS#11]))

if test "$enable_pkcs11" = "no"; then
	echo "disabling pkcs11 support"
	enable_pkcs11="no"
else
	PKG_CHECK_MODULES(GCR, gcr-3 >= $GCR_REQUIRED, gcr_is_present="yes", gcr_is_present="no")
	if test "$gcr_is_present" = "no"; then
		echo "disabling pkcs11 support"
	fi
	PKG_CHECK_MODULES(GCK, gck-1 >= 3.1.2, enable_pkcs11="yes", enable_pkcs11="no")
	if test "$enable_pkcs11" = "yes" && test "$gcr_is_present" = "yes"; then
		AC_DEFINE(WITH_PKCS11, 1, [gck library available])
		SEAHORSE_CFLAGS="$SEAHORSE_CFLAGS $GCK_CFLAGS $GCR_CFLAGS"
		SEAHORSE_LIBS="$SEAHORSE_LIBS $GCK_LIBS $GCR_LIBS"
	else
		enable_pkcs11="no"
	fi
fi

AM_CONDITIONAL(WITH_PKCS11, test "$enable_pkcs11" = "yes")

dnl ****************************************************************************
dnl  SSH 
dnl ****************************************************************************

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
# Debug mode
#

AC_MSG_CHECKING([for debug mode])
AC_ARG_ENABLE(debug,
              AC_HELP_STRING([--enable-debug=no/yes/full],
              [Turn on or off debugging]))

if test "$enable_debug" != "no"; then
	AC_DEFINE_UNQUOTED(WITH_DEBUG, 1, [Print debug output])
	AC_DEFINE_UNQUOTED(_DEBUG, 1, [In debug mode])
fi

if test "$enable_debug" = "full"; then
	debug_status="full"
	CFLAGS="$CFLAGS -g -O0 -Werror"
	CFLAGS="$CFLAGS -DG_DISABLE_DEPRECATED -DGDK_PIXBUF_DISABLE_DEPRECATED"
	CFLAGS="$CFLAGS -DGDK_DISABLE_DEPRECATED -DGTK_DISABLE_DEPRECATED"
elif test "$enable_debug" = "no"; then
	debug_status="no"
	AC_DEFINE_UNQUOTED(G_DISABLE_ASSERT, 1, [Disable glib assertions])
else
	debug_status="yes"
fi

AC_MSG_RESULT($debug_status)

# Fatal Messages mode
AC_ARG_ENABLE(fatal-messages, 
	    AC_HELP_STRING([--enable-fatal-messages],
	    [Don't make compile and or runtime messages fatal in debug mode.]))

if test "$enable_fatal_messages" = "yes" && test "$enable_debug" = "yes"; then
    CFLAGS="$CFLAGS -Wall -Werror"
    AC_DEFINE_UNQUOTED(FATAL_MESSAGES, 1, [Fatal runtime messages])
    echo "enabling fatal runtime messages"
fi

# Build Tests
AC_ARG_ENABLE(tests,
        AC_HELP_STRING([--enable-tests],
        [Compile Test Programs]))
        
if test "$enable_tests" != "yes"; then
    enable_tests="no"
fi

AM_CONDITIONAL(WITH_TESTS, test "$enable_tests" = "yes")

# -------------------------------------------------------------------
# More warnings

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

dnl ****************************************************************************

AC_SUBST(SEAHORSE_CFLAGS)
AC_SUBST(SEAHORSE_LIBS)

AC_PATH_PROG(GLIB_GENMARSHAL, glib-genmarshal)

AC_ARG_ENABLE(update-mime-database,
	AC_HELP_STRING([--disable-update-mime-database],
	[do not run the update-mime-database utility (mostly useful for package maintainers) ]))
AM_CONDITIONAL(UPDATE_MIME_DATABASE, test "$enable_update_mime_database" != "no")

dnl *******************
dnl ***** gettext *****
dnl *******************

GETTEXT_PACKAGE=seahorse
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE", [Name of the gettext domain.])

AM_GLIB_GNU_GETTEXT

dnl ****************************************************************************
## here we get the flags we'll actually use
PKG_CHECK_MODULES(EGG, gtk+-3.0 >= $GTK_REQUIRED)
AC_SUBST(EGG_LIBS)
AC_SUBST(EGG_CFLAGS)

PKG_CHECK_MODULES(EGG_SMCLIENT, gtk+-3.0)
AC_SUBST(EGG_SMCLIENT_LIBS)
AC_SUBST(EGG_SMCLIENT_CFLAGS)

dnl ****************************************************************************

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
pixmaps/Makefile
pixmaps/22x22/Makefile
pixmaps/48x48/Makefile
pixmaps/scalable/Makefile
pkcs11/Makefile
src/Makefile
src/seahorse.desktop.in
ssh/Makefile
])

dnl ****************************************************************************
dnl  SUMMARY
dnl ****************************************************************************

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
"
