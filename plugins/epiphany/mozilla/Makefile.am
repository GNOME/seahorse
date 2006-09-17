noinst_LTLIBRARIES = libsbmozilla.la

libsbmozilla_la_SOURCES = 	\
	mozilla-helper.cpp	\
	mozilla-helper.h

mozilla_include_subdirs = \
	.		\
	content		\
	docshell	\
	dom		\
	gtkembedmoz	\
	webbrwsr	\
	widget		\
	xpcom

libsbmozilla_la_CPPFLAGS = \
	-I..			\
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/libcryptui \
	$(addprefix -I$(MOZILLA_INCLUDE_ROOT)/,$(mozilla_include_subdirs))	\
	-DALLOW_PRIVATE_API		\
	-DSHARE_DIR=\"$(pkgdatadir)\"   \
	$(AM_CPPFLAGS) \
	-DLIBCRYPTUI_API_SUBJECT_TO_CHANGE
	
libsbmozilla_la_CXXFLAGS = \
	$(MOZILLA_COMPONENT_CFLAGS)	\
	$(EPIPHANY_DEPENDENCY_CFLAGS) 	\
	$(LIBCRYPTUI_CFLAGS) \
	$(AM_CXXFLAGS)
