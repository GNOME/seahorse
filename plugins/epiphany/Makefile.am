SUBDIRS = mozilla

extensiondir = $(libdir)/epiphany/$(E_API_VERSION)/extensions
extension_LTLIBRARIES = libseahorseextension.la

libseahorseextension_la_SOURCES =	\
	ephy-debug.h \
	eel-gconf-extensions.h \
	seahorse-extension.c	\
	seahorse-extension.h	\
    seahorse.c

libseahorseextension_la_CPPFLAGS = \
    -I$(top_srcdir)/include					\
	-I$/mozilla	\
	-DSHARE_DIR=\"$(pkgdatadir)\"				\
    -DEPHY_EXTENSIONS_LOCALEDIR=\"$(datadir)/locale\"	\
	$(AM_CPPFLAGS) \
	-I$(top_srcdir)/libcryptui \
	-DLIBCRYPTUI_API_SUBJECT_TO_CHANGE

libseahorseextension_la_CFLAGS = \
	$(EPIPHANY_DEPENDENCY_CFLAGS)	\
	$(LIBCRYPTUI_CFLAGS) \
	$(AM_CFLAGS)

libseahorseextension_la_LIBADD =	\
	mozilla/libsbmozilla.la	\
	$(top_builddir)/libcryptui/libcryptui.la \
	$(MOZILLA_COMPONENT_LIBS)

libseahorseextension_la_LDFLAGS = \
	-module -avoid-version \
	$(AM_LDFLAGS)

extensioninidir = $(extensiondir)
extensionini_in_files = seahorse.ephy-extension.in.in
extensionini_DATA = $(extensionini_in_files:.ephy-extension.in.in=.ephy-extension)

%.ephy-extension.in: %.ephy-extension.in.in $(extension_LTLIBRARIES)
	sed -e "s|%LIBRARY%|`. ./$(extension_LTLIBRARIES) && echo $$dlname`|" \
	    -e "s|%EXTENSION_DIR%|$(extensiondir)|" \
	$< > $@
	
@EPIPHANY_EXTENSION_RULE@

CLEANFILES = $(extensionini_DATA)
DISTCLEANFILES = $(extensionini_DATA) 

EXTRA_DIST = $(glade_DATA) $(extensionini_in_files)
