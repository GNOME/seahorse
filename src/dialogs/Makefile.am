gladedir = $(datadir)/seahorse/glade/

INCLUDES = -I$(top_srcdir) \
	-I$(top_srcdir)/src \
	$(SEAHORSE_CFLAGS) \
	-DDATA_DIR=\""$(datadir)"\" \
	-DSEAHORSE_GLADEDIR=\""$(gladedir)"\"

noinst_LIBRARIES = libdialogs.a

libdialogs_a_SOURCES = \
	seahorse-add-subkey.c \
	seahorse-add-uid.c \
	seahorse-delete.c \
	seahorse-export.c \
	seahorse-revoke.c \
	seahorse-sign.c \
	seahorse-key-dialogs.h \
	seahorse-encrypt-file.c \
	seahorse-decrypt-file.c \
	seahorse-sign-file.c \
	seahorse-verify-file.c \
	seahorse-encrypt-sign-file.c \
	seahorse-decrypt-verify-file.c \
	seahorse-file-dialogs.h \
	seahorse-generate.c seahorse-generate.h \
	seahorse-preferences.c seahorse-preferences.h \
	seahorse-import.c seahorse-import.h \
	seahorse-passphrase.c seahorse-passphrase.h \
	seahorse-signatures.c seahorse-signatures.h \
	seahorse-recipients.c seahorse-recipients.h

glade_DATA = \
	seahorse-generate.glade2 \
	seahorse-export.glade2 \
	seahorse-preferences.glade2 \
	seahorse-import.glade2 \
	seahorse-signatures.glade2 \
	seahorse-recipients.glade2 \
	seahorse-add-uid.glade2 \
	seahorse-add-subkey.glade2 \
	seahorse-revoke.glade2 \
	seahorse-sign.glade2 \
	seahorse-encrypt-file.glade2 \
	seahorse-decrypt-file.glade2 \
	seahorse-sign-file.glade2 \
	seahorse-verify-file.glade2 \
	seahorse-encrypt-sign-file.glade2 \
	seahorse-decrypt-verify-file.glade2

EXTRA_DIST = $(glade_DATA)
