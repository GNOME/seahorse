/*
 * Seahorse
 *
 * Copyright (C) 2004-2006 Nate Nielsen
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

#include <stdlib.h>
#include <libintl.h>
#include <gnome.h>

#include "seahorse-operation.h"
#include "seahorse-progress.h"
#include "seahorse-libdialogs.h"
#include "seahorse-widget.h"
#include "seahorse-validity.h"
#include "seahorse-default-key-control.h"
#include "seahorse-gconf.h"
#include "seahorse-pgp-key.h"
#include "seahorse-util.h"

SeahorsePGPKey*
seahorse_signer_get ()
{
    SeahorseWidget *swidget;
    SeahorseKeyset *skset;
    SeahorseKey *skey = NULL;
    GtkWidget *combo;
    GtkWidget *widget;
    gint response;
    gboolean done = FALSE;
    gboolean ok = FALSE;
    gchar *id;
    guint nkeys;

    skset = seahorse_keyset_pgp_signers_new ();
    nkeys = seahorse_keyset_get_count (skset);
    
    /* If no signing keys then we can't sign */
    if (nkeys == 0) {
        /* TODO: We should be giving an error message that allows them to 
           generate or import a key */
        seahorse_util_show_error (NULL, _("No keys usable for signing"), 
                _("You have no personal PGP keys that can be used to sign a document or message."));
        return NULL;
    }
    
    /* If only one key (probably default) then return it immediately */
    if (nkeys == 1) {
        GList *keys = seahorse_keyset_get_keys (skset);
        skey = SEAHORSE_KEY (keys->data);
        
        g_list_free (keys);
        g_object_unref (skset);

        g_assert (SEAHORSE_IS_PGP_KEY (skey));
        return SEAHORSE_PGP_KEY (skey);
    }
    
    swidget = seahorse_widget_new ("signer");
    g_return_val_if_fail (swidget != NULL, NULL);
            
    combo = glade_xml_get_widget (swidget->xml, "signer-select");
    g_return_val_if_fail (combo != NULL, NULL);
    seahorse_combo_keys_attach (GTK_OPTION_MENU (combo), skset, NULL);
    g_object_unref (skset);
    
    /* Select the last key used */
    id = seahorse_gconf_get_string (SEAHORSE_LASTSIGNER_KEY);
    seahorse_combo_keys_set_active_id (GTK_OPTION_MENU (combo), id);
    g_free (id); 
    
    widget = seahorse_widget_get_top (swidget);
    seahorse_widget_show (swidget);
    
    while (!done) {
        response = gtk_dialog_run (GTK_DIALOG (widget));
        switch (response) {
            case GTK_RESPONSE_HELP:
                break;
            case GTK_RESPONSE_OK:
                ok = TRUE;
            default:
                done = TRUE;
                break;
        }
    }

    if (ok) {
        skey = seahorse_combo_keys_get_active (GTK_OPTION_MENU (combo));
        g_return_val_if_fail (SEAHORSE_IS_PGP_KEY (skey), NULL);

        /* Save this as the last key signed with */
        seahorse_gconf_set_string (SEAHORSE_LASTSIGNER_KEY, skey == NULL ? 
                        "" : seahorse_key_get_keyid (skey));
    }
    
    seahorse_widget_destroy (swidget);
    return SEAHORSE_PGP_KEY (skey);
}
