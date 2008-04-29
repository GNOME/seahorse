
gladedir = $(datadir)/seahorse/glade/
localedir = $(datadir)/locale
seahorselibexecbindir = $(libdir)/seahorse/

INCLUDES = -I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libcryptui \
	-I$(top_srcdir)/libseahorse \
	$(SEAHORSE_CFLAGS) \
	-DDATA_DIR=\""$(datadir)"\" \
	-DSEAHORSE_GLADEDIR=\""$(gladedir)"\" \
	-DLOCALEDIR=\"$(localedir)\" \
	-DEXECDIR=\""$(seahorselibexecbindir)"\" \
	-DLIBCRYPTUI_API_SUBJECT_TO_CHANGE 

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
else
KEYSERVER_SRCS = 
endif

libseahorse_pgp_la_SOURCES = \
	seahorse-gpgme-io.c seahorse-gpgme-io.h \
	seahorse-gpgmex.h seahorse-gpgmex-op.c seahorse-gpgmex-util.c \
	seahorse-gpg-options.c seahorse-gpg-options.h \
	seahorse-pgp.c seahorse-pgp.h \
	seahorse-pgp-keysets.c seahorse-pgp-keysets.h \
	seahorse-pgp-error.c \
	seahorse-pgp-key.c seahorse-pgp-key.h \
	seahorse-pgp-key-op.c seahorse-pgp-key-op.h \
	seahorse-pgp-source.c seahorse-pgp-source.h \
	seahorse-pgp-operation.c seahorse-pgp-operation.h \
	seahorse-signer.c seahorse-pgp-dialogs.h \
	$(KEYSERVER_SRCS) \
	$(LDAP_SRCS) \
	$(HKP_SRCS)

libseahorse_pgp_la_LIBADD = \
	$(top_builddir)/libseahorse/libseahorse.la \
	$(top_builddir)/common/libseahorse-common.la \
	$(top_builddir)/libcryptui/libcryptui.la

glade_DATA = \
	seahorse-signer.glade

EXTRA_DIST = $(glade_DATA)
	