
noinst_LTLIBRARIES += \
	libseahorse-pkcs11.la

pkcs11_VALA = \
	pkcs11/certificate-der-exporter.vala \
	pkcs11/pkcs11-certificate.vala \
	pkcs11/pkcs11-deleter.vala \
	pkcs11/pkcs11-key-deleter.vala \
	pkcs11/pkcs11-module.vala \
	pkcs11/pkcs11-private-key.vala \
	pkcs11/pkcs11-properties.vala \
	pkcs11/pkcs11-request.vala \
	pkcs11/pkcs11-token.vala \
	$(NULL)

pkcs11_HEADER = pkcs11/seahorse-pkcs11.h

pkcs11_C = $(pkcs11_VALA:.vala=.c)

libseahorse_pkcs11_la_SOURCES = \
	$(pkcs11_C) $(pkcs11_HEADER) \
	pkcs11/seahorse-pkcs11-backend.c pkcs11/seahorse-pkcs11-backend.h \
	pkcs11/seahorse-pkcs11-generate.c pkcs11/seahorse-pkcs11-generate.h

libseahorse_pkcs11_la_CFLAGS = \
	-include config.h -w

if WITH_VALAC
$(pkcs11_HEADER): $(pkcs11_VALA)
	$(V_VALAC) $(VALAC) $(VALA_FLAGS) -C --use-header --header=$(pkcs11_HEADER) \
		--vapidir=$(builddir)/common --vapidir=$(srcdir)/common \
		--pkg common --pkg config --pkg gtk+-3.0  --pkg gcr-3 --pkg gcr-ui-3 --pkg pkcs11 \
		--directory=pkcs11 $^
endif

$(pkcs11_C): $(pkcs11_HEADER)

pkcs11_BUILT = \
	$(pkcs11_C) \
	$(pkcs11_HEADER)

BUILT_SOURCES += $(pkcs11_BUILT)

EXTRA_DIST += \
	$(pkcs11_BUILT) \
	$(pkcs11_VALA) \
	$(NULL)

ui_DATA += \
	pkcs11/seahorse-pkcs11-generate.xml \
	pkcs11/seahorse-pkcs11-request.xml
