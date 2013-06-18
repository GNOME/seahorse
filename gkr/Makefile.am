
pkgdatadir = $(datadir)/seahorse
uidir = $(pkgdatadir)/ui
localedir = $(datadir)/locale
seahorselibexecbindir = $(libdir)/seahorse/

INCLUDES = -I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/common \
	-I$(top_srcdir)/libseahorse \
	$(SEAHORSE_CFLAGS) \
	-DSEAHORSE_UIDIR=\""$(uidir)/"\" \
	-DLOCALEDIR=\"$(localedir)\" \
	-DEXECDIR=\""$(seahorselibexecbindir)"\" \
	-DGETTEXT_PACKAGE=\""seahorse\"" \
	-DSECRET_API_SUBJECT_TO_CHANGE

noinst_LTLIBRARIES = \
	libvala-code.la \
	libseahorse-gkr.la

AM_VALAFLAGS = \
	$(VALA_FLAGS) \
	--use-header \
	--header=seahorse-gkr.h \
	--vapidir $(top_builddir)/common \
	--pkg common \
	--pkg gtk+-3.0 \
	--pkg gcr-3 \
	--pkg libsecret-1 \
	$(NULL)

libvala_code_la_SOURCES = \
	gkr-dialogs.vala \
	gkr-module.vala \
	$(NULL)

libvala_code_la_CFLAGS = \
	-include config.h -w \
	$(NULL)

libseahorse_gkr_la_SOURCES = \
	seahorse-gkr-actions.c seahorse-gkr-actions.h \
	seahorse-gkr-add-item.c \
	seahorse-gkr-add-keyring.c \
	seahorse-gkr-backend.c seahorse-gkr-backend.h \
	seahorse-gkr-dialogs.h \
	seahorse-gkr-item.c seahorse-gkr-item.h \
	seahorse-gkr-item-deleter.c seahorse-gkr-item-deleter.h \
	seahorse-gkr-item-properties.c \
	seahorse-gkr-keyring.c seahorse-gkr-keyring.h \
	seahorse-gkr-keyring-deleter.c seahorse-gkr-keyring-deleter.h \
	seahorse-gkr-keyring-properties.c

libseahorse_gkr_la_LIBADD = \
	$(top_builddir)/common/libcommon.la \
	$(top_builddir)/libseahorse/libseahorse.la \
	libvala-code.la \
	$(NULL)

ui_DATA = \
	seahorse-add-keyring.xml \
	seahorse-gkr-add-item.xml \
	seahorse-gkr-item-properties.xml \
	seahorse-gkr-keyring.xml

EXTRA_DIST = \
	$(ui_DATA) \
	seahorse-gkr.h \
	$(NULL)
