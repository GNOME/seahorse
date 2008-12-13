
gladedir = $(datadir)/seahorse/glade/
localedir = $(datadir)/locale
pixmapsdir = $(datadir)/pixmaps/

INCLUDES = -I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libegg \
	-I$(top_srcdir)/libcryptui \
	-I$(top_srcdir)/libseahorse \
	$(SEAHORSE_CFLAGS) \
	-DDATA_DIR=\""$(datadir)"\" \
	-DSEAHORSE_GLADEDIR=\""$(gladedir)"\" \
	-DLOCALEDIR=\"$(localedir)\" \
	-DPIXMAPSDIR=\""$(pixmapsdir)"\" \
	-DLIBCRYPTUI_API_SUBJECT_TO_CHANGE \
	-DGETTEXT_PACKAGE=\""seahorse\""

VALA_SRCS = \
	seahorse-key-manager.vala \
	seahorse-keyserver-results.vala
	
VALA_VAPIS = \
	$(top_srcdir)/bugs.vapi \
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

if WITH_KEYSERVER
KEYSERVER_SRCS = seahorse-keyserver-search.c seahorse-keyserver-sync.c seahorse-keyserver-sync.h
else
KEYSERVER_SRCS = 
endif

if WITH_PGP
PGP_LDADD = $(top_builddir)/pgp/libseahorse-pgp.la
else
PGP_LDADD = 
endif

if WITH_SSH
SSH_LDADD = $(top_builddir)/ssh/libseahorse-ssh.la
else
SSH_LDADD = 
endif

if WITH_PKCS11
PKCS11_LDADD = $(top_builddir)/pkcs11/libseahorse-pkcs11.la
else
PKCS11_LDADD = 
endif

bin_PROGRAMS = seahorse

seahorse_SOURCES = main.c \
	seahorse-preferences.c seahorse-preferences.h \
	eggtreemultidnd.c eggtreemultidnd.h \
	seahorse-generate-select.c seahorse-generate-select.h \
	seahorse-key-manager-store.c seahorse-key-manager-store.h \
	seahorse-viewer.c seahorse-viewer.h \
	seahorse-windows.h \
	$(KEYSERVER_SRCS) \
	$(VALA_CFILES) $(VALA_HFILES)

seahorse_LDADD = \
	$(top_builddir)/libseahorse/libseahorse.la \
	$(PGP_LDADD) \
	$(SSH_LDADD) \
	$(PKCS11_LDADD) \
	$(top_builddir)/gkr/libseahorse-gkr.la \
	$(top_builddir)/common/libseahorse-common.la \
	$(top_builddir)/libcryptui/libcryptui.la \
	$(top_builddir)/libegg/libeggdatetime.la \
	$(SEAHORSE_LIBS)

glade_DATA = \
	seahorse-key-manager.glade \
	seahorse-key-manager.ui \
	seahorse-change-passphrase.glade \
	seahorse-generate-select.glade \
	seahorse-keyserver-search.glade \
	seahorse-keyserver-results.glade \
	seahorse-keyserver-results.ui \
	seahorse-keyserver-sync.glade 

desktopdir = $(datadir)/applications
desktop_in_files = seahorse.desktop.in 
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST = \
	$(glade_DATA) \
	seahorse.desktop.in.in \
	vala-build.stamp \
	$(VALA_CFILES) $(VALA_HFILES) \
	$(VALA_SRCS)

CLEANFILES = *.gladep* *.bak \
    $(desktop_DATA) \
    seahorse.desktop.in 

vala-clean:
	rm -f vala-build.stamp $(VALA_CFILES) $(VALA_HFILES)

