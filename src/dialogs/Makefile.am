gladedir = $(datadir)/seahorse/glade/

INCLUDES = -I$(top_srcdir) \
	-I$(top_srcdir)/src \
	$(SEAHORSE_CFLAGS) \
	-DDATA_DIR=\""$(datadir)"\" \
	-DSEAHORSE_GLADEDIR=\""$(gladedir)"\"

noinst_LIBRARIES = libdialogs.a

libdialogs_a_SOURCES = \
	seahorse-generate.c seahorse-generate.h \
	seahorse-export.c seahorse-export.h \
	seahorse-delete.c seahorse-delete.h \
	seahorse-preferences.c seahorse-preferences.h \
	seahorse-import.c seahorse-import.h \
	seahorse-passphrase.c seahorse-passphrase.h \
	seahorse-signatures.c seahorse-signatures.h \
	seahorse-recipients.c seahorse-recipients.h \
	seahorse-add-uid.c seahorse-add-uid.h \
	seahorse-add-subkey.c seahorse-add-subkey.h \
	seahorse-revoke.c seahorse-revoke.h

glade_DATA = \
	seahorse-generate.glade2 \
	seahorse-export.glade2 \
	seahorse-preferences.glade2 \
	seahorse-import.glade2 \
	seahorse-signatures.glade2 \
	seahorse-recipients.glade2 \
	seahorse-add-uid.glade2 \
	seahorse-add-subkey.glade2 \
	seahorse-revoke.glade2

EXTRA_DIST = $(glade_DATA)
