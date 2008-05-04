## Process this file with automake to produce Makefile.in

ACLOCAL_AMFLAGS = -I m4

if WITH_SSH
SSH_DIR = ssh
else
SSH_DIR = 
endif

SUBDIRS = libegg \
	libcryptui \
	common \
	libseahorse \
	gkr \
	pgp \
	$(SSH_DIR) \
	src \
	po \
	data \
	help \
	pixmaps \
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
    omf.make 

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
