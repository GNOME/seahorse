NULL =
pkgdatadir = $(datadir)/seahorse

public_icons = \
	hicolor_apps_16x16_seahorse.png \
	hicolor_apps_16x16_seahorse-preferences.png \
	hicolor_apps_22x22_seahorse.png \
	hicolor_apps_22x22_seahorse-preferences.png \
	hicolor_apps_24x24_seahorse.png \
	hicolor_apps_24x24_seahorse-preferences.png \
	hicolor_apps_32x32_seahorse.png \
	hicolor_apps_32x32_seahorse-preferences.png \
	hicolor_apps_48x48_seahorse.png \
	hicolor_apps_48x48_seahorse-preferences.png \
	hicolor_apps_256x256_seahorse.png \
	$(NULL)

private_icons = \
	hicolor_apps_22x22_seahorse-key.png \
	hicolor_apps_22x22_seahorse-key-personal.png \
	hicolor_apps_22x22_seahorse-key-ssh.png \
	hicolor_apps_22x22_seahorse-person.png \
	hicolor_apps_48x48_seahorse-key.png \
	hicolor_apps_48x48_seahorse-key-personal.png \
	hicolor_apps_48x48_seahorse-key-ssh.png \
	hicolor_apps_48x48_seahorse-person.png \
	hicolor_status_22x22_seahorse-sign.png \
	hicolor_status_22x22_seahorse-sign-bad.png \
	hicolor_status_22x22_seahorse-sign-ok.png \
	hicolor_status_48x48_seahorse-sign.png \
	hicolor_status_48x48_seahorse-sign-bad.png \
	hicolor_status_48x48_seahorse-sign-ok.png \
	hicolor_status_48x48_seahorse-sign-unknown.png \
	$(NULL)

noinst_DATA = \
 	$(NULL)

EXTRA_DIST = \
	$(public_icons)		\
	$(private_icons)	\
	$(noinst_DATA)		\
	$(NULL)

###############################################################################

gtk_update_icon_cache = gtk-update-icon-cache -f -t

update-icon-cache:
	@-if test -z "$(DESTDIR)"; then \
		echo "Updating Gtk icon cache."; \
		$(gtk_update_icon_cache) $(datadir)/icons/hicolor; \
	else \
		echo "*** Icon cache not updated.  After (un)install, run this:"; \
		echo "***   $(gtk_update_icon_cache) $(datadir)/icons/hicolor"; \
	fi

install-icons:
	for icon in $(public_icons); do \
		THEME=`echo $$icon | cut -d_ -f1`; \
		CONTEXT=`echo $$icon | cut -d_ -f2`; \
		SIZE=`echo $$icon | cut -d_ -f3`; \
		ICONFILE=`echo $$icon | cut -d_ -f4`; \
		mkdir -p $(DESTDIR)$(datadir)/icons/$$THEME/$$SIZE/$$CONTEXT; \
		$(INSTALL_DATA) $(srcdir)/$$icon $(DESTDIR)$(datadir)/icons/$$THEME/$$SIZE/$$CONTEXT/$$ICONFILE; \
	done; \
	for icon in $(private_icons); do \
		THEME=`echo $$icon | cut -d_ -f1`; \
		CONTEXT=`echo $$icon | cut -d_ -f2`; \
		SIZE=`echo $$icon | cut -d_ -f3`; \
		ICONFILE=`echo $$icon | cut -d_ -f4`; \
		mkdir -p $(DESTDIR)$(pkgdatadir)/$(BASE_VERSION)/icons/$$THEME/$$SIZE/$$CONTEXT; \
		$(INSTALL_DATA) $(srcdir)/$$icon $(DESTDIR)$(pkgdatadir)/icons/$$THEME/$$SIZE/$$CONTEXT/$$ICONFILE; \
	done

uninstall-icons:
	-for icon in $(public_icons); do \
		THEME=`echo $$icon | cut -d_ -f1`; \
		CONTEXT=`echo $$icon | cut -d_ -f2`; \
		SIZE=`echo $$icon | cut -d_ -f3`; \
		ICONFILE=`echo $$icon | cut -d_ -f4`; \
		rm -f $(DESTDIR)$(datadir)/icons/$$THEME/$$SIZE/$$CONTEXT/$$ICONFILE; \
	done; \
	for icon in $(private_icons); do \
		THEME=`echo $$icon | cut -d_ -f1`; \
		CONTEXT=`echo $$icon | cut -d_ -f2`; \
		SIZE=`echo $$icon | cut -d_ -f3`; \
		ICONFILE=`echo $$icon | cut -d_ -f4`; \
		rm -f $(DESTDIR)$(pkgdatadir)/icons/$$THEME/$$SIZE/$$CONTEXT/$$ICONFILE; \
	done

install-data-local: install-icons update-icon-cache

uninstall-local: uninstall-icons update-icon-cache
