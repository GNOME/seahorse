/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
 * Copyright (C) 2004,2005 Stefan Walter
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2 of the licence or (at
 * your option) any later version.
 *
 * See the included COPYING file for more information.
 */

#include "config.h"

#include "seahorse-application.h"
#include "seahorse-common.h"
#include "seahorse-servers.h"
#include "seahorse-util.h"

#include "seahorse-key-manager.h"

#include <glib/gi18n.h>

#include <locale.h>
#include <stdlib.h>

static void
on_application_activate (GApplication *application,
                         gpointer user_data)
{
	seahorse_key_manager_show ();
}

/* Initializes context and preferences, then loads key manager */
int
main (int argc, char **argv)
{
	GtkApplication *application;
	int status;

#ifdef ENABLE_NLS
	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);
#endif

#if !GLIB_CHECK_VERSION(2,35,0)
	g_type_init ();
#endif

	application = seahorse_application_new ();
	g_signal_connect (application, "activate", G_CALLBACK (on_application_activate), NULL);
	status = g_application_run (G_APPLICATION (application), argc, argv);

	seahorse_registry_cleanup ();
	seahorse_servers_cleanup ();
	g_object_unref (application);

	return status;
}
