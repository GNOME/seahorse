
uidir = $(datadir)/seahorse/ui/
localedir = $(datadir)/locale
pixmapsdir = $(datadir)/pixmaps/seahorse/
seahorselibexecbindir = $(libdir)/seahorse/

INCLUDES = -I$(top_builddir) \
	-I$(top_srcdir) \
	$(SEAHORSE_CFLAGS) \
	-DDATA_DIR=\""$(datadir)"\" \
	-DSEAHORSE_UIDIR=\""$(uidir)"\" \
	-DLOCALEDIR=\"$(localedir)\" \
	-DPIXMAPSDIR=\""$(pixmapsdir)"\" \
	-DEXECDIR=\""$(seahorselibexecbindir)"\"

MARSHAL_SRCS = \
	seahorse-marshal.c seahorse-marshal.h

BUILT_SOURCES = \
	$(MARSHAL_SRCS)

noinst_LTLIBRARIES = libseahorse.la

if WITH_KEYSERVER
KEYSERVER_SRCS = seahorse-keyserver-control.c seahorse-keyserver-control.h
else
KEYSERVER_SRCS = 
endif

libseahorse_la_SOURCES = \
	seahorse-bind.c seahorse-bind.h \
	seahorse-cleanup.c seahorse-cleanup.h \
	seahorse-combo-keys.c seahorse-combo-keys.h \
	seahorse-commands.c seahorse-commands.h \
	seahorse-context.c seahorse-context.h \
	seahorse-debug.c seahorse-debug.h \
	seahorse-dns-sd.c seahorse-dns-sd.h \
	seahorse-gtkstock.c seahorse-gtkstock.h \
	seahorse-object.c seahorse-object.h \
	seahorse-object-list.c seahorse-object-list.h \
	seahorse-object-model.c seahorse-object-model.h \
	seahorse-object-widget.c seahorse-object-widget.h \
	seahorse-passphrase.c seahorse-passphrase.h \
	seahorse-prefs.c seahorse-prefs.h \
	seahorse-progress.c seahorse-progress.h \
	seahorse-registry.c seahorse-registry.h \
	seahorse-secure-memory.c seahorse-secure-memory.h \
	seahorse-secure-buffer.c seahorse-secure-buffer.h \
	seahorse-servers.c seahorse-servers.h \
	seahorse-set.c seahorse-set.h \
	seahorse-set-model.c seahorse-set-model.h \
	seahorse-source.c seahorse-source.h \
	seahorse-transfer.c seahorse-transfer.h \
	seahorse-types.c seahorse-types.h \
	seahorse-unknown.c seahorse-unknown.h \
	seahorse-unknown-source.c seahorse-unknown-source.h \
	seahorse-util.c seahorse-util.h \
	seahorse-validity.c seahorse-validity.h \
	seahorse-view.c seahorse-view.h \
	seahorse-widget.c seahorse-widget.h \	
	$(MARSHAL_SRCS) \
	$(KEYSERVER_SRCS)

seahorse-marshal.h: seahorse-marshal.list $(GLIB_GENMARSHAL)
	$(GLIB_GENMARSHAL) $< --header --prefix=seahorse_marshal > $@

seahorse-marshal.c: seahorse-marshal.list $(GLIB_GENMARSHAL)
	echo "#include \"seahorse-marshal.h\"" > $@ && \
	$(GLIB_GENMARSHAL) $< --body --prefix=seahorse_marshal >> $@

ui_DATA = \
	seahorse-prefs.xml \
	seahorse-add-keyserver.xml \
	seahorse-progress.xml

EXTRA_DIST = $(ui_DATA) \
	seahorse-marshal.list
