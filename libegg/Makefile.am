noinst_LTLIBRARIES = \
	libeggdatetime.la \
	libeggtreemultidnd.la

libeggdatetime_la_INCLUDES = -DGTK_DISABLE_DEPRECATED \
	                         -DGDK_DISABLE_DEPRECATED \
	                         -DG_DISABLE_DEPRECATED \
	                         -DGETTEXT_PACKAGE=NULL

libeggdatetime_la_LIBADD = $(GTK_LIBS)
libeggdatetime_la_CFLAGS = $(GTK_CFLAGS)
libeggdatetime_la_SOURCES = egg-datetime.h egg-datetime.c

libeggtreemultidnd_la_INCLUDES = -DGTK_DISABLE_DEPRECATED \
	                         -DGDK_DISABLE_DEPRECATED \
	                         -DG_DISABLE_DEPRECATED \
	                         -DGETTEXT_PACKAGE=NULL

libeggtreemultidnd_la_LIBADD = $(GTK_LIBS)
libeggtreemultidnd_la_CFLAGS = $(GTK_CFLAGS)
libeggtreemultidnd_la_SOURCES = eggtreemultidnd.c eggtreemultidnd.h
