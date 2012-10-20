
uidir = $(datadir)/seahorse/ui/
localedir = $(datadir)/locale
seahorselibexecbindir = $(libdir)/seahorse/

INCLUDES = -I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/common \
	-I$(top_srcdir)/libseahorse \
	$(SEAHORSE_CFLAGS) \
	-DSEAHORSE_UIDIR=\""$(uidir)"\" \
	-DLOCALEDIR=\"$(localedir)\" \
	-DEXECDIR=\""$(seahorselibexecbindir)"\" \
	-DGETTEXT_PACKAGE=\""seahorse\""

AM_LDFLAGS = @NETLIBS@
	
noinst_LTLIBRARIES = libseahorse-ssh.la

libseahorse_ssh_la_SOURCES = \
	seahorse-ssh.h seahorse-ssh.c \
	seahorse-ssh-actions.c seahorse-ssh-actions.h \
	seahorse-ssh-backend.c seahorse-ssh-backend.h \
	seahorse-ssh-deleter.c seahorse-ssh-deleter.h \
	seahorse-ssh-dialogs.h \
	seahorse-ssh-exporter.c seahorse-ssh-exporter.h \
	seahorse-ssh-generate.c \
	seahorse-ssh-key-data.c seahorse-ssh-key-data.h \
	seahorse-ssh-key.c seahorse-ssh-key.h  \
	seahorse-ssh-key-properties.c \
	seahorse-ssh-source.c seahorse-ssh-source.h \
	seahorse-ssh-operation.c seahorse-ssh-operation.h \
	seahorse-ssh-upload.c

libseahorse_ssh_la_LIBADD = \
	$(top_builddir)/common/libcommon.la \
	$(top_builddir)/libseahorse/libseahorse.la

seahorselibexecbin_PROGRAMS = seahorse-ssh-askpass

seahorse_ssh_askpass_SOURCES = seahorse-ssh-askpass.c
seahorse_ssh_askpass_LDADD = \
	$(top_builddir)/libseahorse/libseahorse.la \
	$(SEAHORSE_LIBS)

ui_DATA = \
	seahorse-ssh-key-properties.xml \
	seahorse-ssh-generate.xml \
	seahorse-ssh-upload.xml

EXTRA_DIST = \
	$(ui_DATA)

