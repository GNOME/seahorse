
uidir = $(datadir)/seahorse/ui/
localedir = $(datadir)/locale
seahorselibexecbindir = $(libdir)/seahorse/

AM_CPPFLAGS = -I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libegg \
	-I$(top_srcdir)/common \
	-I$(top_builddir)/common \
	-I$(top_srcdir)/libseahorse \
	$(SEAHORSE_CFLAGS) \
	-DSEAHORSE_UIDIR=\""$(uidir)"\" \
	-DLOCALEDIR=\"$(localedir)\" \
	-DEXECDIR=\""$(seahorselibexecbindir)"\" \
	-DLDAP_DEPRECATED \
	-DGETTEXT_PACKAGE=\""seahorse\""

noinst_LTLIBRARIES = libseahorse-pgp.la

if WITH_LDAP
LDAP_SRCS = seahorse-ldap-source.c seahorse-ldap-source.h
else
LDAP_SRCS = 
endif

if WITH_HKP
HKP_SRCS = seahorse-hkp-source.c seahorse-hkp-source.h
else
HKP_SRCS =
endif

if WITH_KEYSERVER
KEYSERVER_SRCS = seahorse-server-source.c seahorse-server-source.h 
KEYSERVER_SRCS += seahorse-keyserver-search.c seahorse-keyserver-search.h
KEYSERVER_SRCS += seahorse-keyserver-sync.c seahorse-keyserver-sync.h
KEYSERVER_SRCS += seahorse-keyserver-results.c seahorse-keyserver-results.h
else
KEYSERVER_SRCS = 
endif

libseahorse_pgp_la_SOURCES = \
	seahorse-combo-keys.c seahorse-combo-keys.h \
	seahorse-discovery.c seahorse-discovery.h \
	seahorse-gpgme.c seahorse-gpgme.h \
	seahorse-gpgme-add-subkey.c \
	seahorse-gpgme-add-uid.c \
	seahorse-gpgme-dialogs.h \
	seahorse-gpgme-data.c seahorse-gpgme-data.h \
	seahorse-gpgme-expires.c \
	seahorse-gpgme-exporter.c seahorse-gpgme-exporter.h \
	seahorse-gpgme-generate.c \
	seahorse-gpgme-key.c seahorse-gpgme-key.h \
	seahorse-gpgme-key-deleter.c seahorse-gpgme-key-deleter.h \
	seahorse-gpgme-key-op.c seahorse-gpgme-key-op.h \
	seahorse-gpgme-keyring.c seahorse-gpgme-keyring.h \
	seahorse-gpgme-photo.c seahorse-gpgme-photo.h \
	seahorse-gpgme-photos.c \
	seahorse-gpgme-revoke.c \
	seahorse-gpgme-secret-deleter.c seahorse-gpgme-secret-deleter.h \
	seahorse-gpgme-sign.c \
	seahorse-gpgme-subkey.c seahorse-gpgme-subkey.h \
	seahorse-gpgme-uid.c seahorse-gpgme-uid.h \
	seahorse-gpg-op.h seahorse-gpg-op.c \
	seahorse-gpg-options.c seahorse-gpg-options.h \
	seahorse-pgp.c seahorse-pgp.h \
	seahorse-pgp-actions.c seahorse-pgp-actions.h \
	seahorse-pgp-backend.c seahorse-pgp-backend.h \
	seahorse-pgp-key.c seahorse-pgp-key.h \
	seahorse-pgp-key-properties.c \
	seahorse-pgp-keysets.c seahorse-pgp-keysets.h \
	seahorse-pgp-photo.c seahorse-pgp-photo.h \
	seahorse-pgp-signature.c seahorse-pgp-signature.h \
	seahorse-pgp-subkey.c seahorse-pgp-subkey.h \
	seahorse-pgp-uid.c seahorse-pgp-uid.h \
	seahorse-signer.c seahorse-pgp-dialogs.h \
	seahorse-transfer.c seahorse-transfer.h \
	seahorse-unknown.c seahorse-unknown.h \
	seahorse-unknown-source.c seahorse-unknown-source.h \
	$(VALA_CFILES) $(VALA_HFILES) \
	$(KEYSERVER_SRCS) \
	$(LDAP_SRCS) \
	$(HKP_SRCS)

libseahorse_pgp_la_LIBADD = \
	$(top_builddir)/common/libcommon.la \
	$(top_builddir)/libseahorse/libseahorse.la

seahorselibexecbin_PROGRAMS = xloadimage

xloadimage_SOURCES = seahorse-xloadimage.c

ui_DATA = \
	seahorse-add-subkey.xml \
	seahorse-add-uid.xml \
	seahorse-expires.xml \
	seahorse-keyserver-search.xml \
	seahorse-keyserver-results.ui \
	seahorse-keyserver-results.xml \
	seahorse-keyserver-sync.xml \
	seahorse-pgp-public-key-properties.xml \
	seahorse-pgp-private-key-properties.xml \
	seahorse-pgp-generate.xml \
	seahorse-revoke.xml \
	seahorse-sign.xml \
	seahorse-signer.xml

EXTRA_DIST = \
	$(ui_DATA)

