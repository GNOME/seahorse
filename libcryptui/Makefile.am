if WITH_TESTS
TESTS_DIR = tests
else
TESTS_DIR = 
endif

SUBDIRS = . $(TESTS_DIR)

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
    cryptui-key-chooser.h \
    crui-x509-cert.h \
    crui-x509-cert-simple.h \
    crui-x509-cert-basics.h \
    crui-x509-cert-dialog.h

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
    crui-asn1.h crui-asn1.c \
    crui-util.h crui-util.c \
    crui-x509-cert.h crui-x509-cert.c \
    crui-x509-cert-basics.h crui-x509-cert-basics.c \
    crui-x509-cert-dialog.h crui-x509-cert-dialog.c \
    crui-x509-cert-simple.h crui-x509-cert-simple.c \
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
# ASN1 FILES

ASN1_FILES = \
	pk.asn \
	pkix.asn
	
TAB_FILES = $(ASN1_FILES:.asn=.tab)

.asn.tab:
	asn1Parser -o $@ $< 
	
# ------------------------------------------------------------------
# UI BUILDER
# 

uidir = $(datadir)/cryptui/ui/

GLADE_FILES = \
	crui-x509-cert-basics.glade
	
.glade.ui: 
	gtk-builder-convert --skip-windows $< $@
	
ui_DATA = $(GLADE_FILES:.glade=.ui)

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
	$(GLADE_FILES) \
	$(ui_DATA) \
	$(TAB_FILES)
    
CLEANFILES = \
	cryptui-marshal.h \
    	cryptui-marshal.c \
    	$(ui_DATA) \
	$(TAB_FILES) \
    	$(pkgconfig_DATA)

BUILT_SOURCES = \
	$(MARSHALLERS) \
	$(TAB_FILES)
