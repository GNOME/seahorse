
INCLUDES = -I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libcryptui \
	$(LIBCRYPTUI_CFLAGS) 

bin_PROGRAMS = cryptui-test-keyset

cryptui_test_keyset_SOURCES = cryptui-test-keyset.c
cryptui_test_keyset_LDADD = $(top_builddir)/libcryptui/libcryptui.la \
	                        $(LIBCRYPTUI_LIBS)
