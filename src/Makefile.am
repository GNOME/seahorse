
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

# HACK: One day libcommon.a and libseahorse.a will be one and
# the same. Until then specify libseahorse.a twice when linking

seahorse_LDADD = \
	$(PGP_LDADD) \
	$(SSH_LDADD) \
	$(PKCS11_LDADD) \
	libseahorse-gkr.a \
	libseahorse.a \
	libcommon.a \
	libseahorse.a \
	libeggdatetime.a \
	libeggtreemultidnd.a \
	$(SEAHORSE_LIBS)

ui_DATA += \
	src/seahorse-key-manager.ui \
	src/seahorse-key-manager.xml \
	src/seahorse-change-passphrase.xml \
	src/seahorse-generate-select.xml

man_MANS += src/seahorse.1

EXTRA_DIST += \
	$(man_MANS) \
	$(ui_DATA) \
	$(NULL)

CLEANFILES += *.xmlp* *.bak \
	$(NULL)
