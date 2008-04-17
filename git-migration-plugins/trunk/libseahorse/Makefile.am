
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

noinst_LIBRARIES = libseahorse.a

BUILT_SOURCES = \
	seahorse-marshal.c seahorse-marshal.h

if WITH_AGENT
AGENT_SRCS = seahorse-prefs-cache.c
else
AGENT_SRCS = 
endif

libseahorse_a_SOURCES = \
	seahorse-pgp-operation.c seahorse-pgp-operation.h \
	seahorse-widget.c seahorse-widget.h \
	seahorse-util.c seahorse-util.h \
	seahorse-check-button-control.c seahorse-check-button-control.h \
	seahorse-gpg-options.c seahorse-gpg-options.h \
	seahorse-passphrase.c seahorse-passphrase.h \
	seahorse-libdialogs.h \
        seahorse-notification.c \
	seahorse-vfs-data.c seahorse-vfs-data.h \
	seahorse-gpgmex.h seahorse-gpgmex-op.c seahorse-gpgmex-util.c \
	seahorse-prefs.c seahorse-prefs.h \
	seahorse-operation.c seahorse-operation.h \
	seahorse-progress.c seahorse-progress.h \
	seahorse-gconf.c seahorse-gconf.h \
	seahorse-gtkstock.c seahorse-gtkstock.h \
	seahorse-secure-memory.c seahorse-secure-memory.h \
	seahorse-secure-entry.c seahorse-secure-entry.h \
	seahorse-algo.c seahorse-algo.h \
	seahorse-unix-signal.c seahorse-unix-signal.h \
	$(BUILT_SOURCES) \
	$(AGENT_SRCS)

seahorse-marshal.h: seahorse-marshal.list $(GLIB_GENMARSHAL)
	$(GLIB_GENMARSHAL) $< --header --prefix=seahorse_marshal > $@

seahorse-marshal.c: seahorse-marshal.list $(GLIB_GENMARSHAL)
	echo "#include \"seahorse-marshal.h\"" > $@ && \
	$(GLIB_GENMARSHAL) $< --body --prefix=seahorse_marshal >> $@

glade_DATA = \
	seahorse-notify.glade \
	seahorse-prefs.glade \
	seahorse-progress.glade 

EXTRA_DIST = $(glade_DATA) \
	seahorse-marshal.list
