
INCLUDES = \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libseahorse \
	$(SEAHORSE_CFLAGS) \
	-DDATA_DIR=\""$(datadir)"\"

bonobodir = $(libdir)/bonobo
bonobo_LTLIBRARIES = libseahorse-pgp-file-listener.la

libseahorse_pgp_file_listener_la_SOURCES = seahorse-pgp-file-listener.c

libseahorse_pgp_file_listener_la_LIBADD = $(SEAHORSE_LIBS)
libseahorse_pgp_file_listener_la_LDFLAGS = -module -avoid-version

libexec_PROGRAMS = seahorse-context-factory

seahorse_context_factory_SOURCES = seahorse-context-factory.c
seahorse_context_factory_LDADD = \
	$(top_builddir)/libseahorse/libseahorse.a \
	$(SEAHORSE_LIBS)

@INTLTOOL_SERVER_RULE@

bonoboserver_in_files = Seahorse_PGP_File_Listener.server.in.in
libexecserver_in_files = Seahorse_Context_Factory.server.in.in
server_in_files = \
	$(bonoboserver_in_files) \
	$(libexecserver_in_files)

serverdir = $(libdir)/bonobo/servers
server_DATA = $(server_in_files:.server.in.in=.server)

$(bonoboserver_in_files:.server.in.in=.server.in): $(bonoboserver_in_files)
	sed -e "s|\@BONOBODIR\@|$(bonobodir)|" $< > $@
$(libexecserver_in_files:.server.in.in=.server.in): $(libexecserver_in_files)
	sed -e "s|\@LIBEXECDIR\@|$(libexecdir)|" $< > $@

EXTRA_DIST = $(server_in_files)
	
CLEANFILES = *.server.in \
	$(server_DATA)
