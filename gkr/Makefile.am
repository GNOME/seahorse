
noinst_LIBRARIES += libseahorse-gkr.a

gkr_VALA = \
	gkr/gkr-backend.vala \
	gkr/gkr-dialogs.vala \
	gkr/gkr-module.vala \
	gkr/gkr-item.vala \
	gkr/gkr-item-add.vala \
	gkr/gkr-item-properties.vala \
	gkr/gkr-keyring.vala \
	gkr/gkr-keyring-add.vala \
	gkr/gkr-keyring-properties.vala \
	$(NULL)

gkr_HEADER = gkr/seahorse-gkr.h

gkr_C = $(gkr_VALA:.vala=.c)

libseahorse_gkr_a_SOURCES = \
	$(gkr_C) $(gkr_HEADER)

libseahorse_gkr_a_CFLAGS = \
	-include config.h -w \
	-DSECRET_API_SUBJECT_TO_CHANGE

if WITH_VALAC
$(gkr_HEADER): $(gkr_VALA) $(common_VAPI)
	$(V_VALAC) $(VALAC) $(VALA_FLAGS) -C --use-header --header=$(gkr_HEADER) \
		--pkg gtk+-3.0 --pkg gcr-3 --pkg gcr-ui-3 --pkg libsecret-1 \
		--directory=$(builddir)/gkr --basedir=$(dir $<) $^
endif

$(gkr_C): $(gkr_HEADER)

gkr_BUILT = \
	$(gkr_C) \
	$(gkr_HEADER)

BUILT_SOURCES += $(gkr_BUILT)

EXTRA_DIST += \
	$(gkr_BUILT) \
	$(gkr_VALA) \
	$(NULL)

ui_files += \
	gkr/seahorse-add-keyring.ui \
	gkr/seahorse-gkr-add-item.ui \
	gkr/seahorse-gkr-item-properties.ui \
	gkr/seahorse-gkr-keyring.ui

-include $(top_srcdir)/git.mk
