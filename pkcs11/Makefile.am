NULL =

uidir = $(datadir)/seahorse/ui/
localedir = $(datadir)/locale
seahorselibexecbindir = $(libdir)/seahorse/

# Generate seahorse-pkcs11.h before building libseahorse_pkcs11_la_SOURCES
BUILT_SOURCES = libvala_code_la_vala.stamp

AM_CPPFLAGS = -I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/common \
	-I$(top_builddir)/common \
	-I$(top_srcdir)/libseahorse \
	$(SEAHORSE_CFLAGS) \
	-DSEAHORSE_UIDIR=\""$(uidir)"\" \
	-DLOCALEDIR=\"$(localedir)\" \
	-DEXECDIR=\""$(seahorselibexecbindir)"\" \
	-DGETTEXT_PACKAGE=\""seahorse\""

noinst_LTLIBRARIES = \
	libvala-code.la \
	libseahorse-pkcs11.la

AM_VALAFLAGS = \
	$(VALA_FLAGS) \
	--use-header \
	--header=seahorse-pkcs11.h \
	--vapidir $(top_builddir)/common \
	--pkg common \
	--pkg gtk+-3.0 \
	--pkg gcr-3 \
	$(NULL)

libvala_code_la_SOURCES = \
	certificate-der-exporter.vala \
	pkcs11-module.vala \
	$(NULL)

libvala_code_la_CFLAGS = \
	-include config.h -w \
	$(NULL)

libseahorse_pkcs11_la_SOURCES = \
	seahorse-certificate.c seahorse-certificate.h \
	seahorse-pkcs11-backend.c seahorse-pkcs11-backend.h \
	seahorse-pkcs11-deleter.c seahorse-pkcs11-deleter.h \
	seahorse-pkcs11-helpers.c seahorse-pkcs11-helpers.h \
	seahorse-pkcs11-generate.c seahorse-pkcs11-generate.h \
	seahorse-pkcs11-key-deleter.c seahorse-pkcs11-key-deleter.h \
	seahorse-pkcs11-properties.c seahorse-pkcs11-properties.h \
	seahorse-pkcs11-request.c seahorse-pkcs11-request.h \
	seahorse-private-key.c seahorse-private-key.h \
	seahorse-token.c seahorse-token.h

libseahorse_pkcs11_la_LIBADD = \
	$(top_builddir)/common/libcommon.la \
	$(top_builddir)/libseahorse/libseahorse.la \
	libvala-code.la \
	$(GCR_LIBS)

ui_DATA = \
	seahorse-pkcs11-generate.xml \
	seahorse-pkcs11-request.xml

EXTRA_DIST = \
	$(ui_DATA) \
	seahorse-pkcs11.h \
	$(NULL)
