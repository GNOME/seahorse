
gladedir = $(datadir)/seahorse/glade/
localedir = $(datadir)/locale
pixmapsdir = $(datadir)/pixmaps/seahorse/
seahorselibexecbindir = $(libdir)/seahorse/

INCLUDES = -I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libcryptui \
	$(SEAHORSE_CFLAGS) \
	-DDATA_DIR=\""$(datadir)"\" \
	-DSEAHORSE_GLADEDIR=\""$(gladedir)"\" \
	-DLOCALEDIR=\"$(localedir)\" \
	-DPIXMAPSDIR=\""$(pixmapsdir)"\" \
	-DEXECDIR=\""$(seahorselibexecbindir)"\" \
	-DLIBCRYPTUI_API_SUBJECT_TO_CHANGE

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
	seahorse-check-button-control.c seahorse-check-button-control.h \
	seahorse-combo-keys.c seahorse-combo-keys.h \
	seahorse-commands.c seahorse-commands.h \
	seahorse-context.c seahorse-context.h \
	seahorse-dns-sd.c seahorse-dns-sd.h \
	seahorse-gconf.c seahorse-gconf.h \
	seahorse-gtkstock.c seahorse-gtkstock.h \
	seahorse-libdialogs.h \
	seahorse-notification.c \
	seahorse-object.c seahorse-object.h \
	seahorse-object-model.c seahorse-object-model.h \
	seahorse-object-widget.c seahorse-object-widget.h \
	seahorse-operation.c seahorse-operation.h \
	seahorse-passphrase.c seahorse-passphrase.h \
	seahorse-prefs.c seahorse-prefs.h \
	seahorse-progress.c seahorse-progress.h \
	seahorse-secure-memory.c seahorse-secure-memory.h \
	seahorse-secure-entry.c seahorse-secure-entry.h \
	seahorse-servers.c seahorse-servers.h \
	seahorse-set.c seahorse-set.h \
	seahorse-set-model.c seahorse-set-model.h \
	seahorse-source.c seahorse-source.h \
	seahorse-transfer-operation.c seahorse-transfer-operation.h \
	seahorse-types.c seahorse-types.h \
	seahorse-unix-signal.c seahorse-unix-signal.h \
	seahorse-unknown.c seahorse-unknown.h \
	seahorse-unknown-source.c seahorse-unknown-source.h \
	seahorse-util.c seahorse-util.h \
	seahorse-validity.c seahorse-validity.h \
	seahorse-view.c seahorse-view.h \
	seahorse-widget.c seahorse-widget.h \
	$(MARSHAL_SRCS) \
	$(KEYSERVER_SRCS)
	
libseahorse_la_LIBADD = \
	$(top_builddir)/common/libseahorse-common.la \
	$(top_builddir)/libcryptui/libcryptui.la

seahorse-marshal.h: seahorse-marshal.list $(GLIB_GENMARSHAL)
	$(GLIB_GENMARSHAL) $< --header --prefix=seahorse_marshal > $@

seahorse-marshal.c: seahorse-marshal.list $(GLIB_GENMARSHAL)
	echo "#include \"seahorse-marshal.h\"" > $@ && \
	$(GLIB_GENMARSHAL) $< --body --prefix=seahorse_marshal >> $@

glade_DATA = \
	seahorse-prefs.glade \
	seahorse-add-keyserver.glade \
	seahorse-progress.glade \
	seahorse-notify.glade

EXTRA_DIST = $(glade_DATA) \
	seahorse-marshal.list
