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

#include <config.h>
#include <gnome.h>
#include <libgnomevfs/gnome-vfs.h>
#include <locale.h>

#include "seahorse-context.h"
#include "seahorse-windows.h"
#include "seahorse-util.h"
#include "seahorse-libdialogs.h"
#include "seahorse-gtkstock.h"
#include "seahorse-secure-memory.h"

#include "pgp/seahorse-pgp-key.h"
#include "pgp/seahorse-pgp-source.h"

/* Initializes context and preferences, then loads key manager */
int
main (int argc, char **argv)
{
    SeahorseOperation *op = NULL;
    GtkWindow* win;
    int ret = 0;

    seahorse_secure_memory_init ();
    
#ifdef ENABLE_NLS
    bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
    textdomain (GETTEXT_PACKAGE);
#endif

    g_message("init gpgme version %s", gpgme_check_version(NULL));

#ifdef ENABLE_NLS
    gpgme_set_locale(NULL, LC_CTYPE, setlocale(LC_CTYPE, NULL));
    gpgme_set_locale(NULL, LC_MESSAGES, setlocale(LC_MESSAGES, NULL));
#endif

    gnome_program_init(PACKAGE, VERSION, LIBGNOMEUI_MODULE, argc, argv,
                       GNOME_PARAM_HUMAN_READABLE_NAME, _("Encryption Key Manager"),
                       GNOME_PARAM_APP_DATADIR, DATA_DIR, NULL);

    /* Insert Icons into Stock */ 
    seahorse_gtkstock_init();
    
    /* Make the default SeahorseContext */
    seahorse_context_new (SEAHORSE_CONTEXT_APP, 0);
    op = seahorse_context_load_local_keys (SCTX_APP());

    win = seahorse_key_manager_show (op);
    g_signal_connect_after (G_OBJECT (win), "destroy", gtk_main_quit, NULL);
    
    gtk_main ();
    
    if (gnome_vfs_initialized ())
        gnome_vfs_shutdown ();
    
    return ret;
}
