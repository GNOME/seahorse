if WITH_TESTS
TESTS_DIR = tests
else
TESTS_DIR = 
endif

SUBDIRS = . $(TESTS_DIR)

incdir = $(includedir)/libcryptui

inc_HEADERS = \
    cryptui.h \
    cryptui-keyset.h \
    cryptui-key-store.h \
    cryptui-key-combo.h \
    cryptui-key-list.h \
    cryptui-key-chooser.h

INCLUDES = -I$(top_builddir) \
    -I$(top_srcdir) \
    $(LIBCRYPTUI_CFLAGS) \
    -DLIBCRYPTUI_BUILD=1

lib_LTLIBRARIES = libcryptui.la

BUILT_SOURCES = \
    cryptui-marshal.c cryptui-marshal.h

libcryptui_la_SOURCES = \
    cryptui.h cryptui.c cryptui-defines.h cryptui-priv.h \
    cryptui-keyset.h cryptui-keyset.c \
    cryptui-key-store.h cryptui-key-store.c \
    cryptui-key-combo.h cryptui-key-combo.c \
    cryptui-key-list.h cryptui-key-list.c \
    cryptui-key-chooser.h cryptui-key-chooser.c \
    $(BUILT_SOURCES)
    
libcryptui_la_LDFLAGS = \
    -version-info $(LIBCRYPTUI_LT_RELEASE)
    
libcryptui_la_LIBADD = $(LIBCRYPTUI_LIBS) 

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = cryptui-$(LIBCRYPTUI_MAJOR).pc

EXTRA_DIST = \
    cryptui-marshal.list \
    cryptui.pc.in
    
CLEANFILES = \
    cryptui-marshal.h \
    cryptui-marshal.c \
    $(pkgconfig_DATA)

cryptui-marshal.h: cryptui-marshal.list $(GLIB_GENMARSHAL)
	$(GLIB_GENMARSHAL) $< --header --prefix=cryptui_marshal > $@

cryptui-marshal.c: cryptui-marshal.list $(GLIB_GENMARSHAL)
	echo "#include \"cryptui-marshal.h\"" > $@ && \
	$(GLIB_GENMARSHAL) $< --body --prefix=cryptui_marshal >> $@

cryptui-$(LIBCRYPTUI_MAJOR).pc: cryptui.pc
	cp cryptui.pc cryptui-$(LIBCRYPTUI_MAJOR).pc
