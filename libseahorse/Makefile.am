noinst_LIBRARIES = libseahorse.a

gladedir = $(datadir)/seahorse/glade/
localedir = $(datadir)/locale
pixmapsdir = $(datadir)/pixmaps/

BUILT_SOURCES = \
	seahorse-marshal.c seahorse-marshal.h

libseahorse_a_SOURCES = \
	seahorse-key.c seahorse-key.h \
	seahorse-key-pair.c seahorse-key-pair.h \
	seahorse-context.c seahorse-context.h \
	seahorse-widget.c seahorse-widget.h \
	seahorse-op.c seahorse-op.h \
	seahorse-key-store.c seahorse-key-store.h \
	seahorse-signer-menu-item.c seahorse-signer-menu-item.h \
	seahorse-signer-menu.c seahorse-signer-menu.h \
	seahorse-util.c seahorse-util.h \
	seahorse-validity.c seahorse-validity.h \
	seahorse-passphrase.c \
	seahorse-libdialogs.h \
	$(BUILT_SOURCES)

INCLUDES = -I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/libseahorse \
	$(SEAHORSE_CFLAGS) \
	-DDATA_DIR=\""$(datadir)"\" \
	-DSEAHORSE_GLADEDIR=\""$(gladedir)"\" \
	-DLOCALEDIR=\"$(localedir)\" \
	-DPIXMAPSDIR=\""$(pixmapsdir)"\"

seahorse-marshal.h: seahorse-marshal.list $(GLIB_GENMARSHAL)
	$(GLIB_GENMARSHAL) $< --header --prefix=seahorse_marshal > $@

seahorse-marshal.c: seahorse-marshal.list $(GLIB_GENMARSHAL)
	echo "#include \"seahorse-marshal.h\"" > $@ && \
	$(GLIB_GENMARSHAL) $< --body --prefix=seahorse_marshal >> $@

glade_DATA = \
	seahorse-passphrase.glade

EXTRA_DIST = $(glade_DATA) \
	seahorse-marshal.list
