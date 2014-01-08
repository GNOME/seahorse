/*
 * Seahorse
 *
 * Copyright (C) 2004-2006 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the licence or (at
 * your option) any later version.
 *
 * See the included COPYING file for more information.
 */

#include <config.h>

#include <stdlib.h>

#include <glib/gi18n.h>

#include "seahorse-pgp-dialogs.h"
#include "seahorse-progress.h"
#include "seahorse-widget.h"
#include "seahorse-validity.h"
#include "seahorse-combo-keys.h"
#include "seahorse-util.h"

#include "pgp/seahorse-pgp-key.h"
#include "pgp/seahorse-pgp-keysets.h"

SeahorsePgpKey*
seahorse_signer_get (GtkWindow *parent)
{
    SeahorseWidget *swidget;
    GcrCollection *collection;
    SeahorsePgpKey *key = NULL;
    GtkWidget *combo;
    GtkWidget *widget;
    gint response;
    gboolean done = FALSE;
    gboolean ok = FALSE;
    GSettings *settings;
    const gchar *keyid;
    gchar *id;
    guint nkeys;

    collection = seahorse_keyset_pgp_signers_new ();
    nkeys = gcr_collection_get_length (collection);

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
        GList *keys = gcr_collection_get_objects (collection);
        key = keys->data;
        g_list_free (keys);
        g_object_unref (collection);
        return key;
    }
    
    swidget = seahorse_widget_new ("signer", parent);
    g_return_val_if_fail (swidget != NULL, NULL);
            
    combo = GTK_WIDGET (seahorse_widget_get_widget (swidget, "signer-select"));
    g_return_val_if_fail (combo != NULL, NULL);
    seahorse_combo_keys_attach (GTK_COMBO_BOX (combo), collection, NULL);
    g_object_unref (collection);

    settings = seahorse_application_pgp_settings (NULL);

    /* Select the last key used */
    id = g_settings_get_string (settings, "last-signer");
    if (!id || !id[0])
        keyid = NULL;
    else if (g_str_has_prefix (id, "openpgp:"))
        keyid = id + strlen ("openpgp:");
    else
        keyid = id;
    seahorse_combo_keys_set_active_id (GTK_COMBO_BOX (combo), keyid);
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
        key = seahorse_combo_keys_get_active (GTK_COMBO_BOX (combo));

        /* Save this as the last key signed with */
        g_settings_set_string (settings, "last-signer",
                               key ? seahorse_pgp_key_get_keyid (key) : NULL);
    }
    
    seahorse_widget_destroy (swidget);
    return key;
}
