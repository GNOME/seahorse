
uidir = $(datadir)/seahorse/ui/
localedir = $(datadir)/locale
seahorselibexecbindir = $(libdir)/seahorse/

INCLUDES = -I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libegg \
	$(SEAHORSE_CFLAGS) \
	-DPKGDATADIR=\""$(pkgdatadir)/"\" \
	-DDATA_DIR=\""$(datadir)"\" \
	-DSEAHORSE_UIDIR=\""$(uidir)"\" \
	-DLOCALEDIR=\"$(localedir)\" \
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
	seahorse-backend.c seahorse-backend.h \
	seahorse-bind.c seahorse-bind.h \
	seahorse-cleanup.c seahorse-cleanup.h \
	seahorse-collection.c seahorse-collection.h \
	seahorse-commands.c seahorse-commands.h \
	seahorse-context.c seahorse-context.h \
	seahorse-debug.c seahorse-debug.h \
	seahorse-icons.c seahorse-icons.h \
	seahorse-key-manager-store.c seahorse-key-manager-store.h \
	seahorse-object.c seahorse-object.h \
	seahorse-object-list.c seahorse-object-list.h \
	seahorse-object-model.c seahorse-object-model.h \
	seahorse-object-widget.c seahorse-object-widget.h \
	seahorse-passphrase.c seahorse-passphrase.h \
	seahorse-predicate.h \
	seahorse-prefs.c seahorse-prefs.h \
	seahorse-preferences.c seahorse-preferences.h \
	seahorse-progress.c seahorse-progress.h \
	seahorse-registry.c seahorse-registry.h \
	seahorse-secure-memory.c seahorse-secure-memory.h \
	seahorse-secure-buffer.c seahorse-secure-buffer.h \
	seahorse-servers.c seahorse-servers.h \
	seahorse-source.c seahorse-source.h \
	seahorse-types.c seahorse-types.h \
	seahorse-util.c seahorse-util.h \
	seahorse-validity.c seahorse-validity.h \
	seahorse-view.c seahorse-view.h \
	seahorse-viewer.c seahorse-viewer.h \
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
