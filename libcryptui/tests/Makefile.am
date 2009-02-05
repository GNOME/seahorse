
noinst_PROGRAMS= \
	cryptui-test
	
# ------------------------------------------------------------------------------
# UI TEST PROGRAMS

cryptui_test_SOURCES = cryptui-test-ui.c
cryptui_test_LDADD = $(LIBCRYPTUI_LIBS) \
	$(top_builddir)/libcryptui/libcryptui.la 

# ------------------------------------------------------------------------------
# COMMON

INCLUDES = -I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libcryptui \
	-DLIBCRYPTUI_API_SUBJECT_TO_CHANGE \
	$(LIBCRYPTUI_CFLAGS) 
