
gladedir = $(datadir)/seahorse/glade/
localedir = $(datadir)/locale
pixmapsdir = $(datadir)/pixmaps/

INCLUDES = -I$(top_builddir) \
	-I$(top_srcdir) \
	$(SEAHORSE_CFLAGS) \
	-DDATA_DIR=\""$(datadir)"\" \
	-DSEAHORSE_GLADEDIR=\""$(gladedir)"\" \
	-DLOCALEDIR=\"$(localedir)\" \
	-DPIXMAPSDIR=\""$(pixmapsdir)"\"

lib_LTLIBRARIES = libseahorse-internal.la

BUILT_SOURCES = \
	seahorse-marshal.c seahorse-marshal.h

if WITH_AGENT
AGENT_SRCS = seahorse-prefs-cache.c
else
AGENT_SRCS = 
endif

if WITH_LDAP
LDAP_SRCS = seahorse-ldap-source.c seahorse-ldap-source.h
else
LDAP_SRCS = 
endif

if WITH_HKP
HKP_SRCS = seahorse-hkp-source.c seahorse-hkp-source.h
else
HKP_SRCS =
endif

if WITH_KEYSERVER
KEYSERVER_SRCS = seahorse-server-source.c seahorse-server-source.h 
KEYSERVER_SRCS += seahorse-keyserver-control.c seahorse-keyserver-control.h
else
KEYSERVER_SRCS = 
endif

libseahorse_internal_la_SOURCES = \
	seahorse-key.c seahorse-key.h \
	seahorse-key-pair.c seahorse-key-pair.h \
	seahorse-context.c seahorse-context.h \
	seahorse-widget.c seahorse-widget.h \
	seahorse-op.c seahorse-op.h \
	seahorse-key-store.c seahorse-key-store.h \
	seahorse-recipients-store.c seahorse-recipients-store.h \
	seahorse-util.c seahorse-util.h \
	seahorse-validity.c seahorse-validity.h \
	seahorse-check-button-control.c seahorse-check-button-control.h \
	seahorse-default-key-control.c seahorse-default-key-control.h \
	seahorse-gpg-options.c seahorse-gpg-options.h \
	seahorse-passphrase.c \
	seahorse-recipients.c \
    seahorse-signer.c \
	seahorse-signatures.c \
	seahorse-libdialogs.h \
	seahorse-vfs-data.c seahorse-vfs-data.h \
	seahorse-key-source.c seahorse-key-source.h \
	seahorse-pgp-source.c seahorse-pgp-source.h \
	seahorse-multi-source.c seahorse-multi-source.h \
	seahorse-gpgmex.h seahorse-gpgmex-op.c seahorse-gpgmex-util.c \
	seahorse-prefs.c seahorse-prefs.h \
	seahorse-operation.c seahorse-operation.h \
	seahorse-progress.c seahorse-progress.h \
    seahorse-gconf.c seahorse-gconf.h \
	$(BUILT_SOURCES) \
	$(AGENT_SRCS) \
	$(KEYSERVER_SRCS) \
    $(LDAP_SRCS) \
    $(HKP_SRCS)
    

seahorse-marshal.h: seahorse-marshal.list $(GLIB_GENMARSHAL)
	$(GLIB_GENMARSHAL) $< --header --prefix=seahorse_marshal > $@

seahorse-marshal.c: seahorse-marshal.list $(GLIB_GENMARSHAL)
	echo "#include \"seahorse-marshal.h\"" > $@ && \
	$(GLIB_GENMARSHAL) $< --body --prefix=seahorse_marshal >> $@

glade_DATA = \
	seahorse-passphrase.glade \
	seahorse-recipients.glade \
    seahorse-signer.glade \
	seahorse-signatures.glade \
	seahorse-prefs.glade \
    seahorse-add-keyserver.glade
	seahorse-progress.glade

EXTRA_DIST = $(glade_DATA) \
	seahorse-marshal.list 
