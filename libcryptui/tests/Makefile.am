
noinst_PROGRAMS= \
	run-tests \
	crui-show \
	cryptui-test
	
# ------------------------------------------------------------------------------
# UI TEST PROGRAMS

cryptui_test_SOURCES = cryptui-test-ui.c
cryptui_test_LDADD = $(LIBCRYPTUI_LIBS) \
	$(top_builddir)/libcryptui/libcryptui.la 

crui_show_SOURCES = crui-show.c
crui_show_LDADD = $(LIBCRYPTUI_LIBS) \
	$(top_builddir)/libcryptui/libcryptui.la

# ------------------------------------------------------------------------------
# UNIT TEST PROGRAMS

UNIT_TESTS = \
	unit-test-asn1.c \
	unit-test-util.c \
	unit-test-x509-cert.c
	
run-tests.c: $(UNIT_TESTS) Makefile.am gtest-prep.sh
	sh gtest-prep.sh -b run-tests $(UNIT_TESTS)

run_tests_SOURCES = \
	run-tests.c run-tests.h \
	$(UNIT_TESTS)

run_tests_LDADD = $(LIBCRYPTUI_LIBS) \
	$(top_builddir)/libcryptui/libcryptui.la

# ------------------------------------------------------------------------------
# COMMON

INCLUDES = -I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libcryptui \
	-DLIBCRYPTUI_API_SUBJECT_TO_CHANGE \
	$(LIBCRYPTUI_CFLAGS) 

EXTRA_DIST = \
	gtest-helpers.c \
	gtest-helpers.h \
	gtest-prep.sh \
	$(UNIT_TESTS)