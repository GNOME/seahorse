INCLUDES = -I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libseahorse \
	$(SEAHORSE_CFLAGS) \
	-DDATA_DIR=\""$(datadir)"\"

bonobodir = $(libdir)/bonobo
bonobo_LTLIBRARIES = libseahorse-component.la

libseahorse_component_la_SOURCES = \
	seahorse-component.c seahorse-component.h \
	libmain.c

libseahorse_component_la_LIBADD = $(SEAHORSE_LIBS)
libseahorse_component_la_LDFLAGS = -module -avoid-version

server_in_files = Seahorse_Component.server.in.in

serverdir = $(libdir)/bonobo/servers
server_DATA = $(server_in_files:.server.in.in=.server)
$(server_in_files:.server.in.in=.server.in): $(server_in_files)
	sed -e "s|\@BONOBODIR\@|$(bonobodir)|" $< > $@

@INTLTOOL_SERVER_RULE@

EXTRA_DIST = $(server_in_files)
CLEANFILES = $(server_DATA) $(server_DATA).in
