
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

VALA_SRCS = \
	seahorse-pkcs11.vala
	
VALA_VAPIS = \
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
	
noinst_LTLIBRARIES = libseahorse-pkcs11.la

libseahorse_pkcs11_la_SOURCES = \
	seahorse-pkcs11-module.c seahorse-pkcs11-module.h \
	$(VALA_CFILES) $(VALA_HFILES)

libseahorse_pkcs11_la_LIBADD = \
	$(top_builddir)/libseahorse/libseahorse.la \
	$(top_builddir)/libcryptui/libcryptui.la

glade_DATA = 

EXTRA_DIST = \
	$(glade_DATA) \
	vala-build.stamp \
	$(VALA_CFILES) $(VALA_HFILES) \
	$(VALA_SRCS)

vala-clean:
	rm -f vala-build.stamp $(VALA_CFILES) $(VALA_HFILES)

