
if WITH_TESTS
TESTS_DIR = tests
else
TESTS_DIR = 
endif

SUBDIRS = . $(TESTS_DIR)

INCLUDES = -I$(top_builddir) \
	-I$(top_srcdir) \
	$(LIBCRYPTUI_CFLAGS)

lib_LTLIBRARIES = libcryptui.la

BUILT_SOURCES = \
	cryptui-marshal.c cryptui-marshal.h

libcryptui_la_SOURCES = \
	cryptui.h cryptui.c \
	cryptui-keyset.h cryptui-keyset.c \
	cryptui-key-store.h cryptui-key-store.c \
	cryptui-key-combo.h cryptui-key-combo.c \
	cryptui-key-list.h cryptui-key-list.c \
	cryptui-key-chooser.h cryptui-key-chooser.c \
	$(BUILT_SOURCES)

cryptui-marshal.h: cryptui-marshal.list $(GLIB_GENMARSHAL)
	$(GLIB_GENMARSHAL) $< --header --prefix=cryptui_marshal > $@

cryptui-marshal.c: cryptui-marshal.list $(GLIB_GENMARSHAL)
	echo "#include \"cryptui-marshal.h\"" > $@ && \
	$(GLIB_GENMARSHAL) $< --body --prefix=cryptui_marshal >> $@
