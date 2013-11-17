
pkgdatadir = $(datadir)/seahorse
uidir = $(pkgdatadir)/ui
localedir = $(datadir)/locale
seahorselibexecbindir = $(libdir)/seahorse/

AM_CPPFLAGS = -I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/common \
	-I$(top_builddir)/common \
	-I$(top_srcdir)/libseahorse \
	$(SEAHORSE_CFLAGS) \
	-DSEAHORSE_UIDIR=\""$(uidir)/"\" \
	-DUIDIR=\""$(uidir)/"\" \
	-DLOCALEDIR=\"$(localedir)\" \
	-DEXECDIR=\""$(seahorselibexecbindir)"\" \
	-DGETTEXT_PACKAGE=\""seahorse\"" \
	-DSECRET_API_SUBJECT_TO_CHANGE

noinst_LTLIBRARIES = \
	libseahorse-gkr.la

AM_VALAFLAGS = \
	$(VALA_FLAGS) \
	--use-header \
	--header=seahorse-gkr.h \
	--vapidir $(top_builddir)/common \
	--pkg common \
	--pkg config \
	--pkg gtk+-3.0 \
	--pkg gcr-3 \
	--pkg gcr-ui-3 \
	--pkg libsecret-1 \
	--pkg libsecret-unstable \
	$(NULL)

libseahorse_gkr_la_CFLAGS = \
	-include config.h -w

libseahorse_gkr_la_SOURCES = \
	gkr-backend.vala \
	gkr-dialogs.vala \
	gkr-module.vala \
	gkr-item.vala \
	gkr-item-add.vala \
	gkr-item-properties.vala \
	gkr-keyring.vala \
	gkr-keyring-add.vala \
	gkr-keyring-properties.vala \
	$(NULL)

libseahorse_gkr_la_LIBADD = \
	$(top_builddir)/common/libcommon.la \
	$(top_builddir)/libseahorse/libseahorse.la \
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
