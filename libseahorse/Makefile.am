
noinst_LIBRARIES = libseahorse.a

libexec_PROGRAMS = seahorse-pgp-controls

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
	seahorse-recipients-store.c seahorse-recipients-store.h \
	seahorse-util.c seahorse-util.h \
	seahorse-validity.c seahorse-validity.h \
	seahorse-passphrase.c \
	seahorse-recipients.c \
	seahorse-signatures.c \
	seahorse-libdialogs.h \
	$(BUILT_SOURCES)

seahorse_pgp_controls_SOURCES = seahorse-pgp-controls.c
seahorse_pgp_controls_LDADD = \
	libseahorse.a \
	$(SEAHORSE_LIBS)

server_in_files = Seahorse_PGP_Controls.server.in.in

serverdir = $(libdir)/bonobo/servers
server_DATA = $(server_in_files:.server.in.in=.server)
$(server_in_files:.server.in.in=.server.in): $(server_in_files)
	sed -e "s|\@LIBEXECDIR\@|$(libexecdir)|" $< > $@

@INTLTOOL_SERVER_RULE@

INCLUDES = -I$(top_builddir) \
	-I$(top_srcdir) \
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
	seahorse-passphrase.glade \
	seahorse-recipients.glade \
	seahorse-signatures.glade

EXTRA_DIST = $(glade_DATA) \
	$(server_in_files) \
	seahorse-marshal.list

CLEANFILES = \
	$(server_DATA) \
	$(server_DATA).in
