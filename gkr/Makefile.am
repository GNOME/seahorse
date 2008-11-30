
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
	-DGETTEXT_PACKAGE=\""seahorse\""

noinst_LTLIBRARIES = libseahorse-gkr.la

libseahorse_gkr_la_SOURCES = \
	seahorse-gkr.c seahorse-gkr.h \
	seahorse-gkr-commands.c seahorse-gkr-commands.h \
	seahorse-gkr-dialogs.h \
	seahorse-gkr-item.c seahorse-gkr-item.h \
	seahorse-gkr-item-properties.c \
	seahorse-gkr-keyring.c seahorse-gkr-keyring.h \
	seahorse-gkr-keyring-properties.c \
	seahorse-gkr-module.c seahorse-gkr-module.h \
	seahorse-gkr-operation.c seahorse-gkr-operation.h \
	seahorse-gkr-source.c seahorse-gkr-source.h
	
libseahorse_gkr_la_LIBADD = \
	$(top_builddir)/libseahorse/libseahorse.la \
	$(top_builddir)/common/libseahorse-common.la \
	$(top_builddir)/libcryptui/libcryptui.la

glade_DATA = \
	seahorse-gkr-item-properties.glade \
	seahorse-gkr-keyring.glade

EXTRA_DIST = \
	$(glade_DATA)

