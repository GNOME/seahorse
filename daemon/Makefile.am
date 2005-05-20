
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

if WITH_AGENT
AGENT_LIB = $(top_builddir)/agent/libseahorseagent.a
else
AGENT_LIB = 
endif

seahorse_daemon_SOURCES = \
        seahorse-daemon.c \
        seahorse-daemon.h 

seahorse_daemon_LDADD = \
	$(top_builddir)/libseahorse/libseahorse-internal.la \
    $(AGENT_LIB) \
	$(SEAHORSE_LIBS) 
                         
glade_DATA = 

EXTRA_DIST = \
	$(glade_DATA) 

CLEANFILES = *.gladep* *.bak 

install-exec-hook:
	chmod u+s $(bindir)/seahorse-daemon
