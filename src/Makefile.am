
gladedir = $(datadir)/seahorse/glade/
localedir = $(datadir)/locale
pixmapsdir = $(datadir)/pixmaps/

INCLUDES = -I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libseahorse \
	$(SEAHORSE_CFLAGS) \
	-DDATA_DIR=\""$(datadir)"\" \
	-DSEAHORSE_GLADEDIR=\""$(gladedir)"\" \
	-DLOCALEDIR=\"$(localedir)\" \
	-DPIXMAPSDIR=\""$(pixmapsdir)"\"

if WITH_KEYSERVER
KEYSERVER_SRCS = seahorse-keyserver-search.c seahorse-keyserver-results.c
else
KEYSERVER_SRCS = 
endif

bin_PROGRAMS = seahorse

seahorse_SOURCES = main.c \
	seahorse-key-widget.c seahorse-key-widget.h \
	seahorse-key-manager-store.c seahorse-key-manager-store.h \
	seahorse-key-op.c seahorse-key-op.h \
	seahorse-preferences.c seahorse-preferences.h \
	seahorse-key-manager.c eggtreemultidnd.c eggtreemultidnd.h \
	seahorse-generate-select.c \
	seahorse-generate-druid.c seahorse-generate-druid.h \
	seahorse-generate-adv.c seahorse-generate-adv.h \
	seahorse-windows.h \
	seahorse-key-properties.c \
	seahorse-revoke.c \
    seahorse-expires.c \
	seahorse-sign.c \
	seahorse-delete.c \
	seahorse-add-uid.c \
	seahorse-add-subkey.c \
	seahorse-key-dialogs.h \
	seahorse-process-multiple.c \
	$(KEYSERVER_SRCS) 	

seahorse_LDADD = \
	$(top_builddir)/libseahorse/libseahorse-internal.la \
	$(SEAHORSE_LIBS)

glade_DATA = \
	seahorse-key-manager.glade \
	seahorse-key-properties.glade \
	seahorse-revoke.glade \
    seahorse-expires.glade \
	seahorse-change-passphrase.glade \
	seahorse-sign.glade \
	seahorse-generate-select.glade \
	seahorse-generate-druid.glade \
	seahorse-generate-adv.glade \
	seahorse-preferences.glade \
	seahorse-add-uid.glade \
	seahorse-add-subkey.glade \
	seahorse-multi-encrypt.glade \
	seahorse-keyserver-search.glade \
	seahorse-keyserver-results.glade

@INTLTOOL_DESKTOP_RULE@
desktopdir = $(datadir)/applications
desktop_in_files = seahorse.desktop.in \
				   seahorse-pgp-encrypted.desktop.in \
				   seahorse-pgp-signature.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

EXTRA_DIST = \
	$(glade_DATA) \
	$(desktop_in_files)

CLEANFILES = *.gladep* *.bak \
	$(desktop_DATA)
