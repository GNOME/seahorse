SUBDIRS = dialogs

gladedir = $(datadir)/seahorse/glade/
localedir = $(datadir)/locale

bin_PROGRAMS = seahorse

INCLUDES = -I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(srcdir)/dialogs \
	$(SEAHORSE_CFLAGS) \
	-DDATA_DIR=\""$(datadir)"\" \
	-DSEAHORSE_GLADEDIR=\""$(gladedir)"\" \
	-DLOCALEDIR=\"$(localedir)\"

seahorse_LDADD = \
	dialogs/libdialogs.a \
	$(SEAHORSE_LIBS)

seahorse_SOURCES = main.c \
	seahorse-key.c seahorse-key.h \
	seahorse-context.c seahorse-context.h \
	seahorse-widget.c seahorse-widget.h \
	seahorse-key-manager.c seahorse-key-manager.h \
	seahorse-util.c seahorse-util.h \
	seahorse-key-widget.c seahorse-key-widget.h \
	seahorse-ops-data.c seahorse-ops-data.h \
	seahorse-ops-text.c seahorse-ops-text.h \
	seahorse-ops-file.c seahorse-ops-file.h \
	seahorse-ops-key.c seahorse-ops-key.h \
	seahorse-text-editor.c seahorse-text-editor.h \
	seahorse-key-properties.c seahorse-key-properties.h \
	seahorse-validity.c seahorse-validity.h \
	seahorse-key-store.c seahorse-key-store.h \
	seahorse-key-manager-store.c seahorse-key-manager-store.h \
	seahorse-recipients-store.c seahorse-recipients-store.h \
	seahorse-key-menu.c seahorse-key-menu.h \
	seahorse-key-menu-item.c seahorse-key-menu-item.h \
	seahorse-file-manager.c seahorse-file-manager.h \
	seahorse-encrypt-recipients-store.c seahorse-encrypt-recipients-store.h

glade_DATA = seahorse-key-manager.glade2 \
	seahorse-text-editor.glade2 \
	seahorse-key-properties.glade2 \
	seahorse-file-manager.glade2

EXTRA_DIST = $(glade_DATA)
