
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
	-DLIBCRYPTUI_API_SUBJECT_TO_CHANGE 

noinst_LTLIBRARIES = libseahorse-ssh.la

libseahorse_ssh_la_SOURCES = \
	seahorse-ssh.c seahorse-ssh.h \
	seahorse-algo.c seahorse-algo.h \
	seahorse-ssh-key-data.c seahorse-ssh-key-data.h \
	seahorse-ssh-key.c seahorse-ssh-key.h  \
	seahorse-ssh-source.c seahorse-ssh-source.h \
	seahorse-ssh-operation.c seahorse-ssh-operation.h
	
libseahorse_ssh_la_LIBADD = \
	$(top_builddir)/libseahorse/libseahorse.la \
	$(top_builddir)/libcryptui/libcryptui.la

seahorselibexecbin_PROGRAMS = seahorse-ssh-askpass

seahorse_ssh_askpass_SOURCES = seahorse-ssh-askpass.c
seahorse_ssh_askpass_LDADD = \
	$(top_builddir)/libseahorse/libseahorse.la \
	$(top_builddir)/libcryptui/libcryptui.la \
	$(SEAHORSE_LIBS)

	