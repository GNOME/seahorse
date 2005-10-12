
gladedir = $(datadir)/seahorse/glade/
localedir = $(datadir)/locale
pixmapsdir = $(datadir)/pixmaps/

INCLUDES = -I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libseahorse \
	$(SEAHORSE_CFLAGS) \
	-DDATA_DIR=\""$(datadir)"\" \
	-DSEAHORSE_GLADEDIR=\""$(gladedir)"\" \
	-DLOCALEDIR=\""$(localedir)"\" \
	-DPIXMAPSDIR=\""$(pixmapsdir)"\"

bin_PROGRAMS = seahorse-agent

seahorse_agent_SOURCES = seahorse-agent.c seahorse-agent.h seahorse-agent-actions.c \
                         seahorse-agent-cache.c seahorse-agent-prompt.c seahorse-agent-io.c \
                         gtk-secure-entry.c gtk-secure-entry.h seahorse-agent-secmem.h \
                         seahorse-agent-secmem.c seahorse-agent-status.c \
                         eggtrayicon.h eggtrayicon.c

seahorse_agent_LDADD = \
	$(top_builddir)/libseahorse/libseahorse-internal.la \
	$(SEAHORSE_LIBS)
                         
glade_DATA = \
	seahorse-agent-cache.glade 

EXTRA_DIST = \
	$(glade_DATA) 

CLEANFILES = *.gladep* *.bak \
	$(desktop_DATA)
	
install-exec-hook:
	chmod u+s $(DESTDIR)/$(bindir)/seahorse-agent
