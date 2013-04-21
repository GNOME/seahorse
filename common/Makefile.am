NULL =

INCLUDES = -I$(top_builddir) \
	-I$(top_srcdir) \
	$(SEAHORSE_CFLAGS)

noinst_LTLIBRARIES = libcommon.la

libcommon_la_SOURCES = \
	seahorse-deletable.vala \
	seahorse-deleter.vala \
	seahorse-exportable.vala \
	seahorse-exporter.vala \
	seahorse-viewable.vala \
	$(NULL)

AM_VALAFLAGS = \
	$(VALA_FLAGS) \
	--use-header \
	--header=seahorse-common.h \
	--pkg gtk+-3.0 \
	--vapi=common.vapi \
	$(NULL)

CFLAGS += \
	-include config.h -w \
	$(NULL)

EXTRA_DIST = \
	seahorse-common.h \
	common.vapi \
	$(NULL)

DISTCLEANFILES = \
	common.vapi \
	$(NULL)
