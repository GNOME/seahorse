/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
 * Copyright (C) 2004,2005 Stefan Walter
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see
 * <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "seahorse-key-manager.h"

#include "seahorse-common.h"
#include "seahorse-resources.h"

#include "libseahorse/seahorse-application.h"
#include "libseahorse/seahorse-servers.h"
#include "libseahorse/seahorse-util.h"

#include "pgp/seahorse-pgp.h"
#include "ssh/seahorse-ssh.h"

#include <glib/gi18n.h>

#include <locale.h>
#include <stdlib.h>

static void
on_application_activate (GApplication *application,
                         gpointer user_data)
{
	seahorse_key_manager_show (GDK_CURRENT_TIME);
}

#ifdef WITH_PKCS11
void seahorse_pkcs11_backend_initialize (void);
#endif

void seahorse_gkr_backend_initialize (void);

static void
on_application_startup (GApplication *application,
                        gpointer user_data)
{
	/* Initialize the various components */
#ifdef WITH_PGP
	seahorse_pgp_backend_initialize ();
#endif
#ifdef WITH_SSH
	seahorse_ssh_backend_initialize ();
#endif
#ifdef WITH_PKCS11
	seahorse_pkcs11_backend_initialize ();
#endif
	seahorse_gkr_backend_initialize ();

	/* Initialize the search provider now that backends are registered */
	seahorse_application_initialize_search (SEAHORSE_APPLICATION (application));
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

	seahorse_register_resource ();

	application = seahorse_application_new ();
	g_signal_connect (application, "activate", G_CALLBACK (on_application_activate), NULL);
	g_signal_connect (application, "startup", G_CALLBACK (on_application_startup), NULL);
	status = g_application_run (G_APPLICATION (application), argc, argv);

	seahorse_registry_cleanup ();
	seahorse_servers_cleanup ();
	g_object_unref (application);

	return status;
}
