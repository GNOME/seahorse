
uidir = $(datadir)/seahorse/ui/
localedir = $(datadir)/locale
seahorselibexecbindir = $(libdir)/seahorse/

INCLUDES = -I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libseahorse \
	$(SEAHORSE_CFLAGS) \
	-DSEAHORSE_UIDIR=\""$(uidir)"\" \
	-DLOCALEDIR=\"$(localedir)\" \
	-DEXECDIR=\""$(seahorselibexecbindir)"\" \
	-DGETTEXT_PACKAGE=\""seahorse\""

noinst_LTLIBRARIES = libseahorse-pkcs11.la

libseahorse_pkcs11_la_SOURCES = \
	seahorse-pkcs11-backend.c seahorse-pkcs11-backend.h \
	seahorse-pkcs11-certificate.c seahorse-pkcs11-certificate.h \
	seahorse-pkcs11-certificate-props.c seahorse-pkcs11-certificate-props.h \
	seahorse-pkcs11-commands.c seahorse-pkcs11-commands.h \
	seahorse-pkcs11-helpers.c seahorse-pkcs11-helpers.h \
	seahorse-pkcs11-object.c seahorse-pkcs11-object.h \
	seahorse-pkcs11-operations.c seahorse-pkcs11-operations.h \
	seahorse-pkcs11-token.c seahorse-pkcs11-token.h\
	seahorse-pkcs11.c seahorse-pkcs11.h

libseahorse_pkcs11_la_LIBADD = \
	$(top_builddir)/libseahorse/libseahorse.la \
	$(GCR_LIBS)

ui_DATA =

EXTRA_DIST = \
	$(ui_DATA)
