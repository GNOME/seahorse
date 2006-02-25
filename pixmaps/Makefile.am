INCLUDES = -DDATA_DIR=\""$(datadir)"\"

pixmap_DATA = seahorse.png
appicon_DATA = seahorse-secret.png \
               seahorse-key.png \
               seahorse-key-ssh.png \
               seahorse-key-ssh-large.png \
               seahorse-share-keys.png \
               seahorse-person.png \
               seahorse-unknown.png \
               seahorse-good.png \
               seahorse-bad.png \
               seahorse-applet-encrypted.png \
               seahorse-applet-key.png \
               seahorse-applet-signed.png \ 
               seahorse-applet-text.png \ 
               seahorse-applet-unknown.png

pixmapdir = $(datadir)/pixmaps/
appicondir = $(datadir)/pixmaps/seahorse

desktop_iconsdir   = $(datadir)/icons/hicolor/48x48/apps
desktop_icons_DATA = seahorse.png

EXTRA_DIST = $(pixmap_DATA) \
             $(appicon_DATA) \
             seahorse-shared-keys.xcf
