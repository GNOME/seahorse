
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

noinst_LTLIBRARIES = libseahorse-gkr.la

libseahorse_gkr_la_SOURCES = \
	seahorse-gkr.c seahorse-gkr.h \
	seahorse-gkeyring-item.c seahorse-gkeyring-item.h \
	seahorse-gkeyring-source.c seahorse-gkeyring-source.h \
	seahorse-gkeyring-operation.c seahorse-gkeyring-operation.h
	
libseahorse_gkr_la_LIBADD = \
	$(top_builddir)/libseahorse/libseahorse.la \
	$(top_builddir)/common/libseahorse-common.la \
	$(top_builddir)/libcryptui/libcryptui.la
