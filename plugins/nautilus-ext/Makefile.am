
INCLUDES = -I$(top_builddir) 	\
	-I$(top_srcdir) 			\
	$(NAUTILUS_CFLAGS) 			\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"

# There doesn't seem to be a way to get this dynamically
nautilus_extensiondir=$(libdir)/nautilus/extensions-1.0

nautilus_extension_LTLIBRARIES=libnautilus-seahorse.la

libnautilus_seahorse_la_SOURCES =     \
        seahorse-nautilus.c           \
        seahorse-nautilus.h           \
        seahorse-nautilus-module.c

libnautilus_seahorse_la_LDFLAGS = -module -avoid-version -no-undefined
libnautilus_seahorse_la_LIBADD  = $(NAUTILUS_LIBS)
