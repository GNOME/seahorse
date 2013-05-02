NULL =

INCLUDES = -I$(top_builddir) \
	-I$(top_srcdir) \
	$(SEAHORSE_CFLAGS)

noinst_LTLIBRARIES = libcommon.la

libcommon_la_SOURCES = \
	backend.vala \
	deletable.vala \
	deleter.vala \
	exportable.vala \
	exporter.vala \
	place.vala \
	registry.vala \
	viewable.vala \
	$(NULL)

AM_VALAFLAGS = \
	$(VALA_FLAGS) \
	--use-header \
	--header=seahorse-common.h \
	--pkg gtk+-3.0 \
	--pkg gcr-3 \
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
