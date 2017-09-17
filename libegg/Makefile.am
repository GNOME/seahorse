noinst_LIBRARIES += \
	libeggdatetime.a \
	libeggtreemultidnd.a

libeggdatetime_a_CPPFLAGS = -DGTK_DISABLE_DEPRECATED \
	                         -DGDK_DISABLE_DEPRECATED \
	                         -DG_DISABLE_DEPRECATED

libeggdatetime_a_CFLAGS = $(GTK_CFLAGS)
libeggdatetime_a_SOURCES = libegg/egg-datetime.h libegg/egg-datetime.c

libeggtreemultidnd_a_CPPFLAGS = -DGTK_DISABLE_DEPRECATED \
	                         -DGDK_DISABLE_DEPRECATED \
	                         -DG_DISABLE_DEPRECATED

libeggtreemultidnd_a_CFLAGS = $(GTK_CFLAGS)
libeggtreemultidnd_a_SOURCES = libegg/eggtreemultidnd.c libegg/eggtreemultidnd.h

-include $(top_srcdir)/git.mk
