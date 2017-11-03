NULL =
pkgdatadir = $(datadir)/seahorse

# Application icons
icon16dir=$(datadir)/icons/hicolor/16x16/apps
icon22dir=$(datadir)/icons/hicolor/22x22/apps
icon24dir=$(datadir)/icons/hicolor/24x24/apps
icon32dir=$(datadir)/icons/hicolor/32x32/apps
icon48dir=$(datadir)/icons/hicolor/48x48/apps
icon256dir=$(datadir)/icons/hicolor/256x256/apps
icon512dir=$(datadir)/icons/hicolor/512x512/apps
iconsymbolicdir=$(datadir)/icons/hicolor/symbolic/apps

icon16_DATA = hicolor/16x16/apps/seahorse.png hicolor/16x16/apps/seahorse-preferences.png
icon22_DATA = hicolor/22x22/apps/seahorse.png hicolor/22x22/apps/seahorse-preferences.png
icon24_DATA = hicolor/24x24/apps/seahorse.png hicolor/24x24/apps/seahorse-preferences.png
icon32_DATA = hicolor/32x32/apps/seahorse.png hicolor/32x32/apps/seahorse-preferences.png
icon48_DATA = hicolor/48x48/apps/seahorse.png hicolor/48x48/apps/seahorse-preferences.png
icon256_DATA = hicolor/256x256/apps/seahorse.png
iconsymbolic_DATA = hicolor/symbolic/apps/seahorse-symbolic.svg

# Private icons
private_icon22appsdir = $(pkgdatadir)/icons/hicolor/22x22/apps
private_icon22apps_DATA = \
	hicolor/22x22/apps/seahorse-key.png \
	hicolor/22x22/apps/seahorse-key-personal.png \
	hicolor/22x22/apps/seahorse-key-ssh.png \
	hicolor/22x22/apps/seahorse-person.png \
	$(NULL)

private_icon22statusdir = $(pkgdatadir)/icons/hicolor/22x22/status
private_icon22status_DATA = \
	hicolor/22x22/status/seahorse-sign.png \
	hicolor/22x22/status/seahorse-sign-bad.png \
	hicolor/22x22/status/seahorse-sign-ok.png \
	$(NULL)

private_icon48appsdir = $(pkgdatadir)/icons/hicolor/48x48/apps
private_icon48apps_DATA = \
	hicolor/48x48/apps/seahorse-key.png \
	hicolor/48x48/apps/seahorse-key-personal.png \
	hicolor/48x48/apps/seahorse-key-ssh.png \
	hicolor/48x48/apps/seahorse-person.png \
	$(NULL)

private_icon48statusdir = $(pkgdatadir)/icons/hicolor/48x48/status
private_icon48status_DATA = \
	hicolor/48x48/status/seahorse-sign.png \
	hicolor/48x48/status/seahorse-sign-bad.png \
	hicolor/48x48/status/seahorse-sign-ok.png \
	hicolor/48x48/status/seahorse-sign-unknown.png \
	$(NULL)


noinst_DATA = \
	$(NULL)

EXTRA_DIST = \
	$(icon16_DATA)	\
	$(icon22_DATA)	\
	$(icon24_DATA)	\
	$(icon32_DATA)	\
	$(icon48_DATA)	\
	$(icon256_DATA)	\
	$(iconsymbolic_DATA)	\
	$(private_icon22apps_DATA)	\
	$(private_icon22status_DATA)	\
	$(private_icon48apps_DATA)	\
	$(private_icon48status_DATA)	\
	$(noinst_DATA)		\
	$(NULL)

-include $(top_srcdir)/git.mk
