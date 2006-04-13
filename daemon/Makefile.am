
gladedir = $(datadir)/seahorse/glade/
localedir = $(datadir)/locale
pixmapsdir = $(datadir)/pixmaps/

INCLUDES = -I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libcryptui \
	-I$(top_srcdir)/libseahorse \
	-I$(top_srcdir)/agent \
	$(SEAHORSE_CFLAGS) \
	-DDATA_DIR=\""$(datadir)"\" \
	-DSEAHORSE_GLADEDIR=\""$(gladedir)"\" \
	-DLOCALEDIR=\""$(localedir)"\" \
	-DPIXMAPSDIR=\""$(pixmapsdir)"\"

bin_PROGRAMS = seahorse-daemon

BUILT_SOURCES = 

if WITH_AGENT
AGENT_LIB = $(top_builddir)/agent/libseahorseagent.a
else
AGENT_LIB = 
endif

if WITH_SHARING
SHARING_SRCS = seahorse-hkp-server.c seahorse-sharing.c 
else
SHARING_SRCS = 
endif

if WITH_DBUS
DBUS_SRCS = seahorse-dbus-server.c seahorse-service.c seahorse-service.h 
DBUS_SRCS += seahorse-service-keyset.c seahorse-service-crypto.c
BUILT_SOURCES += seahorse-service-bindings.h seahorse-service-keyset-bindings.h
BUILT_SOURCES += seahorse-service-crypto-bindings.h
else
DBUS_SRCS = 
endif

seahorse_daemon_SOURCES = \
		seahorse-daemon.c \
		seahorse-daemon.h \
		$(DBUS_SRCS) \
		$(SHARING_SRCS)

seahorse_daemon_LDADD = \
    $(AGENT_LIB) \
	$(top_builddir)/libseahorse/libseahorse-internal.la \
	$(SEAHORSE_LIBS)

glade_DATA = 

EXTRA_DIST = \
	$(glade_DATA) \
	seahorse-service.xml \
	seahorse-service-keyset.xml \
	seahorse-service-bindings.h \
	seahorse-service-keyset-bindings.h

CLEANFILES = *.gladep* *.bak 

# DBUS binding files
seahorse-service-bindings.h: seahorse-service.xml
	dbus-binding-tool --prefix=seahorse_service --mode=glib-server --output=$@ $(srcdir)/$<
seahorse-service-keyset-bindings.h: seahorse-service-keyset.xml
	dbus-binding-tool --prefix=seahorse_service_keyset --mode=glib-server --output=$@ $(srcdir)/$<
seahorse-service-crypto-bindings.h: seahorse-service-crypto.xml
	dbus-binding-tool --prefix=seahorse_service_crypto --mode=glib-server --output=$@ $(srcdir)/$<

# Dbus service file
servicedir = $(DBUS_SERVICES_DIR)
service_in_files = org.gnome.seahorse.service.in
service_DATA = $(service_in_files:.service.in=.service)

# Rule to make the service file with bindir expanded
$(service_DATA): $(service_in_files) Makefile
	@sed -e "s|\@bindir\@|$(bindir)|" $< > $@

# Make the daemon setuid
install-exec-hook:
	chmod u+s $(DESTDIR)$(bindir)/seahorse-daemon
