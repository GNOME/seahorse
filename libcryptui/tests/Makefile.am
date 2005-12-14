
INCLUDES = -I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libcryptui \
	$(LIBCRYPTUI_CFLAGS) 

bin_PROGRAMS = cryptui-test-keyset cryptui-test-ui

cryptui_test_keyset_SOURCES = cryptui-test-keyset.c
cryptui_test_keyset_LDADD = $(top_builddir)/libcryptui/libcryptui.la \
	                        $(LIBCRYPTUI_LIBS)

cryptui_test_ui_SOURCES = cryptui-test-ui.c
cryptui_test_ui_LDADD = $(top_builddir)/libcryptui/libcryptui.la \
	                        $(LIBCRYPTUI_LIBS)
