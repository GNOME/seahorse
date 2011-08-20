
uidir = $(datadir)/seahorse/ui/
localedir = $(datadir)/locale
pixmapsdir = $(datadir)/pixmaps/

INCLUDES = -I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libegg \
	-I$(top_srcdir)/libseahorse \
	$(SEAHORSE_CFLAGS) \
	-DDATA_DIR=\""$(datadir)"\" \
	-DSEAHORSE_UIDIR=\""$(uidir)"\" \
	-DLOCALEDIR=\"$(localedir)\" \
	-DPIXMAPSDIR=\""$(pixmapsdir)"\" \
	-DGETTEXT_PACKAGE=\""seahorse\""

if WITH_KEYSERVER
KEYSERVER_SRCS = seahorse-keyserver-search.c seahorse-keyserver-sync.c seahorse-keyserver-sync.h
KEYSERVER_SRCS += seahorse-keyserver-results.c seahorse-keyserver-results.h
else
KEYSERVER_SRCS = 
endif

if WITH_PGP
PGP_LDADD = $(top_builddir)/pgp/libseahorse-pgp.la
else
PGP_LDADD = 
endif

if WITH_SSH
SSH_LDADD = $(top_builddir)/ssh/libseahorse-ssh.la
else
SSH_LDADD = 
endif

if WITH_PKCS11
PKCS11_LDADD = $(top_builddir)/pkcs11/libseahorse-pkcs11.la
else
PKCS11_LDADD = 
endif

bin_PROGRAMS = seahorse

seahorse_SOURCES = \
	seahorse-preferences.c seahorse-preferences.h \
	eggtreemultidnd.c eggtreemultidnd.h \
	seahorse-generate-select.c seahorse-generate-select.h \
	seahorse-key-manager.c seahorse-key-manager.h \
	seahorse-key-manager-store.c seahorse-key-manager-store.h \
	seahorse-main.c \
	seahorse-viewer.c seahorse-viewer.h \
	seahorse-windows.h \
	$(KEYSERVER_SRCS)

seahorse_LDADD = \
	$(top_builddir)/libseahorse/libseahorse.la \
	$(PGP_LDADD) \
	$(SSH_LDADD) \
	$(PKCS11_LDADD) \
	$(top_builddir)/gkr/libseahorse-gkr.la \
	$(top_builddir)/libegg/libeggdatetime.la \
	$(SEAHORSE_LIBS)

ui_DATA = \
	seahorse-key-manager.ui \
	seahorse-key-manager.xml \
	seahorse-change-passphrase.xml \
	seahorse-generate-select.xml \
	seahorse-keyserver-search.xml \
	seahorse-keyserver-results.ui \
	seahorse-keyserver-results.xml \
	seahorse-keyserver-sync.xml

desktopdir = $(datadir)/applications
desktop_in_files = seahorse.desktop.in 
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

man_MANS = seahorse.1

EXTRA_DIST = \
	$(man_MANS) \
	$(ui_DATA) \
	seahorse.desktop.in.in

CLEANFILES = *.xmlp* *.bak \
    $(desktop_DATA) \
    seahorse.desktop.in 
