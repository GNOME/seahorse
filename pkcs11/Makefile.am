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
	-DUIDIR=\""$(uidir)"\" \
	-DLOCALEDIR=\"$(localedir)\" \
	-DEXECDIR=\""$(seahorselibexecbindir)"\" \
	-DGETTEXT_PACKAGE=\""seahorse\""

noinst_LTLIBRARIES = \
	libvala-code.la \
	libseahorse-pkcs11.la

AM_VALAFLAGS = \
	$(VALA_FLAGS) \
	--header=seahorse-pkcs11.h \
	--use-header \
	--vapidir $(top_builddir)/common \
	--pkg config \
	--pkg common \
	--pkg gtk+-3.0 \
	--pkg gcr-3 \
	--pkg gcr-ui-3 \
	--pkg pkcs11 \
	$(NULL)

libvala_code_la_SOURCES = \
	certificate-der-exporter.vala \
	pkcs11-certificate.vala \
	pkcs11-deleter.vala \
	pkcs11-key-deleter.vala \
	pkcs11-module.vala \
	pkcs11-private-key.vala \
	pkcs11-properties.vala \
	pkcs11-request.vala \
	pkcs11-token.vala \
	$(NULL)

libvala_code_la_CFLAGS = \
	-include seahorse-pkcs11.h -w \
	$(NULL)

libseahorse_pkcs11_la_SOURCES = \
	seahorse-pkcs11-backend.c seahorse-pkcs11-backend.h \
	seahorse-pkcs11-generate.c seahorse-pkcs11-generate.h

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
