## Process this file with automake to produce Makefile.in

if WITH_AGENT
AGENT_DIR = agent
else
AGENT_DIR = 
endif

SUBDIRS = libcryptui \
          libseahorse \
          src \
          po \
          data \
          help \
          pixmaps \
          plugins \
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
    gnome-doc-utils.make \
    intltool-extract \
    intltool-merge \
    intltool-update
    
DISTCHECK_CONFIGURE_FLAGS = \
    --disable-scrollkeeper \
    --disable-update-mime-database

distuninstallcheck:
	@:
