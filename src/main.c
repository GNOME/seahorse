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

#include "seahorse-context.h"
#include "seahorse-util.h"
#include "seahorse-libdialogs.h"
#include "seahorse-gtkstock.h"
#include "seahorse-secure-memory.h"

#include "seahorse-key-manager.h"

#include "common/seahorse-cleanup.h"
#include "common/seahorse-registry.h"

#ifdef WITH_PGP
#include "pgp/seahorse-pgp-module.h"
#endif

#ifdef WITH_SSH
#include "ssh/seahorse-ssh-module.h"
#endif

#include "gkr/seahorse-gkr-module.h"

#include <locale.h>
#include <stdlib.h>
  
#include <glib/gi18n.h>

/* Initializes context and preferences, then loads key manager */
int
main (int argc, char **argv)
{
    SeahorseOperation *op = NULL;
    int ret = 0;

    seahorse_secure_memory_init ();
    
#ifdef ENABLE_NLS
    bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
    textdomain (GETTEXT_PACKAGE);
#endif

    gtk_init_with_args (&argc, &argv, _("Encryption Key Manager"), NULL, GETTEXT_PACKAGE, NULL);

    /* Insert Icons into Stock */ 
    seahorse_gtkstock_init ();
    
    /* Initialize the various components */
#ifdef WITH_PGP
    seahorse_registry_load_types (NULL, SEAHORSE_PGP_REGISTRY);
#endif
#ifdef WITH_SSH
    seahorse_registry_load_types (NULL, SEAHORSE_SSH_REGISTRY);
#endif
    
    /* Make the default SeahorseContext */
    seahorse_context_new (SEAHORSE_CONTEXT_APP, 0);
    op = seahorse_context_load_local_objects (SCTX_APP ());

    /* Load these components after loading local keys */
    seahorse_registry_load_types (NULL, SEAHORSE_GKR_REGISTRY);

    seahorse_key_manager_show (op);
    g_signal_connect_after (SCTX_APP (), "destroy", gtk_main_quit, NULL);
    
    gtk_main ();


    seahorse_cleanup_perform ();
    return ret;
}
