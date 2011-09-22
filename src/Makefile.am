
uidir = $(datadir)/seahorse/ui/
localedir = $(datadir)/locale

INCLUDES = -I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libseahorse \
	$(SEAHORSE_CFLAGS) \
	-DSEAHORSE_UIDIR=\""$(uidir)"\" \
	-DLOCALEDIR=\"$(localedir)\" \
	-DGETTEXT_PACKAGE=\""seahorse\""

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
	seahorse-generate-select.c seahorse-generate-select.h \
	seahorse-import-dialog.c seahorse-import-dialog.h \
	seahorse-key-manager.c seahorse-key-manager.h \
	seahorse-main.c \
	seahorse-sidebar.c seahorse-sidebar.h \
	seahorse-windows.h \
	$(KEYSERVER_SRCS)

seahorse_LDADD = \
	$(top_builddir)/libseahorse/libseahorse.la \
	$(PGP_LDADD) \
	$(SSH_LDADD) \
	$(PKCS11_LDADD) \
	$(top_builddir)/gkr/libseahorse-gkr.la \
	$(top_builddir)/libegg/libeggdatetime.la \
	$(top_builddir)/libegg/libeggtreemultidnd.la \
	$(SEAHORSE_LIBS)

ui_DATA = \
	seahorse-key-manager.ui \
	seahorse-key-manager.xml \
	seahorse-change-passphrase.xml \
	seahorse-generate-select.xml

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
