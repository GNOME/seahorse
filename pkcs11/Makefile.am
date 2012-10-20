
uidir = $(datadir)/seahorse/ui/
localedir = $(datadir)/locale
seahorselibexecbindir = $(libdir)/seahorse/

INCLUDES = -I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/common \
	-I$(top_srcdir)/libseahorse \
	$(SEAHORSE_CFLAGS) \
	-DSEAHORSE_UIDIR=\""$(uidir)"\" \
	-DLOCALEDIR=\"$(localedir)\" \
	-DEXECDIR=\""$(seahorselibexecbindir)"\" \
	-DGETTEXT_PACKAGE=\""seahorse\""

noinst_LTLIBRARIES = libseahorse-pkcs11.la

libseahorse_pkcs11_la_SOURCES = \
	seahorse-certificate.c seahorse-certificate.h \
	seahorse-certificate-der-exporter.c seahorse-certificate-der-exporter.h \
	seahorse-pkcs11-backend.c seahorse-pkcs11-backend.h \
	seahorse-pkcs11-deleter.c seahorse-pkcs11-deleter.h \
	seahorse-pkcs11-helpers.c seahorse-pkcs11-helpers.h \
	seahorse-pkcs11-generate.c seahorse-pkcs11-generate.h \
	seahorse-pkcs11-key-deleter.c seahorse-pkcs11-key-deleter.h \
	seahorse-pkcs11-properties.c seahorse-pkcs11-properties.h \
	seahorse-pkcs11-request.c seahorse-pkcs11-request.h \
	seahorse-pkcs11.c seahorse-pkcs11.h \
	seahorse-private-key.c seahorse-private-key.h \
	seahorse-token.c seahorse-token.h

libseahorse_pkcs11_la_LIBADD = \
	$(top_builddir)/common/libcommon.la \
	$(top_builddir)/libseahorse/libseahorse.la \
	$(GCR_LIBS)

ui_DATA = \
	seahorse-pkcs11-generate.xml \
	seahorse-pkcs11-request.xml

EXTRA_DIST = \
	$(ui_DATA)
