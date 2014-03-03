
if WITH_PGP
PGP_LDADD = libseahorse-pgp.a
else
PGP_LDADD = 
endif

if WITH_SSH
SSH_LDADD = libseahorse-ssh.a
else
SSH_LDADD = 
endif

if WITH_PKCS11
PKCS11_LDADD = libseahorse-pkcs11.a
else
PKCS11_LDADD = 
endif

bin_PROGRAMS += seahorse

seahorse_SOURCES = \
	src/seahorse-generate-select.c src/seahorse-generate-select.h \
	src/seahorse-import-dialog.c src/seahorse-import-dialog.h \
	src/seahorse-key-manager.c src/seahorse-key-manager.h \
	src/seahorse-main.c \
	src/seahorse-sidebar.c src/seahorse-sidebar.h

seahorse_LDADD = \
	$(PGP_LDADD) \
	$(SSH_LDADD) \
	$(PKCS11_LDADD) \
	libseahorse.a \
	libseahorse-gkr.a \
	libcommon.a \
	libeggdatetime.a \
	libeggtreemultidnd.a \
	$(SEAHORSE_LIBS)

ui_DATA += \
	src/seahorse-key-manager.ui \
	src/seahorse-key-manager.xml \
	src/seahorse-change-passphrase.xml \
	src/seahorse-generate-select.xml

desktopdir = $(datadir)/applications
desktop_in_files = src/seahorse.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

man_MANS += src/seahorse.1

EXTRA_DIST += \
	$(man_MANS) \
	$(ui_DATA) \
	src/seahorse.desktop.in.in

CLEANFILES += *.xmlp* *.bak \
	$(desktop_DATA) \
	$(desktop_in_files)
