
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

RESOURCES_SRCS = \
	seahorse-resources.c seahorse-resources.h

BUILT_SOURCES += \
	$(dbus_shell_search_provider_built_sources) \
	$(MARSHAL_SRCS) \
	$(RESOURCES_SRCS)

noinst_LIBRARIES += libseahorse.a

if WITH_KEYSERVER
KEYSERVER_SRCS = libseahorse/seahorse-keyserver-control.c libseahorse/seahorse-keyserver-control.h
else
KEYSERVER_SRCS = 
endif

libseahorse_a_SOURCES = \
	libseahorse/seahorse-application.c libseahorse/seahorse-application.h \
	libseahorse/seahorse-bind.c libseahorse/seahorse-bind.h \
	libseahorse/seahorse-collection.c libseahorse/seahorse-collection.h \
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
	$(RESOURCES_SRCS) \
	$(KEYSERVER_SRCS)

seahorse-marshal.h: libseahorse/seahorse-marshal.list $(GLIB_GENMARSHAL)
	$(AM_V_GEN) $(GLIB_GENMARSHAL) $< --header --prefix=seahorse_marshal > $@

seahorse-marshal.c: libseahorse/seahorse-marshal.list $(GLIB_GENMARSHAL)
	$(AM_V_GEN) echo "#include \"seahorse-marshal.h\"" > $@ && \
	$(GLIB_GENMARSHAL) $< --body --prefix=seahorse_marshal >> $@

seahorse-resources.h: libseahorse/seahorse.gresource.xml $(ui_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ \
		--sourcedir=$(srcdir) --generate-header --manual-register $<

seahorse-resources.c: libseahorse/seahorse.gresource.xml $(ui_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ \
		--sourcedir=$(srcdir) --generate-source --manual-register $<

libseahorse/seahorse.gresource.xml: Makefile.am
	$(AM_V_GEN) $(MKDIR_P) libseahorse; \
	echo "<?xml version='1.0' encoding='UTF-8'?>" > $@; \
	echo "<gresources>" >> $@; \
	echo "  <gresource prefix='/org/gnome/Seahorse'>" >> $@; \
	list='$(ui_files)'; \
	for f in $$list; do \
	  n=`basename $$f`; \
	  case "$$n" in \
	  *.xml) \
	    echo "    <file alias='"$$n"' preprocess='xml-stripblanks'>$$f</file>" >> $@ ;; \
	  *) \
	    echo "    <file alias='"$$n"'>$$f</file>" >> $@ ;; \
	  esac; \
	done; \
	echo "  </gresource>" >> $@; \
	echo "</gresources>" >> $@;

ui_files += \
	libseahorse/seahorse-prefs.ui \
	libseahorse/seahorse-add-keyserver.ui \
	libseahorse/seahorse-progress.ui \
	libseahorse/seahorse.css

EXTRA_DIST += \
	libseahorse/seahorse-marshal.list \
	libseahorse/org.gnome.ShellSearchProvider2.xml \
	libseahorse/seahorse.gresource.xml

MAINTAINERCLEANFILES += \
	libseahorse/seahorse.gresource.xml

-include $(top_srcdir)/git.mk
