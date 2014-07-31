include Makefile.decl

ACLOCAL_AMFLAGS = -I build/m4 ${ACLOCAL_FLAGS}

SUBDIRS = \
	. \
	po \
	data \
	help

EXTRA_DIST = \
    AUTHORS \
    COPYING \
    COPYING-DOCS \
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
    intltool-extract.in \
    intltool-merge.in \
    intltool-update.in

DISTCLEANFILES = \
    intltool-extract \
    intltool-merge \
    intltool-update
    
DISTCHECK_CONFIGURE_FLAGS = \
    --disable-scrollkeeper \
    --disable-update-mime-database \
    --disable-debug \
    --disable-strict \
    --disable-valac \
    --enable-pgp

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

upload-release: $(DIST_ARCHIVES)
	scp $(DIST_ARCHIVES) master.gnome.org:
	ssh master.gnome.org ftpadmin install $(DIST_ARCHIVES)

noinst_LIBRARIES =
bin_PROGRAMS =
man_MANS =

pkgdatadir = $(datadir)/seahorse
localedir = $(datadir)/locale

uidir = $(datadir)/seahorse/ui/
ui_DATA =

seahorselibexecbindir = $(libdir)/seahorse/
seahorselibexecbin_PROGRAMS =

BUILT_SOURCES =
CLEANFILES = *.c */*.c *.h */*.h $(BUILT_SOURCES)
EXTRA_DIST += $(ui_DATA)
DISTCLEANFILES +=

AM_CPPFLAGS = \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_builddir)/common \
	-I$(top_srcdir)/common \
	-DPKGDATADIR=\""$(pkgdatadir)/"\" \
	-DUIDIR=\""$(uidir)"\" \
	-DSEAHORSE_UIDIR=\""$(uidir)/"\" \
	-DLOCALEDIR=\"$(localedir)\" \
	-DGETTEXT_PACKAGE=\""seahorse\"" \
	-DEXECDIR=\""$(seahorselibexecbindir)"\" \
	-DG_LOG_DOMAIN=\"seahorse\" \
	$(SEAHORSE_CFLAGS) \
	$(NULL)

V_VALAC = $(V_VALAC_$(V))
V_VALAC_ = $(V_VALAC_$(AM_DEFAULT_VERBOSITY))
V_VALAC_0 = @echo "  VALAC   " $@;

include common/Makefile.am
include libegg/Makefile.am
include libseahorse/Makefile.am
include gkr/Makefile.am
include src/Makefile.am

if WITH_PGP
include pgp/Makefile.am
endif

if WITH_SSH
include ssh/Makefile.am
endif

if WITH_PKCS11
include pkcs11/Makefile.am
endif
