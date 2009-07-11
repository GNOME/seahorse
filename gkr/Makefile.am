
uidir = $(datadir)/seahorse/ui/
localedir = $(datadir)/locale
seahorselibexecbindir = $(libdir)/seahorse/

INCLUDES = -I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libcryptui \
	-I$(top_srcdir)/libseahorse \
	$(SEAHORSE_CFLAGS) \
	-DDATA_DIR=\""$(datadir)"\" \
	-DSEAHORSE_UIDIR=\""$(uidir)"\" \
	-DLOCALEDIR=\"$(localedir)\" \
	-DEXECDIR=\""$(seahorselibexecbindir)"\" \
	-DLIBCRYPTUI_API_SUBJECT_TO_CHANGE \
	-DGETTEXT_PACKAGE=\""seahorse\""

noinst_LTLIBRARIES = libseahorse-gkr.la

libseahorse_gkr_la_SOURCES = \
	seahorse-gkr.c seahorse-gkr.h \
	seahorse-gkr-add-keyring.c \
	seahorse-gkr-dialogs.h \
	seahorse-gkr-item.c seahorse-gkr-item.h \
	seahorse-gkr-item-commands.c seahorse-gkr-item-commands.h \
	seahorse-gkr-item-properties.c \
	seahorse-gkr-keyring.c seahorse-gkr-keyring.h \
	seahorse-gkr-keyring-commands.c seahorse-gkr-keyring-commands.h \
	seahorse-gkr-keyring-properties.c \
	seahorse-gkr-module.c seahorse-gkr-module.h \
	seahorse-gkr-operation.c seahorse-gkr-operation.h \
	seahorse-gkr-source.c seahorse-gkr-source.h
	
libseahorse_gkr_la_LIBADD = \
	$(top_builddir)/libseahorse/libseahorse.la \
	$(top_builddir)/common/libseahorse-common.la \
	$(top_builddir)/libcryptui/libcryptui.la

ui_DATA = \
	seahorse-add-keyring.xml \
	seahorse-gkr-item-properties.xml \
	seahorse-gkr-keyring.xml

EXTRA_DIST = \
	$(ui_DATA)

