
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
	seahorse-generate.c \
	seahorse-import.c \
	seahorse-export.c \
	seahorse-windows.h \
	seahorse-key-properties.c \
	seahorse-revoke.c \
	seahorse-sign.c \
	seahorse-delete.c \
	seahorse-add-uid.c \
	seahorse-add-subkey.c \
	seahorse-key-dialogs.h

seahorse_LDADD = \
	$(top_builddir)/libseahorse/libseahorse.a \
	$(SEAHORSE_LIBS)

glade_DATA = \
	seahorse-key-manager.glade \
	seahorse-key-properties.glade \
	seahorse-revoke.glade \
	seahorse-change-passphrase.glade \
	seahorse-sign.glade \
	seahorse-import.glade \
	seahorse-export.glade \
	seahorse-generate.glade \
	seahorse-preferences.glade \
	seahorse-add-uid.glade \
	seahorse-add-subkey.glade

@INTLTOOL_DESKTOP_RULE@
desktopdir = $(datadir)/applications
desktop_in_files = seahorse.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

EXTRA_DIST = \
	$(glade_DATA) \
	$(desktop_in_files)

CLEANFILES = *.gladep* *.bak \
	$(desktop_DATA)
