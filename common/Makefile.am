
localedir = $(datadir)/locale

INCLUDES = -I$(top_builddir) \
	-I$(top_srcdir) \
	$(SEAHORSE_CFLAGS)

noinst_LTLIBRARIES = libseahorse-common.la

libseahorse_common_la_SOURCES = \
	seahorse-cleanup.c seahorse-cleanup.h \
	seahorse-registry.c seahorse-registry.h
	