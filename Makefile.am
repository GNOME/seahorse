## Process this file with automake to produce Makefile.in

if WITH_AGENT
AGENT_DIR = agent
else
AGENT_DIR = 
endif

if WITH_NAUTILUS
NAUTILUS_DIR = bonobo
else
NAUTILUS_DIR = 
endif

if WITH_DBUS
DBUS_DIR = libcryptui
else
DBUS_DIR = 
endif

SUBDIRS = libseahorse \
          $(DBUS_DIR) \
          src \
          po \
          data \
          help \
          pixmaps \
          plugins \
          $(NAUTILUS_DIR) \
          $(AGENT_DIR) \
          daemon 

EXTRA_DIST = \
    AUTHORS \
    COPYING \
    COPYING-DOCS \
    COPYING-LIBCRYPTUI \
    ChangeLog \
    HACKING \
    INSTALL \
    MAINTAINERS \
    NEWS \
    README \
    THANKS \
    TODO \
    seahorse.spec \
    seahorse.spec.in \
    intltool-extract.in \
    intltool-merge.in \
    intltool-update.in \
    gnome-doc-utils.make \
    omf.make \
    m4

DISTCLEANFILES = \
    gnome-doc-utils.make
    
DISTCHECK_CONFIGURE_FLAGS = \
    --disable-scrollkeeper
