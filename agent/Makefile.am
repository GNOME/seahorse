
gladedir = $(datadir)/seahorse/glade/
localedir = $(datadir)/locale
pixmapsdir = $(datadir)/pixmaps/

INCLUDES = -I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libcryptui \
	-I$(top_srcdir)/libseahorse \
	-I$(top_srcdir)/daemon \
	$(SEAHORSE_CFLAGS) \
	$(AGENT_CFLAGS) \
	-DDATA_DIR=\""$(datadir)"\" \
	-DSEAHORSE_GLADEDIR=\""$(gladedir)"\" \
	-DLOCALEDIR=\""$(localedir)"\" \
	-DPIXMAPSDIR=\""$(pixmapsdir)"\"

noinst_LIBRARIES = libseahorseagent.a

libseahorseagent_a_SOURCES = seahorse-agent.c seahorse-agent.h seahorse-agent-actions.c \
                             seahorse-agent-cache.c seahorse-agent-prompt.c seahorse-agent-io.c \
                             seahorse-agent-status.c

glade_DATA = \
	seahorse-agent-cache.glade 

EXTRA_DIST = \
	$(glade_DATA) 

CLEANFILES = *.gladep* *.bak \
	$(desktop_DATA)
