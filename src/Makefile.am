
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

bin_PROGRAMS = seahorse

seahorse_SOURCES = main.c \
	seahorse-key-widget.c seahorse-key-widget.h \
	seahorse-key-manager-store.c seahorse-key-manager-store.h \
	seahorse-key-op.c seahorse-key-op.h \
	seahorse-preferences.c seahorse-preferences.h \
	seahorse-key-manager.c \
	seahorse-generate-select.c \
	seahorse-generate-druid.c seahorse-generate-druid.h \
	seahorse-generate-adv.c seahorse-generate-adv.h \
	seahorse-windows.h \
	seahorse-key-properties.c \
	seahorse-revoke.c \
	seahorse-sign.c \
	seahorse-delete.c \
	seahorse-add-uid.c \
	seahorse-add-subkey.c \
	seahorse-key-dialogs.h \
	seahorse-process-multiple.c

seahorse_LDADD = \
	$(top_builddir)/libseahorse/libseahorse.a \
	$(SEAHORSE_LIBS)

glade_DATA = \
	seahorse-key-manager.glade \
	seahorse-key-properties.glade \
	seahorse-revoke.glade \
	seahorse-change-passphrase.glade \
	seahorse-sign.glade \
	seahorse-generate-select.glade \
	seahorse-generate-druid.glade \
	seahorse-generate-adv.glade \
	seahorse-preferences.glade \
	seahorse-add-uid.glade \
	seahorse-add-subkey.glade \
	seahorse-multi-encrypt.glade

@INTLTOOL_DESKTOP_RULE@
desktopdir = $(datadir)/applications
desktop_in_files = seahorse.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

EXTRA_DIST = \
	$(glade_DATA) \
	$(desktop_in_files)

CLEANFILES = *.gladep* *.bak \
	$(desktop_DATA)
