
dbus_shell_search_provider_built_sources =	\
	seahorse-shell-search-provider-generated.c	\
	seahorse-shell-search-provider-generated.h

# The upstream for the DBus interface definition is
# at http://git.gnome.org/browse/gnome-shell/plain/data/org.gnome.ShellSearchProvider2.xml
$(dbus_shell_search_provider_built_sources) : Makefile.am $(srcdir)/libseahorse/org.gnome.ShellSearchProvider2.xml
	$(AM_V_GEN) gdbus-codegen \
		--interface-prefix org.gnome.					\
		--c-namespace Seahorse						\
		--generate-c-code seahorse-shell-search-provider-generated	\
		$(srcdir)/libseahorse/org.gnome.ShellSearchProvider2.xml

MARSHAL_SRCS = \
	seahorse-marshal.c seahorse-marshal.h

BUILT_SOURCES += \
	$(dbus_shell_search_provider_built_sources) \
	$(MARSHAL_SRCS)

noinst_LTLIBRARIES += libseahorse.la

if WITH_KEYSERVER
KEYSERVER_SRCS = libseahorse/seahorse-keyserver-control.c libseahorse/seahorse-keyserver-control.h
else
KEYSERVER_SRCS = 
endif

libseahorse_la_SOURCES = \
	libseahorse/seahorse-application.c libseahorse/seahorse-application.h \
	libseahorse/seahorse-bind.c libseahorse/seahorse-bind.h \
	libseahorse/seahorse-collection.c libseahorse/seahorse-collection.h \
	libseahorse/seahorse-debug.c libseahorse/seahorse-debug.h \
	libseahorse/seahorse-interaction.c libseahorse/seahorse-interaction.h \
	libseahorse/seahorse-key-manager-store.c libseahorse/seahorse-key-manager-store.h \
	libseahorse/seahorse-object.c libseahorse/seahorse-object.h \
	libseahorse/seahorse-object-list.c libseahorse/seahorse-object-list.h \
	libseahorse/seahorse-object-model.c libseahorse/seahorse-object-model.h \
	libseahorse/seahorse-object-widget.c libseahorse/seahorse-object-widget.h \
	libseahorse/seahorse-passphrase.c libseahorse/seahorse-passphrase.h \
	libseahorse/seahorse-predicate.c libseahorse/seahorse-predicate.h \
	libseahorse/seahorse-prefs.c libseahorse/seahorse-prefs.h \
	libseahorse/seahorse-progress.c libseahorse/seahorse-progress.h \
	libseahorse/seahorse-search-provider.c libseahorse/seahorse-search-provider.h \
	libseahorse/seahorse-servers.c libseahorse/seahorse-servers.h \
	libseahorse/seahorse-util.c libseahorse/seahorse-util.h \
	libseahorse/seahorse-validity.c libseahorse/seahorse-validity.h \
	libseahorse/seahorse-widget.c libseahorse/seahorse-widget.h \
	$(dbus_shell_search_provider_built_sources) \
	$(MARSHAL_SRCS) \
	$(KEYSERVER_SRCS)

libseahorse_la_LIBADD = \
	libcommon.la

seahorse-marshal.h: libseahorse/seahorse-marshal.list $(GLIB_GENMARSHAL)
	$(AM_V_GEN) $(GLIB_GENMARSHAL) $< --header --prefix=seahorse_marshal > $@

seahorse-marshal.c: libseahorse/seahorse-marshal.list $(GLIB_GENMARSHAL)
	$(AM_V_GEN) echo "#include \"seahorse-marshal.h\"" > $@ && \
	$(GLIB_GENMARSHAL) $< --body --prefix=seahorse_marshal >> $@

ui_DATA += \
	libseahorse/seahorse-prefs.xml \
	libseahorse/seahorse-add-keyserver.xml \
	libseahorse/seahorse-progress.xml \
	libseahorse/seahorse.css

EXTRA_DIST += \
	libseahorse/seahorse-marshal.list \
	libseahorse/org.gnome.ShellSearchProvider2.xml
