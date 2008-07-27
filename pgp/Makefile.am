
gladedir = $(datadir)/seahorse/glade/
localedir = $(datadir)/locale
seahorselibexecbindir = $(libdir)/seahorse/

INCLUDES = -I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libegg \
	-I$(top_srcdir)/libcryptui \
	-I$(top_srcdir)/libseahorse \
	$(SEAHORSE_CFLAGS) \
	-DDATA_DIR=\""$(datadir)"\" \
	-DSEAHORSE_GLADEDIR=\""$(gladedir)"\" \
	-DLOCALEDIR=\"$(localedir)\" \
	-DEXECDIR=\""$(seahorselibexecbindir)"\" \
	-DLDAP_DEPRECATED \
	-DLIBCRYPTUI_API_SUBJECT_TO_CHANGE \
	-DGETTEXT_PACKAGE=\""seahorse\""

VALA_SRCS = \
	seahorse-pgp.vala \
	seahorse-pgp-commands.vala \
	seahorse-pgp-generator.vala

VALA_VAPIS = \
	libseahorse-pgp-c.vapi \
	$(top_srcdir)/config.vapi \
	$(top_srcdir)/libseahorse/libseahorse-c.vapi \
	$(top_srcdir)/libseahorse/libseahorse.vapi

VALA_CFILES = $(VALA_SRCS:.vala=.c)
VALA_HFILES = $(VALA_SRCS:.vala=.h)

SUFFIXES = .vala .c .h

vala-build.stamp: $(VALA_SRCS) $(VALA_VAPIS)
	$(VALAC) -C $(VALA_PKGS) $(VALA_VAPIS) $(VALA_SRCS)
	date +'%s' > $@

BUILT_SOURCES = \
	vala-build.stamp

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
	seahorse-pgp-add-subkey.c \
	seahorse-pgp-add-uid.c \
	seahorse-pgp-error.c \
	seahorse-pgp-expires.c \
	seahorse-pgp-generate.c \
	seahorse-pgp-key.c seahorse-pgp-key.h \
	seahorse-pgp-key-op.c seahorse-pgp-key-op.h \
	seahorse-pgp-key-properties.c \
	seahorse-pgp-keysets.c seahorse-pgp-keysets.h \
	seahorse-pgp-operation.c seahorse-pgp-operation.h \
	seahorse-pgp-photos.c \
	seahorse-pgp-module.c seahorse-pgp-module.h \
	seahorse-pgp-revoke.c \
	seahorse-pgp-source.c seahorse-pgp-source.h \
	seahorse-pgp-sign.c \
	seahorse-pgp-uid.c seahorse-pgp-uid.h \
	seahorse-signer.c seahorse-pgp-dialogs.h \
	$(VALA_CFILES) $(VALA_HFILES) \
	$(KEYSERVER_SRCS) \
	$(LDAP_SRCS) \
	$(HKP_SRCS)

libseahorse_pgp_la_LIBADD = \
	$(top_builddir)/libseahorse/libseahorse.la \
	$(top_builddir)/common/libseahorse-common.la \
	$(top_builddir)/libcryptui/libcryptui.la

seahorselibexecbin_PROGRAMS = xloadimage

xloadimage_SOURCES = seahorse-xloadimage.c

glade_DATA = \
	seahorse-add-subkey.glade \
	seahorse-add-uid.glade \
	seahorse-expires.glade \
	seahorse-pgp-public-key-properties.glade \
	seahorse-pgp-private-key-properties.glade \
	seahorse-pgp-generate.glade \
	seahorse-revoke.glade \
	seahorse-sign.glade \
	seahorse-signer.glade

EXTRA_DIST = \
	$(glade_DATA) \
	libseahorse-pgp-c.vapi \
	vala-build.stamp \
	$(VALA_CFILES) $(VALA_HFILES) \
	$(VALA_SRCS)

vala-clean:
	rm -f vala-build.stamp $(VALA_CFILES) $(VALA_HFILES)

