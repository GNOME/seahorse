
INCLUDES = -I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libseahorse \
	$(SEAHORSE_CFLAGS) \
	-DDATA_DIR=\""$(datadir)"\"

bin_PROGRAMS = seahorse-pgp-preferences

seahorse_pgp_preferences_SOURCES = seahorse-pgp-preferences.c
seahorse_pgp_preferences_LDADD = $(SEAHORSE_LIBS)

mimedir = $(datadir)/mime-info
mime_DATA = seahorse.mime seahorse.keys

EXTRA_DIST = $(mime_DATA)
