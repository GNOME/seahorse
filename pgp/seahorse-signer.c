/*
 * Seahorse
 *
 * Copyright (C) 2004-2006 Stefan Walter
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

#include <stdlib.h>

#include <glib/gi18n.h>

#include "seahorse-operation.h"
#include "seahorse-progress.h"
#include "seahorse-libdialogs.h"
#include "seahorse-widget.h"
#include "seahorse-validity.h"
#include "seahorse-combo-keys.h"
#include "seahorse-gconf.h"
#include "seahorse-util.h"

#include "pgp/seahorse-pgp-key.h"
#include "pgp/seahorse-pgp-keysets.h"

SeahorsePGPKey*
seahorse_signer_get (GtkWindow *parent)
{
    SeahorseWidget *swidget;
    SeahorseSet *skset;
    SeahorseObject *object = NULL;
    GtkWidget *combo;
    GtkWidget *widget;
    gint response;
    gboolean done = FALSE;
    gboolean ok = FALSE;
    gchar *id;
    guint nkeys;

    skset = seahorse_keyset_pgp_signers_new ();
    nkeys = seahorse_set_get_count (skset);
    
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
        GList *keys = seahorse_set_get_objects (skset);
        object = SEAHORSE_OBJECT (keys->data);
        
        g_list_free (keys);
        g_object_unref (skset);

        g_assert (SEAHORSE_IS_PGP_KEY (object));
        return SEAHORSE_PGP_KEY (object);
    }
    
    swidget = seahorse_widget_new ("signer", parent);
    g_return_val_if_fail (swidget != NULL, NULL);
            
    combo = glade_xml_get_widget (swidget->xml, "signer-select");
    g_return_val_if_fail (combo != NULL, NULL);
    seahorse_combo_keys_attach (GTK_COMBO_BOX (combo), skset, NULL);
    g_object_unref (skset);
    
    /* Select the last key used */
    id = seahorse_gconf_get_string (SEAHORSE_LASTSIGNER_KEY);
    seahorse_combo_keys_set_active_id (GTK_COMBO_BOX (combo), g_quark_from_string (id));
    g_free (id); 
    
    widget = seahorse_widget_get_toplevel (swidget);
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
        object = seahorse_combo_keys_get_active (GTK_COMBO_BOX (combo));
        g_return_val_if_fail (SEAHORSE_IS_PGP_KEY (object), NULL);

        /* Save this as the last key signed with */
        seahorse_gconf_set_string (SEAHORSE_LASTSIGNER_KEY, object == NULL ? 
                        "" : g_quark_to_string (seahorse_object_get_id (object)));
    }
    
    seahorse_widget_destroy (swidget);
    return SEAHORSE_PGP_KEY (object);
}
