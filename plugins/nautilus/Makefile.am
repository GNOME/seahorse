gladedir = $(datadir)/seahorse/glade/

INCLUDES = -I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libseahorse \
	$(SEAHORSE_CFLAGS) \
	-DDATA_DIR=\""$(datadir)"\" \
	-DSEAHORSE_GLADEDIR=\""$(gladedir)"\"

bin_PROGRAMS = seahorse-pgp-preferences

if WITH_AGENT
AGENT_SRCS = seahorse-pgp-preferences-cache.c
else
AGENT_SRCS = 
endif

seahorse_pgp_preferences_SOURCES = \
	seahorse-pgp-preferences.c \
	$(AGENT_SRCS)
	
seahorse_pgp_preferences_LDADD = \
	$(top_builddir)/libseahorse/libseahorse-internal.la \
	$(SEAHORSE_LIBS)

# The old mime structure 
mimeinfodir = $(datadir)/mime-info
mimeinfo_DATA = seahorse.mime seahorse.keys

# The new mime system
mimedir = $(datadir)/mime/packages
mime_DATA = seahorse.xml

install-data-local:
	update-mime-database $(datadir)/mime/
	
@INTLTOOL_DESKTOP_RULE@
desktopdir = $(datadir)/control-center-2.0/capplets
desktop_in_files = seahorse-pgp-preferences.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

glade_DATA = seahorse-pgp-preferences.glade

EXTRA_DIST = \
	$(mime_DATA) \
	$(mimeinfo_DATA) \
	$(desktop_in_files) \
	$(glade_DATA)

CLEANFILES = *.gladep* *.bak \
	$(desktop_DATA)
