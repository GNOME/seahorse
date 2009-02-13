AUTOMAKE_OPTIONS = 1.6

# The name of the module.
DOC_MODULE=libcryptui

# The top-level SGML file.
DOC_MAIN_SGML_FILE=libcryptui-docs.sgml

# Extra options to supply to gtkdoc-scan
SCAN_OPTIONS=--deprecated-guards="GTK_ENABLE_BROKEN|GTK_DISABLE_DEPRECATED" 

# Extra options to pass to gtkdoc-scangobj
SCANGOBJ_OPTIONS=--type-init-func="gtk_type_init(0)" 

# The directory containing the source code. Relative to $(srcdir)
DOC_SOURCE_DIR=../../libcryptui

# Used for dependencies
HFILE_GLOB=$(top_srcdir)/libcryptui/*.h
CFILE_GLOB=$(top_srcdir)/libcryptui/*.c

# Header files to ignore when scanning
IGNORE_HFILES=

# CFLAGS and LDFLAGS for compiling scan program. Only needed
# if $(DOC_MODULE).types is non-empty.
INCLUDES = \
	-I$(top_srcdir) 	\
	-I../../libcryptui 	\
	-DGTK_ENABLE_BROKEN
	
CFLAGS = $(GTK_CFLAGS)
LIBS = $(GTK_LIBS)

CPPFLAGS += \
	-UGTK_DISABLE_SINGLE_INCLUDES

GTKDOC_LIBS = $(top_builddir)/libcryptui/$(cryptuitargetlib)
#	$(top_builddir)/gdk-pixbuf/libgdk_pixbuf-$(GTK_API_VERSION).la	\
#	$(top_builddir)/gdk/$(gdktargetlib)        			\
#	$(top_builddir)/gtk/$(gtktargetlib)


# Extra options to supply to gtkdoc-mkdb
MKDB_OPTIONS=--sgml-mode --output-format=xml --name-space=cryptui

# Extra SGML files that are included by $(DOC_MAIN_SGML_FILE)
content_files = version.xml

expand_content_files =

# Images to copy into HTML directory
HTML_IMAGES = 
# Extra options to supply to gtkdoc-fixref
FIXXREF_OPTIONS= \
	--extra-dir=$(GLIB_PREFIX)/share/gtk-doc/html/gobject \
	--extra-dir=$(GLIB_PREFIX)/share/gtk-doc/html/glib \
	--extra-dir=$(ATK_PREFIX)/share/gtk-doc/html/atk \
	--extra-dir=$(PANGO_PREFIX)/share/gtk-doc/html/pango \
	--extra-dir=$(CAIRO_PREFIX)/share/gtk-doc/html/cairo

include $(top_srcdir)/gtk-doc.make

# Other files to distribute
EXTRA_DIST += version.xml.in
