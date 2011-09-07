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
 * along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include "seahorse-cleanup.h"
#include "seahorse-context.h"
#include "seahorse-icons.h"
#include "seahorse-registry.h"
#include "seahorse-secure-memory.h"
#include "seahorse-util.h"

#include "seahorse-key-manager.h"

#include "gkr/seahorse-gkr.h"
#include "pgp/seahorse-pgp.h"
#include "ssh/seahorse-ssh.h"
#include "pkcs11/seahorse-pkcs11.h"

#include <locale.h>
#include <stdlib.h>
  
#include <glib/gi18n.h>

static gboolean show_version = FALSE;

/* Initializes context and preferences, then loads key manager */
int
main (int argc, char **argv)
{
	SeahorseWidget *swidget;
	GcrUnionCollection *sources;
	SeahorseContext *context;

    static GOptionEntry options[] = {
        { "version", 'v', 0, G_OPTION_ARG_NONE, &show_version, N_("Version of this application"), NULL },
        { NULL, 0, 0, 0, NULL, NULL, NULL }
    };
    GError *error = NULL;
    int ret = 0;

    g_thread_init (NULL);

    seahorse_secure_memory_init ();
    
#ifdef ENABLE_NLS
    bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
    textdomain (GETTEXT_PACKAGE);
#endif

    if (!gtk_init_with_args (&argc, &argv, _("Passwords and Keys"), options, GETTEXT_PACKAGE, &error)) {
        g_printerr ("seahorse: %s\n", error->message);
        g_error_free (error);
        exit (1);
    }

    if (show_version) {
        g_print ("%s\n", PACKAGE_STRING);
        g_print ("GNUPG: %s (%d.%d.%d)\n", GNUPG, GPG_MAJOR, GPG_MINOR, GPG_MICRO);
        exit (1);
    }

    /* Insert Icons into Stock */ 
    seahorse_icons_init ();

    /* Make the default SeahorseContext */
    seahorse_context_create ();
    context = seahorse_context_instance ();

    sources = GCR_UNION_COLLECTION (gcr_union_collection_new ());

    /* Initialize the various components */
    gcr_union_collection_take (sources, seahorse_pgp_backend_initialize ());
    gcr_union_collection_take (sources, seahorse_ssh_backend_initialize ());
    gcr_union_collection_take (sources, seahorse_pkcs11_backend_initialize ());
    gcr_union_collection_take (sources, seahorse_gkr_backend_initialize ());

    swidget = seahorse_key_manager_show (GCR_COLLECTION (sources));
    g_object_unref (sources);

    g_object_ref (context);
    g_signal_connect_after (context, "destroy", gtk_main_quit, NULL);

    gtk_main ();

    g_object_unref (swidget);
    g_object_unref (context);

    seahorse_cleanup_perform ();
    return ret;
}
