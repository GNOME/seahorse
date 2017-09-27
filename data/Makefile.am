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

appdatadir = $(datadir)/appdata
appdata_in_files = seahorse.appdata.xml.in
appdata_DATA = $(appdata_in_files:.xml.in=.xml)
$(appdata_DATA): $(appdata_in_files)
	$(AM_V_GEN) $(MSGFMT) --xml --template $< -d $(top_srcdir)/po -o $@

desktopdir = $(datadir)/applications
desktop_in_files = seahorse.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
$(desktop_DATA): $(desktop_in_files)
	$(AM_V_GEN) $(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@

searchproviderdir = $(datadir)/gnome-shell/search-providers
dist_searchprovider_DATA = seahorse-search-provider.ini

EXTRA_DIST = \
	$(appdata_in_files) \
	$(convert_DATA) \
	$(gsettings_SCHEMAS) \
	$(service_in_files) \
	seahorse.desktop.in.in \
	$(NULL)

CLEANFILES = \
	org.gnome.seahorse.Application.service \
	$(appdata_DATA) \
	$(desktop_DATA) \
	$(desktop_in_files) \
	$(service_in_files) \
	$(NULL)

-include $(top_srcdir)/git.mk
