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
	$(top_builddir)/libseahorse/libseahorse.a \
	$(SEAHORSE_LIBS)

plugin_in_files = seahorse-pgp.gedit-plugin.desktop.in

%.gedit-plugin: %.gedit-plugin.desktop.in  $(INTLTOOL_MERGE) $(wildcard $(top_srcdir)/po/*po) ; $(INTLTOOL_MERGE) $(top_srcdir)/po $< $@ -d -u -c $(top_builddir)/po/.intltool-merge-cache

plugin_DATA = $(plugin_in_files:.gedit-plugin.desktop.in=.gedit-plugin)

EXTRA_DIST = $(plugin_DATA) 
