## Process this file with automake to produce Makefile.in

ACLOCAL_AMFLAGS = -I m4

if WITH_PKCS11
PKCS11_DIR = pkcs11
else
PKCS11_DIR = 
endif

if WITH_SSH
SSH_DIR = ssh
else
SSH_DIR = 
endif

if WITH_PGP
PGP_DIR = pgp
else
PGP_DIR = 
endif

SUBDIRS = libegg \
	libcryptui \
	common \
	libseahorse \
	gkr \
	$(PKCS11_DIR) \
	$(PGP_DIR) \
	$(SSH_DIR) \
	src \
	po \
	data \
	help \
	pixmaps \
	daemon \
	reference

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
    --disable-update-mime-database \
    --enable-gtk-doc
    
distuninstallcheck:
	@:
