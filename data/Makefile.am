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

EXTRA_DIST = \
	$(convert_DATA) \
	$(gsettings_SCHEMAS)