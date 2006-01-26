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

SUBDIRS = libseahorse src po data help pixmaps plugins $(AGENT_DIR) $(NAUTILUS_DIR)

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
	TODO \
	seahorse.spec \
	seahorse.spec.in \
	intltool-extract.in \
	intltool-merge.in \
	intltool-update.in \
	xmldocs.make \
	omf.make
