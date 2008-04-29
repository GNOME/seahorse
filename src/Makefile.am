
gladedir = $(datadir)/seahorse/glade/
localedir = $(datadir)/locale
pixmapsdir = $(datadir)/pixmaps/

INCLUDES = -I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libcryptui \
	-I$(top_srcdir)/libseahorse \
	$(SEAHORSE_CFLAGS) \
	-DDATA_DIR=\""$(datadir)"\" \
	-DSEAHORSE_GLADEDIR=\""$(gladedir)"\" \
	-DLOCALEDIR=\"$(localedir)\" \
	-DPIXMAPSDIR=\""$(pixmapsdir)"\" \
	-DLIBCRYPTUI_API_SUBJECT_TO_CHANGE

if WITH_KEYSERVER
KEYSERVER_SRCS = seahorse-keyserver-search.c seahorse-keyserver-results.c seahorse-keyserver-sync.c seahorse-keyserver-sync.h
else
KEYSERVER_SRCS = 
endif

if WITH_SSH
SSH_SRCS = seahorse-ssh-key-properties.c seahorse-ssh-upload.c seahorse-ssh-generate.c
else
SSH_SRCS = 
endif

GNOME_KEYRING_SRCS = \
	seahorse-gkeyring-item-properties.c

bin_PROGRAMS = seahorse

seahorse_SOURCES = main.c \
	seahorse-key-widget.c seahorse-key-widget.h \
	seahorse-key-manager-store.c seahorse-key-manager-store.h \
	seahorse-preferences.c seahorse-preferences.h \
	seahorse-key-manager.c eggtreemultidnd.c eggtreemultidnd.h \
	seahorse-generate-select.c seahorse-pgp-generate.c \
	seahorse-windows.h \
	seahorse-key-properties.c \
	seahorse-photos.c \
	seahorse-revoke.c \
	seahorse-expires.c \
	seahorse-sign.c \
	seahorse-delete.c \
	seahorse-add-uid.c \
	seahorse-add-subkey.c \
	seahorse-key-dialogs.h \
	$(KEYSERVER_SRCS) \
    $(GNOME_KEYRING_SRCS) \
    $(SSH_SRCS)

seahorse_LDADD = \
	$(top_builddir)/libseahorse/libseahorse.la \
	$(top_builddir)/pgp/libsea-pgp.la \
	$(top_builddir)/ssh/libsea-ssh.la \
	$(top_builddir)/gkr/libseahorse-gkr.la \
	$(top_builddir)/common/libsea-common.la \  
	$(top_builddir)/libcryptui/libcryptui.la \
	$(SEAHORSE_LIBS)

glade_DATA = \
	seahorse-key-manager.glade \
	seahorse-key-manager.ui \
	seahorse-pgp-public-key-properties.glade \
	seahorse-pgp-private-key-properties.glade \
	seahorse-pgp-generate.glade \
	seahorse-ssh-key-properties.glade \
	seahorse-ssh-upload.glade \
	seahorse-ssh-generate.glade \
	seahorse-gkeyring-item-properties.glade \
	seahorse-revoke.glade \
	seahorse-expires.glade \
	seahorse-change-passphrase.glade \
	seahorse-sign.glade \
	seahorse-generate-select.glade \
	seahorse-add-uid.glade \
	seahorse-add-subkey.glade \
	seahorse-multi-encrypt.glade \
	seahorse-keyserver-search.glade \
	seahorse-keyserver-results.glade \
	seahorse-keyserver-results.ui \
	seahorse-keyserver-sync.glade 

desktopdir = $(datadir)/applications
desktop_in_files = seahorse.desktop.in 
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST = \
    $(glade_DATA) \
    seahorse.desktop.in.in 

CLEANFILES = *.gladep* *.bak \
    $(desktop_DATA) \
    seahorse.desktop.in 
