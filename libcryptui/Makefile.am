# -----------------------------------------------------------------
# LIBRARY HEADERS
# 

incdir = $(includedir)/libcryptui

inc_HEADERS = \
    cryptui.h \
    cryptui-keyset.h \
    cryptui-key-store.h \
    cryptui-key-combo.h \
    cryptui-key-list.h \
    cryptui-key-chooser.h

# -----------------------------------------------------------------
# MARSHALLERS
# 

MARSHALLERS = \
    cryptui-marshal.c cryptui-marshal.h

cryptui-marshal.h: cryptui-marshal.list $(GLIB_GENMARSHAL)
	$(GLIB_GENMARSHAL) $< --header --prefix=cryptui_marshal > $@

cryptui-marshal.c: cryptui-marshal.list $(GLIB_GENMARSHAL)
	echo "#include \"cryptui-marshal.h\"" > $@ && \
	$(GLIB_GENMARSHAL) $< --body --prefix=cryptui_marshal >> $@
	

# -----------------------------------------------------------------
# LIBRARY SOURCES
# 

lib_LTLIBRARIES = libcryptui.la

libcryptui_la_SOURCES = \
    cryptui.h cryptui.c cryptui-defines.h cryptui-priv.h \
    cryptui-keyset.h cryptui-keyset.c \
    cryptui-key-store.h cryptui-key-store.c \
    cryptui-key-combo.h cryptui-key-combo.c \
    cryptui-key-list.h cryptui-key-list.c \
    cryptui-key-chooser.h cryptui-key-chooser.c \
    $(MARSHALLERS)
    
libcryptui_la_LDFLAGS = \
    -version-info $(LIBCRYPTUI_LT_RELEASE)
    
libcryptui_la_LIBADD = $(LIBCRYPTUI_LIBS) 

# ------------------------------------------------------------------
# PKGCONFIG
# 

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = cryptui-$(LIBCRYPTUI_MAJOR).pc

cryptui-$(LIBCRYPTUI_MAJOR).pc: cryptui.pc
	cp cryptui.pc cryptui-$(LIBCRYPTUI_MAJOR).pc

# ------------------------------------------------------------------
# COMMON

INCLUDES = -I$(top_builddir) \
	-I$(top_srcdir) \
	$(LIBCRYPTUI_CFLAGS) \
	-DLIBCRYPTUI_BUILD=1 \
	-DLIBCRYPTUI_API_SUBJECT_TO_CHANGE \
	-DUIDIR=\""$(uidir)"\"

EXTRA_DIST = \
	cryptui-marshal.list \
	cryptui.pc.in \
	$(GLADE_FILES)
    
CLEANFILES = \
	cryptui-marshal.h \
    	cryptui-marshal.c \
    	$(pkgconfig_DATA)

BUILT_SOURCES = \
	$(MARSHALLERS)
