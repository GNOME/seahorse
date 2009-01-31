noinst_LTLIBRARIES = libeggdatetime.la \
                     libeggsmclient.la \
                     libeggdesktopfile.la

libeggdatetime_la_INCLUDES = -DGTK_DISABLE_DEPRECATED \
	                         -DGDK_DISABLE_DEPRECATED \
	                         -DG_DISABLE_DEPRECATED \
	                         -DGETTEXT_PACKAGE=NULL

libeggdatetime_la_LIBADD = $(GTK_LIBS)
libeggdatetime_la_CFLAGS = $(GTK_CFLAGS)
libeggdatetime_la_SOURCES = egg-datetime.h egg-datetime.c

libeggsmclient_la_INCLUDES = -DEGG_SM_CLIENT_BACKEND_XSMP \
                             -DGTK_DISABLE_DEPRECATED \
	                         -DGDK_DISABLE_DEPRECATED \
	                         -DG_DISABLE_DEPRECATED \
	                         -DGETTEXT_PACKAGE=NULL \
                             -DG_LOG_DOMAIN=\""EggSMClient"\"

libeggsmclient_la_CFLAGS = $(EGG_CFLAGS) \
                           $(EGG_SMCLIENT_CFLAGS) 
                             
libeggsmclient_la_LIBADD = $(EGG_SMCLIENT_LIBS) \
                           $(GTK_LIBS) \
                           libeggdesktopfile.la -lSM -lICE
libeggsmclient_la_SOURCES = eggsmclient.c \
                            eggsmclient.h \
                            eggsmclient-private.h \
                            eggsmclient-xsmp.c \
                            eggdesktopfile.c \
                            eggdesktopfile.h 
                           
libeggdesktopfile_la_INCLUDES = -DGTK_DISABLE_DEPRECATED \
                                -DGDK_DISABLE_DEPRECATED \
    	                        -DG_DISABLE_DEPRECATED \
    	                        -DGETTEXT_PACKAGE=NULL
libeggdesktopfile_la_CFLAGS = $(EGG_CFLAGS)    	                        
libeggdesktopfile_la_LIBADD =  $(EGG_LIBS)
libeggdesktopfile_la_SOURCES = eggdesktopfile.c \
                               eggdesktopfile.h

