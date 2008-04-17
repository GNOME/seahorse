## Process this file with automake to produce Makefile.in

ACLOCAL_AMFLAGS = -I m4

if WITH_AGENT
AGENT_DIR = agent
else
AGENT_DIR = 
endif

SUBDIRS = libseahorse \
          po \
          data \
          help \
          pixmaps \
          plugins \
          $(AGENT_DIR)

EXTRA_DIST = \
    AUTHORS \
    COPYING \
    COPYING-DOCS \
    ChangeLog \
    HACKING \
    INSTALL \
    MAINTAINERS \
    NEWS \
    README \
    THANKS \
    intltool-extract.in \
    intltool-merge.in \
    intltool-update.in \
    gnome-doc-utils.make \
    omf.make 

DISTCLEANFILES = \
    gnome-doc-utils.make \
    intltool-extract \
    intltool-merge \
    intltool-update
    
DISTCHECK_CONFIGURE_FLAGS = \
    --disable-scrollkeeper \
    --disable-update-mime-database \
    --with-nautilus-ext-dir=/tmp

distuninstallcheck:
	@:
