
@INTLTOOL_SCHEMAS_RULE@

schemasdir = $(GCONF_SCHEMA_FILE_DIR)
schemas_in_files = seahorse.schemas.in
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

@INTLTOOL_DESKTOP_RULE@

desktopdir = $(datadir)/applications
desktop_in_files = seahorse.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

EXTRA_DIST = \
	$(schemas_in_files) \
	$(desktop_in_files)

CLEANFILES = \
	$(desktop_DATA) \
	$(schemas_DATA)
