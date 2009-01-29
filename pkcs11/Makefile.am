
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
	-DLIBCRYPTUI_API_SUBJECT_TO_CHANGE \
	-DGCR_API_SUBJECT_TO_CHANGE \
	-DGETTEXT_PACKAGE=\""seahorse\""

noinst_LTLIBRARIES = libseahorse-pkcs11.la

libseahorse_pkcs11_la_SOURCES = \
	seahorse-pkcs11-certificate.c seahorse-pkcs11-certificate.h \
	seahorse-pkcs11-certificate-props.c seahorse-pkcs11-certificate-props.h \
	seahorse-pkcs11-commands.c seahorse-pkcs11-commands.h \
	seahorse-pkcs11-module.c seahorse-pkcs11-module.h \
	seahorse-pkcs11-helpers.c seahorse-pkcs11-helpers.h \
	seahorse-pkcs11-object.c seahorse-pkcs11-object.h \
	seahorse-pkcs11-operations.c seahorse-pkcs11-operations.h \
	seahorse-pkcs11-source.c seahorse-pkcs11-source.h\
	seahorse-pkcs11.c seahorse-pkcs11.h

libseahorse_pkcs11_la_LIBADD = \
	$(top_builddir)/libseahorse/libseahorse.la \
	$(top_builddir)/libcryptui/libcryptui.la

glade_DATA = 

EXTRA_DIST = \
	$(glade_DATA) 
