/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
 * Copyright (C) 2006 Stefan Walter
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

#include <glib/gi18n.h>

#include "seahorse-windows.h"
#include "seahorse-key-widget.h"
#include "seahorse-util.h"
#include "seahorse-keyset.h"
#include "seahorse-gtkstock.h"
#include "seahorse-combo-keys.h"
#include "seahorse-gconf.h"

#include "pgp/seahorse-pgp-dialogs.h"
#include "pgp/seahorse-pgp-key-op.h"
#include "pgp/seahorse-pgp-keysets.h"

#ifdef WITH_KEYSERVER
#include "seahorse-keyserver-sync.h"
#endif

static gboolean
ok_clicked (SeahorseWidget *swidget)
{
    SeahorseKeyWidget *skwidget;
    SeahorseSignCheck check;
    SeahorseSignOptions options = 0;
    SeahorseKey *signer;
    GtkWidget *w;
    gpgme_error_t err;
    SeahorseKey *skey;
    GList *keys;
    
    skwidget = SEAHORSE_KEY_WIDGET (swidget);
    skey = skwidget->skey;
    
    /* Figure out choice */
    check = SIGN_CHECK_NO_ANSWER;
    w = glade_xml_get_widget (swidget->xml, "sign-choice-not");
    g_return_val_if_fail (w != NULL, FALSE);
    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (w)))
        check = SIGN_CHECK_NONE;
    else {
        w = glade_xml_get_widget (swidget->xml, "sign-choice-casual");
        g_return_val_if_fail (w != NULL, FALSE);
        if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (w)))
            check = SIGN_CHECK_CASUAL;
        else {
            w = glade_xml_get_widget (swidget->xml, "sign-choice-careful");
            g_return_val_if_fail (w != NULL, FALSE);
            if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (w)))
                check = SIGN_CHECK_CAREFUL;
        }
    }
    
    /* Local signature */
    w = glade_xml_get_widget (swidget->xml, "sign-option-local");
    g_return_val_if_fail (w != NULL, FALSE);
    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (w)))
        options |= SIGN_LOCAL;
    
    /* Revocable signature */
    w = glade_xml_get_widget (swidget->xml, "sign-option-revocable");
    g_return_val_if_fail (w != NULL, FALSE);
    if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (w))) 
        options |= SIGN_NO_REVOKE;
    
    /* Signer */
    w = glade_xml_get_widget (swidget->xml, "signer-select");
    g_return_val_if_fail (w != NULL, FALSE);
    signer = seahorse_combo_keys_get_active (GTK_COMBO_BOX (w));
    
    g_assert (!signer || (SEAHORSE_IS_PGP_KEY (signer) && 
                          seahorse_key_get_etype (signer) == SKEY_PRIVATE));
    
    err = seahorse_pgp_key_op_sign (SEAHORSE_PGP_KEY (skey), 
                                    SEAHORSE_PGP_KEY (signer), 
                                    skwidget->index + 1, check, options);
    if (!GPG_IS_OK (err))
        seahorse_pgp_handle_gpgme_error (err, _("Couldn't sign key"));
    
    seahorse_widget_destroy (swidget);
    
#ifdef WITH_KEYSERVER
    if (GPG_IS_OK (err) && seahorse_gconf_get_boolean (AUTOSYNC_KEY)) {
        keys = g_list_append (NULL, skey);
        seahorse_keyserver_sync (keys);
        g_list_free (keys);
    }
#endif
    
    return TRUE;
}

static void
keyset_changed (SeahorseKeyset *skset, GtkWidget *widget)
{
    if (seahorse_keyset_get_count (skset) <= 1)
        gtk_widget_hide (widget);
    else
        gtk_widget_show (widget);
}

static void
choice_toggled (GtkToggleButton *toggle, SeahorseWidget *swidget)
{
    GtkWidget *w;
    
    /* Figure out choice */
    w = glade_xml_get_widget (swidget->xml, "sign-choice-not");
    g_return_if_fail (w != NULL);
    seahorse_widget_set_visible (swidget, "sign-display-not", 
                             gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (w)));
    w = glade_xml_get_widget (swidget->xml, "sign-choice-casual");
    g_return_if_fail (w != NULL);
    seahorse_widget_set_visible (swidget, "sign-display-casual", 
                             gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (w)));
    w = glade_xml_get_widget (swidget->xml, "sign-choice-careful");
    g_return_if_fail (w != NULL);
    seahorse_widget_set_visible (swidget, "sign-display-careful", 
                             gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (w)));
}

void
seahorse_sign_show (SeahorsePGPKey *pkey, GtkWindow *parent, guint uid)
{
    SeahorseKeyset *skset;
    GtkWidget *w;
    gint response;
    SeahorseWidget *swidget;
    gboolean do_sign = TRUE;
    gchar *t;
    gchar *userid;

    /* Some initial checks */
    skset = seahorse_keyset_pgp_signers_new ();
    
    /* If no signing keys then we can't sign */
    if (seahorse_keyset_get_count (skset) == 0) {
        /* TODO: We should be giving an error message that allows them to 
           generate or import a key */
        seahorse_util_show_error (NULL, _("No keys usable for signing"), 
                _("You have no personal PGP keys that can be used to indicate your trust of this key."));
        return;
    }

    swidget = seahorse_key_widget_new_with_index ("sign", parent, SEAHORSE_KEY (pkey), uid);
    g_return_if_fail (swidget != NULL);

    /* ... Except for when calling this, which is messed up */
    w = glade_xml_get_widget (swidget->xml, "sign-uid-text");
    g_return_if_fail (w != NULL);
    t = seahorse_key_get_name (SEAHORSE_KEY (pkey), uid);
    userid = g_markup_escape_text (t, -1);
    g_free (t);
    t = g_strdup_printf ("<i>%s</i>", userid);
    g_free (userid);
    gtk_label_set_markup (GTK_LABEL (w), t);
    g_free (t);
    
    /* Uncheck all selections */
    w = glade_xml_get_widget (swidget->xml, "sign-choice-not");
    g_return_if_fail (w != NULL);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), FALSE);
    g_signal_connect (w, "toggled", G_CALLBACK (choice_toggled), swidget);
    w = glade_xml_get_widget (swidget->xml, "sign-choice-casual");
    g_return_if_fail (w != NULL);
    g_signal_connect (w, "toggled", G_CALLBACK (choice_toggled), swidget);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), FALSE);
    w = glade_xml_get_widget (swidget->xml, "sign-choice-careful");
    g_return_if_fail (w != NULL);
    g_signal_connect (w, "toggled", G_CALLBACK (choice_toggled), swidget);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), FALSE);
    
    /* Initial choice */
    choice_toggled(NULL, swidget);
    
    /* Other question's default state */
    w = glade_xml_get_widget (swidget->xml, "sign-option-local");
    g_return_if_fail (w != NULL);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), FALSE);
    w = glade_xml_get_widget (swidget->xml, "sign-option-revocable");
    g_return_if_fail (w != NULL);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), TRUE);
    
    /* Signature area */
    w = glade_xml_get_widget (swidget->xml, "signer-frame");
    g_return_if_fail (w != NULL);
    g_signal_connect (skset, "set-changed", G_CALLBACK (keyset_changed), w);
    keyset_changed (skset, w);

    /* Signer box */
    w = glade_xml_get_widget (swidget->xml, "signer-select");
    g_return_if_fail (w != NULL);
    seahorse_combo_keys_attach (GTK_COMBO_BOX (w), skset, NULL);

    /* Image */
    w = glade_xml_get_widget (swidget->xml, "sign-image");
    g_return_if_fail (w != NULL);
    gtk_image_set_from_stock (GTK_IMAGE (w), SEAHORSE_STOCK_SIGN, GTK_ICON_SIZE_DIALOG);
    
    g_object_unref (skset);
    seahorse_widget_show (swidget);
    
    while (do_sign) {
        response = gtk_dialog_run (GTK_DIALOG (seahorse_widget_get_top (swidget)));
        switch (response) {
        case GTK_RESPONSE_HELP:
            break;
        case GTK_RESPONSE_OK:
            do_sign = !ok_clicked (swidget);
            break;
        default:
            do_sign = FALSE;
            seahorse_widget_destroy (swidget);
            break;
        }
    }
}
