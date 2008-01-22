
autostartdir = $(sysconfdir)/xdg/autostart
gladedir = $(datadir)/seahorse/glade/
localedir = $(datadir)/locale
pixmapsdir = $(datadir)/pixmaps/

autostart_in_files = seahorse-agent.desktop.in
autostart_DATA = $(autostart_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

INCLUDES = -I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libcryptui \
	-I$(top_srcdir)/libseahorse \
	$(SEAHORSE_CFLAGS) \
	$(AGENT_CFLAGS) \
	-DDATA_DIR=\""$(datadir)"\" \
	-DSEAHORSE_GLADEDIR=\""$(gladedir)"\" \
	-DLOCALEDIR=\""$(localedir)"\" \
	-DPIXMAPSDIR=\""$(pixmapsdir)"\" \
	-DLIBCRYPTUI_API_SUBJECT_TO_CHANGE

bin_PROGRAMS = seahorse-agent

seahorse_agent_SOURCES = seahorse-agent.c seahorse-agent.h seahorse-agent-main.c \
						 seahorse-agent-actions.c seahorse-agent-cache.c seahorse-agent-prompt.c \
						 seahorse-agent-io.c seahorse-agent-status.c

seahorse_agent_LDADD = \
    $(top_builddir)/libseahorse/libseahorse.a \
    $(top_builddir)/libcryptui/libcryptui.la \
    $(SEAHORSE_LIBS)

glade_DATA = \
	seahorse-agent-cache.glade 

man_MANS = seahorse-agent.1

EXTRA_DIST = \
	$(man_MANS)			\
	seahorse-agent.desktop.in.in	\
	$(glade_DATA) 

CLEANFILES = \
	*.gladep* *.bak 	\
	$(autostart_DATA)	\
	$(desktop_DATA)

if HAVE_USER_MLOCK
# Don't make the agent setuid
else
# Make the agent setuid
install-exec-hook:
	chmod u+s $(DESTDIR)$(bindir)/seahorse-agent
endif

