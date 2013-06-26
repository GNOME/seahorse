NULL =

uidir = $(datadir)/seahorse/ui/

INCLUDES = -I$(top_builddir) \
	-I$(top_srcdir) \
	-DPKGDATADIR=\""$(pkgdatadir)/"\" \
	-DUIDIR=\""$(uidir)"\" \
	$(SEAHORSE_CFLAGS)

noinst_LTLIBRARIES = libcommon.la

libcommon_la_SOURCES = \
	actions.vala \
	backend.vala \
	catalog.vala \
	deletable.vala \
	delete-dialog.vala \
	deleter.vala \
	exportable.vala \
	exporter.vala \
	icons.vala \
	lockable.vala \
	place.vala \
	registry.vala \
	types.vala \
	util.vala \
	viewable.vala \
	$(NULL)

AM_VALAFLAGS = \
	$(VALA_FLAGS) \
	--use-header \
	--header=seahorse-internal.h \
	--pkg gtk+-3.0 \
	--pkg gcr-3 \
	--pkg config \
	--vapidir=$(srcdir) \
	--vapi=common.vapi \
	$(NULL)

all-local: seahorse-common.h

seahorse-internal.h: backend.c

# Because vala produces broken headers
seahorse-common.h: seahorse-internal.h
	sed '/_constructv/d' $< > $@

CFLAGS += \
	-include config.h -w \
	$(NULL)

EXTRA_DIST = \
	seahorse-common.h \
	seahorse-internal.h \
	common.vapi \
	config.vapi \
	$(NULL)

DISTCLEANFILES = \
	common.vapi \
	$(NULL)
