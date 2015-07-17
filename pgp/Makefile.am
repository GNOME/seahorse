
noinst_LIBRARIES += libseahorse-pgp.a

if WITH_LDAP
LDAP_SRCS = pgp/seahorse-ldap-source.c pgp/seahorse-ldap-source.h
else
LDAP_SRCS = 
endif

if WITH_HKP
HKP_SRCS = pgp/seahorse-hkp-source.c pgp/seahorse-hkp-source.h
else
HKP_SRCS =
endif

if WITH_KEYSERVER
pgp_KEYSERVER_SRCS = pgp/seahorse-server-source.c pgp/seahorse-server-source.h
pgp_KEYSERVER_SRCS += pgp/seahorse-keyserver-search.c pgp/seahorse-keyserver-search.h
pgp_KEYSERVER_SRCS += pgp/seahorse-keyserver-sync.c pgp/seahorse-keyserver-sync.h
pgp_KEYSERVER_SRCS += pgp/seahorse-keyserver-results.c pgp/seahorse-keyserver-results.h
endif

libseahorse_pgp_a_SOURCES = \
	pgp/seahorse-combo-keys.c pgp/seahorse-combo-keys.h \
	pgp/seahorse-discovery.c pgp/seahorse-discovery.h \
	pgp/seahorse-gpgme.c pgp/seahorse-gpgme.h \
	pgp/seahorse-gpgme-add-subkey.c \
	pgp/seahorse-gpgme-add-uid.c \
	pgp/seahorse-gpgme-dialogs.h \
	pgp/seahorse-gpgme-data.c pgp/seahorse-gpgme-data.h \
	pgp/seahorse-gpgme-expires.c \
	pgp/seahorse-gpgme-exporter.c pgp/seahorse-gpgme-exporter.h \
	pgp/seahorse-gpgme-generate.c \
	pgp/seahorse-gpgme-key.c pgp/seahorse-gpgme-key.h \
	pgp/seahorse-gpgme-key-deleter.c pgp/seahorse-gpgme-key-deleter.h \
	pgp/seahorse-gpgme-key-op.c pgp/seahorse-gpgme-key-op.h \
	pgp/seahorse-gpgme-keyring.c pgp/seahorse-gpgme-keyring.h \
	pgp/seahorse-gpgme-photo.c pgp/seahorse-gpgme-photo.h \
	pgp/seahorse-gpgme-photos.c \
	pgp/seahorse-gpgme-revoke.c \
	pgp/seahorse-gpgme-secret-deleter.c pgp/seahorse-gpgme-secret-deleter.h \
	pgp/seahorse-gpgme-sign.c \
	pgp/seahorse-gpgme-subkey.c pgp/seahorse-gpgme-subkey.h \
	pgp/seahorse-gpgme-uid.c pgp/seahorse-gpgme-uid.h \
	pgp/seahorse-gpg-op.h pgp/seahorse-gpg-op.c \
	pgp/seahorse-gpg-options.c pgp/seahorse-gpg-options.h \
	pgp/seahorse-pgp.c pgp/seahorse-pgp.h \
	pgp/seahorse-pgp-actions.c pgp/seahorse-pgp-actions.h \
	pgp/seahorse-pgp-backend.c pgp/seahorse-pgp-backend.h \
	pgp/seahorse-pgp-key.c pgp/seahorse-pgp-key.h \
	pgp/seahorse-pgp-key-properties.c \
	pgp/seahorse-pgp-keysets.c pgp/seahorse-pgp-keysets.h \
	pgp/seahorse-pgp-photo.c pgp/seahorse-pgp-photo.h \
	pgp/seahorse-pgp-signature.c pgp/seahorse-pgp-signature.h \
	pgp/seahorse-pgp-subkey.c pgp/seahorse-pgp-subkey.h \
	pgp/seahorse-pgp-uid.c pgp/seahorse-pgp-uid.h \
	pgp/seahorse-signer.c pgp/seahorse-pgp-dialogs.h \
	pgp/seahorse-transfer.c pgp/seahorse-transfer.h \
	pgp/seahorse-unknown.c pgp/seahorse-unknown.h \
	pgp/seahorse-unknown-source.c pgp/seahorse-unknown-source.h \
	$(pgp_KEYSERVER_SRCS) \
	$(LDAP_SRCS) \
	$(HKP_SRCS)

libseahorse_pgp_a_CFLAGS = \
	-DLDAP_DEPRECATED

seahorselibexecbin_PROGRAMS += xloadimage

xloadimage_SOURCES = pgp/seahorse-xloadimage.c

ui_files += \
	pgp/seahorse-add-subkey.xml \
	pgp/seahorse-add-uid.xml \
	pgp/seahorse-expires.xml \
	pgp/seahorse-keyserver-search.xml \
	pgp/seahorse-keyserver-results.ui \
	pgp/seahorse-keyserver-results.xml \
	pgp/seahorse-keyserver-sync.xml \
	pgp/seahorse-pgp-public-key-properties.xml \
	pgp/seahorse-pgp-private-key-properties.xml \
	pgp/seahorse-pgp-generate.xml \
	pgp/seahorse-revoke.xml \
	pgp/seahorse-sign.xml \
	pgp/seahorse-signer.xml
