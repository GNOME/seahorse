
INCLUDES = \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libcryptui \
	-I$(top_srcdir)/libseahorse \
	$(BONOBO_CFLAGS) \
	-DDATA_DIR=\""$(datadir)"\"

bonobodir = $(libdir)/bonobo
bonobo_LTLIBRARIES = libseahorse-pgp-file-listener.la

libseahorse_pgp_file_listener_la_SOURCES = seahorse-pgp-file-listener.c

libseahorse_pgp_file_listener_la_LIBADD = $(BONOBO_LIBS)
libseahorse_pgp_file_listener_la_LDFLAGS = -module -avoid-version -no-undefined

@INTLTOOL_SERVER_RULE@

server_in_files = Seahorse_PGP_File_Listener.server.in.in
serverdir = $(libdir)/bonobo/servers
server_DATA = $(server_in_files:.server.in.in=.server)
$(server_in_files:.server.in.in=.server.in): $(server_in_files)
	sed -e "s|\@BONOBODIR\@|$(bonobodir)|" $< > $@

EXTRA_DIST = $(server_in_files)
	
CLEANFILES = *.server.in \
	$(server_DATA)
