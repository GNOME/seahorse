@INTLTOOL_SCHEMAS_RULE@

# PLUGIN ----------------------------------------------------------------

plugindir = $(libdir)/gedit-2/plugins

localedir = $(datadir)/locale

INCLUDES = \
	-I$(top_srcdir) \
	-DGEDIT_LOCALEDIR=\"$(localedir)\" \
	-DLIBCRYPTUI_API_SUBJECT_TO_CHANGE \
	$(GEDIT_CFLAGS) \
	$(DBUS_CFLAGS) \
	$(LIBCRYPTUI_CFLAGS)

plugin_LTLIBRARIES = libseahorse-pgp.la

if WITH_GEDIT_BONOBO
PLUGIN_SRCS = seahorse-gedit-bonobo.c
else
PLUGIN_SRCS = seahorse-gedit-plugin.c
endif

libseahorse_pgp_la_SOURCES = seahorse-gedit.c seahorse-gedit.h $(PLUGIN_SRCS)
libseahorse_pgp_la_LDFLAGS = $(PLUGIN_LIBTOOL_FLAGS)
libseahorse_pgp_la_LIBADD = $(GEDIT_LIBS) $(DBUS_LIBS) $(LIBCRYPTUI_LIBS) 

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
