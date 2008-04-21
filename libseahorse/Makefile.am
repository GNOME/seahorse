
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

noinst_LTLIBRARIES = libseahorse.la

BUILT_SOURCES = \
	seahorse-marshal.c seahorse-marshal.h

if WITH_KEYSERVER
KEYSERVER_SRCS = seahorse-keyserver-control.c seahorse-keyserver-control.h
else
KEYSERVER_SRCS = 
endif

libseahorse_la_SOURCES = \
	seahorse-key.c seahorse-key.h \
	seahorse-key-source.c seahorse-key-source.h \
    seahorse-unknown-key.c seahorse-unknown-key.h \
    seahorse-unknown-source.c seahorse-unknown-source.h \
	seahorse-context.c seahorse-context.h \
	seahorse-widget.c seahorse-widget.h \
	seahorse-keyset.c seahorse-keyset.h \
	seahorse-key-model.c seahorse-key-model.h \
	seahorse-util.c seahorse-util.h \
	seahorse-validity.c seahorse-validity.h \
	seahorse-check-button-control.c seahorse-check-button-control.h \
	seahorse-combo-keys.c seahorse-combo-keys.h \
	seahorse-passphrase.c seahorse-passphrase.h \
	seahorse-libdialogs.h \
	seahorse-vfs-data.c seahorse-vfs-data.h \
	seahorse-prefs.c seahorse-prefs.h \
	seahorse-prefs-keyrings.c \
	seahorse-operation.c seahorse-operation.h \
    seahorse-transfer-operation.c seahorse-transfer-operation.h \
	seahorse-progress.c seahorse-progress.h \
	seahorse-gconf.c seahorse-gconf.h \
	seahorse-dns-sd.c seahorse-dns-sd.h \
	seahorse-gtkstock.c seahorse-gtkstock.h \
	seahorse-notification.c \
	seahorse-secure-memory.c seahorse-secure-memory.h \
	seahorse-secure-entry.c seahorse-secure-entry.h \
	seahorse-unix-signal.c seahorse-unix-signal.h \
	$(BUILT_SOURCES) \
	$(KEYSERVER_SRCS)
	
libseahorse_la_LIBADD = \
	$(top_builddir)/common/libsea-common.la \
	$(top_builddir)/libcryptui/libcryptui.la

seahorselibexecbin_PROGRAMS = xloadimage

xloadimage_SOURCES = ../src/seahorse-xloadimage.c

seahorse-marshal.h: seahorse-marshal.list $(GLIB_GENMARSHAL)
	$(GLIB_GENMARSHAL) $< --header --prefix=seahorse_marshal > $@

seahorse-marshal.c: seahorse-marshal.list $(GLIB_GENMARSHAL)
	echo "#include \"seahorse-marshal.h\"" > $@ && \
	$(GLIB_GENMARSHAL) $< --body --prefix=seahorse_marshal >> $@

glade_DATA = \
	seahorse-prefs.glade \
	seahorse-add-keyring.glade \
	seahorse-add-keyserver.glade \
	seahorse-progress.glade \
	seahorse-notify.glade

EXTRA_DIST = $(glade_DATA) \
	seahorse-marshal.list
