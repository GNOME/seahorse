gladedir = $(datadir)/seahorse/glade/

INCLUDES = -I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libseahorse \
	$(SEAHORSE_CFLAGS) \
	-DDATA_DIR=\""$(datadir)"\" \
	-DSEAHORSE_GLADEDIR=\""$(gladedir)"\"

bin_PROGRAMS = seahorse-pgp-preferences

seahorse_pgp_preferences_SOURCES = seahorse-pgp-preferences.c
seahorse_pgp_preferences_LDADD = \
	$(top_builddir)/libseahorse/libseahorse.a \
	$(SEAHORSE_LIBS)

mimedir = $(datadir)/mime-info
mime_DATA = seahorse.mime seahorse.keys

glade_DATA = seahorse-pgp-preferences.glade

EXTRA_DIST = \
	$(mime_DATA) \
	$(glade_DATA)
