
gladedir = $(datadir)/seahorse/glade/
localedir = $(datadir)/locale
pixmapsdir = $(datadir)/pixmaps/

INCLUDES = -I$(top_builddir) \
	-I$(top_srcdir) \
	$(SEAHORSE_CFLAGS) \
	-DDATA_DIR=\""$(datadir)"\" \
	-DSEAHORSE_GLADEDIR=\""$(gladedir)"\" \
	-DLOCALEDIR=\"$(localedir)\" \
	-DPIXMAPSDIR=\""$(pixmapsdir)"\"

noinst_LIBRARIES = libseahorse.a

BUILT_SOURCES = \
	seahorse-marshal.c seahorse-marshal.h

libseahorse_a_SOURCES = \
	seahorse-key.c seahorse-key.h \
	seahorse-key-pair.c seahorse-key-pair.h \
	seahorse-context.c seahorse-context.h \
	seahorse-widget.c seahorse-widget.h \
	seahorse-op.c seahorse-op.h \
	seahorse-key-store.c seahorse-key-store.h \
	seahorse-recipients-store.c seahorse-recipients-store.h \
	seahorse-util.c seahorse-util.h \
	seahorse-validity.c seahorse-validity.h \
	seahorse-check-button-control.c seahorse-check-button-control.h \
	seahorse-default-key-control.c seahorse-default-key-control.h \
	seahorse-passphrase.c \
	seahorse-recipients.c \
	seahorse-signatures.c \
	seahorse-libdialogs.h \
	$(BUILT_SOURCES)

seahorse-marshal.h: seahorse-marshal.list $(GLIB_GENMARSHAL)
	$(GLIB_GENMARSHAL) $< --header --prefix=seahorse_marshal > $@

seahorse-marshal.c: seahorse-marshal.list $(GLIB_GENMARSHAL)
	echo "#include \"seahorse-marshal.h\"" > $@ && \
	$(GLIB_GENMARSHAL) $< --body --prefix=seahorse_marshal >> $@

glade_DATA = \
	seahorse-passphrase.glade \
	seahorse-recipients.glade \
	seahorse-signatures.glade

EXTRA_DIST = $(glade_DATA) \
	seahorse-marshal.list
