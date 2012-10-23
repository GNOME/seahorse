
uidir = $(datadir)/seahorse/ui/
localedir = $(datadir)/locale
seahorselibexecbindir = $(libdir)/seahorse/

INCLUDES = -I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/common \
	-I$(top_srcdir)/libegg \
	$(SEAHORSE_CFLAGS) \
	-DPKGDATADIR=\""$(pkgdatadir)/"\" \
	-DDATA_DIR=\""$(datadir)"\" \
	-DSEAHORSE_UIDIR=\""$(uidir)"\" \
	-DLOCALEDIR=\"$(localedir)\" \
	-DEXECDIR=\""$(seahorselibexecbindir)"\"

dbus_shell_search_provider_built_sources =	\
	seahorse-shell-search-provider-generated.c	\
	seahorse-shell-search-provider-generated.h

# The upstream for the DBus interface definition is
# at http://git.gnome.org/browse/gnome-shell/plain/data/org.gnome.ShellSearchProvider2.xml
$(dbus_shell_search_provider_built_sources) : Makefile.am $(srcdir)/org.gnome.ShellSearchProvider2.xml
	gdbus-codegen								\
		--interface-prefix org.gnome.					\
		--c-namespace Seahorse						\
		--generate-c-code seahorse-shell-search-provider-generated	\
		$(srcdir)/org.gnome.ShellSearchProvider2.xml

MARSHAL_SRCS = \
	seahorse-marshal.c seahorse-marshal.h

BUILT_SOURCES = \
	$(dbus_shell_search_provider_built_sources) \
	$(MARSHAL_SRCS)

noinst_LTLIBRARIES = libseahorse.la

if WITH_KEYSERVER
KEYSERVER_SRCS = seahorse-keyserver-control.c seahorse-keyserver-control.h
else
KEYSERVER_SRCS = 
endif

libseahorse_la_SOURCES = \
	seahorse-action.c seahorse-action.h \
	seahorse-actions.c seahorse-actions.h \
	seahorse-application.c seahorse-application.h \
	seahorse-backend.c seahorse-backend.h \
	seahorse-bind.c seahorse-bind.h \
	seahorse-catalog.c seahorse-catalog.h \
	seahorse-cleanup.c seahorse-cleanup.h \
	seahorse-collection.c seahorse-collection.h \
	seahorse-debug.c seahorse-debug.h \
	seahorse-delete-dialog.c seahorse-delete-dialog.h \
	seahorse-icons.c seahorse-icons.h \
	seahorse-interaction.c seahorse-interaction.h \
	seahorse-key-manager-store.c seahorse-key-manager-store.h \
	seahorse-lockable.c seahorse-lockable.h \
	seahorse-object.c seahorse-object.h \
	seahorse-object-list.c seahorse-object-list.h \
	seahorse-object-model.c seahorse-object-model.h \
	seahorse-object-widget.c seahorse-object-widget.h \
	seahorse-passphrase.c seahorse-passphrase.h \
	seahorse-place.c seahorse-place.h \
	seahorse-predicate.c seahorse-predicate.h \
	seahorse-prefs.c seahorse-prefs.h \
	seahorse-progress.c seahorse-progress.h \
	seahorse-registry.c seahorse-registry.h \
	seahorse-search-provider.c seahorse-search-provider.h \
	seahorse-servers.c seahorse-servers.h \
	seahorse-types.c seahorse-types.h \
	seahorse-util.c seahorse-util.h \
	seahorse-validity.c seahorse-validity.h \
	seahorse-viewable.c seahorse-viewable.h \
	seahorse-widget.c seahorse-widget.h \
	$(dbus_shell_search_provider_built_sources) \
	$(MARSHAL_SRCS) \
	$(KEYSERVER_SRCS)

libseahorse_la_LIBADD = \
	$(top_builddir)/common/libcommon.la

seahorse-marshal.h: seahorse-marshal.list $(GLIB_GENMARSHAL)
	$(GLIB_GENMARSHAL) $< --header --prefix=seahorse_marshal > $@

seahorse-marshal.c: seahorse-marshal.list $(GLIB_GENMARSHAL)
	echo "#include \"seahorse-marshal.h\"" > $@ && \
	$(GLIB_GENMARSHAL) $< --body --prefix=seahorse_marshal >> $@

ui_DATA = \
	seahorse-prefs.xml \
	seahorse-add-keyserver.xml \
	seahorse-progress.xml \
	seahorse.css

EXTRA_DIST = $(ui_DATA) \
	seahorse-marshal.list \
	org.gnome.ShellSearchProvider2.xml
