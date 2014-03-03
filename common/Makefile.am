
noinst_LTLIBRARIES += libcommon.la

common_VALA = \
	common/actions.vala \
	common/backend.vala \
	common/catalog.vala \
	common/deletable.vala \
	common/delete-dialog.vala \
	common/deleter.vala \
	common/exportable.vala \
	common/exporter.vala \
	common/icons.vala \
	common/lockable.vala \
	common/place.vala \
	common/registry.vala \
	common/types.vala \
	common/util.vala \
	common/viewable.vala \
	$(NULL)

common_C = $(common_VALA:.vala=.c)

common_HEADER = common/seahorse-common.h

libcommon_la_SOURCES = \
	$(common_C) \
	common/seahorse-common.h

libcommon_la_CFLAGS = \
	-include config.h -w

$(common_HEADER): $(common_VALA)
	$(V_VALAC) $(VALAC) $(VALA_FLAGS) -C --use-header --header=$(common_HEADER) \
		--vapidir=$(srcdir)/common --pkg gtk+-3.0 --pkg gcr-3 --pkg config \
		--vapi=common.vapi --directory=common $^

$(common_C): $(common_HEADER)

common/common.vapi: $(common_HEADER)

common_BUILT = \
	$(common_C) \
	$(common_HEADER) \
	common/common.vapi

BUILT_SOURCES += $(common_BUILT)

EXTRA_DIST += \
	$(common_BUILT) \
	$(common_VALA) \
	common/config.vapi \
	$(NULL)
