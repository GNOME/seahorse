
@INTLTOOL_SCHEMAS_RULE@

# PLUGIN ----------------------------------------------------------------

plugindir = $(libdir)/gedit-2/plugins

INCLUDES = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libseahorse \
	$(SEAHORSE_CFLAGS) \
	$(GEDIT_CFLAGS) 								\
	-DGNOME_ICONDIR=\""$(datadir)/pixmaps"\" 		\
	-DGEDIT_GLADEDIR=\""$(datadir)/gedit-2/glade/"\" 

plugin_LTLIBRARIES = libseahorse-pgp.la

libseahorse_pgp_la_SOURCES = seahorse-gedit.c
libseahorse_pgp_la_LDFLAGS = $(PLUGIN_LIBTOOL_FLAGS)
libseahorse_pgp_la_LIBADD = \
	$(top_builddir)/libseahorse/libseahorse-internal.la \
	$(SEAHORSE_LIBS) \
	$(GEDIT_LIBS)

plugin_in_files = seahorse-pgp.gedit-plugin.desktop.in

%.gedit-plugin: %.gedit-plugin.desktop.in  $(INTLTOOL_MERGE) $(wildcard $(top_srcdir)/po/*po) ; $(INTLTOOL_MERGE) $(top_srcdir)/po $< $@ -d -u -c $(top_builddir)/po/.intltool-merge-cache

plugin_DATA = $(plugin_in_files:.gedit-plugin.desktop.in=.gedit-plugin)

# SCHEMA -----------------------------------------------------------------

schemasdir = $(GCONF_SCHEMA_FILE_DIR)
schemas_in_files = seahorse-gedit.schemas.in
schemas_DATA = $(schemas_in_files:.schemas.in=.schemas)

if GCONF_SCHEMAS_INSTALL
install-data-local:
	@GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) gconftool-2 \
		--makefile-install-rule $(schemas_DATA) || \
		(echo ;\
		echo "*****************************************************"; \
		echo "Installation of schemas failed, install them manually"; \
		echo "*****************************************************";)
	@true
else
install-data-local:
endif

EXTRA_DIST = $(plugin_in_files) $(schemas_in_files)
CLEANFILES = $(schemas_DATA) $(plugin_DATA)
	
