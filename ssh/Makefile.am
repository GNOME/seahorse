
gladedir = $(datadir)/seahorse/glade/
localedir = $(datadir)/locale
seahorselibexecbindir = $(libdir)/seahorse/

INCLUDES = -I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libcryptui \
	-I$(top_srcdir)/libseahorse \
	$(SEAHORSE_CFLAGS) \
	-DDATA_DIR=\""$(datadir)"\" \
	-DSEAHORSE_GLADEDIR=\""$(gladedir)"\" \
	-DLOCALEDIR=\"$(localedir)\" \
	-DEXECDIR=\""$(seahorselibexecbindir)"\" \
	-DLIBCRYPTUI_API_SUBJECT_TO_CHANGE \
	-DGETTEXT_PACKAGE=\""seahorse\""

AM_LDFLAGS = @NETLIBS@

VALA_SRCS = \
	seahorse-ssh.vala \
	seahorse-ssh-commands.vala \
	seahorse-ssh-generator.vala
	
VALA_VAPIS = \
	libseahorse-ssh-c.vapi \
	$(top_srcdir)/config.vapi \
	$(top_srcdir)/libseahorse/libseahorse-c.vapi \
	$(top_srcdir)/libseahorse/libseahorse.vapi

VALA_CFILES = $(VALA_SRCS:.vala=.c)
VALA_HFILES = $(VALA_SRCS:.vala=.h)

SUFFIXES = .vala .c .h

vala-build.stamp: $(VALA_SRCS) $(VALA_VAPIS)
	$(VALAC) -C $(VALA_PKGS) $(VALA_VAPIS) $(VALA_SRCS)
	date +'%s' > $@

BUILT_SOURCES = \
	vala-build.stamp
	
noinst_LTLIBRARIES = libseahorse-ssh.la

libseahorse_ssh_la_SOURCES = \
	seahorse-ssh-module.c seahorse-ssh-module.h \
	seahorse-algo.c seahorse-algo.h \
	seahorse-ssh-dialogs.h \
	seahorse-ssh-generate.c \
	seahorse-ssh-key-data.c seahorse-ssh-key-data.h \
	seahorse-ssh-key.c seahorse-ssh-key.h  \
	seahorse-ssh-key-properties.c \
	seahorse-ssh-source.c seahorse-ssh-source.h \
	seahorse-ssh-operation.c seahorse-ssh-operation.h \
	seahorse-ssh-upload.c \
	$(VALA_CFILES) $(VALA_HFILES)

libseahorse_ssh_la_LIBADD = \
	$(top_builddir)/libseahorse/libseahorse.la \
	$(top_builddir)/libcryptui/libcryptui.la

seahorselibexecbin_PROGRAMS = seahorse-ssh-askpass

seahorse_ssh_askpass_SOURCES = seahorse-ssh-askpass.c
seahorse_ssh_askpass_LDADD = \
	$(top_builddir)/libseahorse/libseahorse.la \
	$(top_builddir)/libcryptui/libcryptui.la \
	$(SEAHORSE_LIBS)

glade_DATA = \
	seahorse-ssh-key-properties.glade \
	seahorse-ssh-generate.glade \
	seahorse-ssh-upload.glade 

EXTRA_DIST = \
	$(glade_DATA) \
	libseahorse-ssh-c.vapi \
	vala-build.stamp \
	$(VALA_CFILES) $(VALA_HFILES) \
	$(VALA_SRCS)

vala-clean:
	rm -f vala-build.stamp $(VALA_CFILES) $(VALA_HFILES)

