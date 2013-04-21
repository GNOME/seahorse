include $(top_srcdir)/Makefile.decl
SUBDIRS = icons


gsettings_SCHEMAS = \
	org.gnome.seahorse.gschema.xml \
	org.gnome.seahorse.manager.gschema.xml \
	org.gnome.seahorse.window.gschema.xml

@GSETTINGS_RULES@

convert_DATA = \
	org.gnome.seahorse.convert \
	org.gnome.seahorse.manager.convert

convertdir = \
	$(datarootdir)/GConf/gsettings

servicedir = $(datadir)/dbus-1/services
service_DATA = $(service_in_files:.service.in=.service)
service_in_files = 				\
	org.gnome.seahorse.Application.service.in

searchproviderdir = $(datadir)/gnome-shell/search-providers
dist_searchprovider_DATA = seahorse-search-provider.ini

EXTRA_DIST = \
	$(convert_DATA) \
	$(gsettings_SCHEMAS) \
	$(service_in_files)
