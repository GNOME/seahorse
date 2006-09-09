
uidir = $(datadir)/gnome-2.0/ui
gladedir = $(datadir)/seahorse/glade/
localedir = $(datadir)/locale

SUBDIRS = docs
DIST_SUBDIRS = docs

INCLUDES = \
	-I.	\
	-I$(srcdir) \
	$(GNOME_APPLETS_CFLAGS)	\
	-I$(includedir) \
	-I$(top_srcdir)/libseahorse \
	-I$(top_srcdir)/libcryptui \
	$(SEAHORSE_CFLAGS) \
	$(LIBCRYPTUI_CFLAGS) \
	-DGNOMELOCALEDIR=\"$(localedir)\" \
	-DUIDIR=\"$(uidir)\" \
	-DDATADIR=\""$(datadir)"\" \
	-DLIBDIR=\""$(libdir)"\" \
	-DSYSCONFDIR=\""$(sysconfdir)"\" \
	-DPREFIX=\""$(prefix)"\" \
	-DLIBCRYPTUI_API_SUBJECT_TO_CHANGE

libexec_PROGRAMS = seahorse-applet

seahorse_applet_SOURCES = \
	seahorse-applet.c \
	seahorse-applet.h

seahorse_applet_LDADD = \
	$(GNOME_APPLETS_LIBS) \
	$(top_builddir)/libseahorse/libseahorse.a \
	$(top_builddir)/libcryptui/libcryptui.la \
	$(SEAHORSE_LIBS) \
	$(LIBCRYPTUI_LIBS)

serverdir = $(libdir)/bonobo/servers
server_in_files = GNOME_SeahorseApplet.server.in
server_DATA = $(server_in_files:.server.in=.server)

$(server_in_files): $(server_in_files:.server.in=.server.in.in)
	sed -e "s|\@LIBEXECDIR\@|$(libexecdir)|" $< > $@

CLEANFILES = $(server_in_files) $(server_DATA) $(schemas_DATA) *.gladep* *.bak

ui_DATA = \
	GNOME_SeahorseApplet.xml

@INTLTOOL_SERVER_RULE@

glade_DATA = \
	seahorse-applet-preferences.glade 

EXTRA_DIST = \
	GNOME_SeahorseApplet.server.in.in \
	$(glade_DATA) \
	$(ui_DATA)
