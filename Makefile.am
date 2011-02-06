## Process this file with automake to produce Makefile.in

ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

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
    ChangeLog.pre-auto \
    ChangeLog.pre-1.0 \
    ChangeLog.pre-0.6 \
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
    --enable-gtk-doc \
    --disable-debug
    
CHANGELOG_START = \
	d2f8367dc600080b6cf3edbe4e2fc204f929471e

dist-hook:
	@if test -d "$(srcdir)/.git"; \
	then \
		echo Creating ChangeLog && \
		( cd "$(top_srcdir)" && \
		  echo '# Generated automatically. Do not edit.'; echo; \
		  $(top_srcdir)/missing --run git log $(CHANGELOG_START).. --stat --date=short ) > ChangeLog.tmp \
		&& mv -f ChangeLog.tmp $(top_distdir)/ChangeLog \
		|| ( rm -f ChangeLog.tmp ; \
		     echo Failed to generate ChangeLog >&2 ); \
	else \
		echo A git clone is required to generate a ChangeLog >&2; \
	fi

distuninstallcheck:
	@:
