
gladedir = $(datadir)/seahorse/glade/
localedir = $(datadir)/locale
pixmapsdir = $(datadir)/pixmaps/

INCLUDES = -I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libseahorse \
	-I$(top_srcdir)/agent \
	$(SEAHORSE_CFLAGS) \
	-DDATA_DIR=\""$(datadir)"\" \
	-DSEAHORSE_GLADEDIR=\""$(gladedir)"\" \
	-DLOCALEDIR=\""$(localedir)"\" \
	-DPIXMAPSDIR=\""$(pixmapsdir)"\"

bin_PROGRAMS = seahorse-daemon

seahorse_daemon_SOURCES = \
        seahorse-daemon.c \
        seahorse-daemon.h 

seahorse_daemon_LDADD = \
    $(top_builddir)/agent/libseahorseagent.a \
	$(top_builddir)/libseahorse/libseahorse-internal.la \
	$(SEAHORSE_LIBS) 
                         
glade_DATA = 

EXTRA_DIST = \
	$(glade_DATA) 

CLEANFILES = *.gladep* *.bak 

install-exec-hook:
	chmod u+s $(bindir)/seahorse-daemon
