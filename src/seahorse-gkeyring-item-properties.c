/*
 * Seahorse
 *
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
#include <config.h>
#include <gnome.h>

#include "seahorse-key-widget.h"
#include "seahorse-util.h"
#include "seahorse-key.h"
#include "seahorse-gkeyring-item.h"
#include "seahorse-gkeyring-source.h"
#include "seahorse-gkeyring-operation.h"
#include "seahorse-gtkstock.h"
#include "seahorse-secure-memory.h"
#include "seahorse-secure-entry.h"


static void
load_password (SeahorseWidget *swidget, SeahorseGKeyringItem *git)
{
    SeahorseSecureEntry *entry;
    gchar *secret;
    
    entry = SEAHORSE_SECURE_ENTRY (g_object_get_data (G_OBJECT (swidget), 
                                             "secure-password-entry"));
    if (entry) {
        
        /* Retrieve initial password. Try to keep it safe */
        WITH_SECURE_MEM ((secret = gnome_keyring_item_info_get_secret (git->info)));
        seahorse_secure_entry_set_text (entry, secret ? secret : "");
        g_free (secret);
    
        seahorse_secure_entry_reset_changed (entry);
    }
}

static void
do_main (SeahorseWidget *swidget)
{
    SeahorseKey *key;
    SeahorseGKeyringItem *git;
    GtkWidget *widget;
    gchar *text;
    const gchar *label;
    gboolean network = FALSE;

    key = SEAHORSE_KEY_WIDGET (swidget)->skey;
    git = SEAHORSE_GKEYRING_ITEM (key);

    /* Image */
    widget = seahorse_widget_get_widget (swidget, "key-image");
    if (widget)
        gtk_image_set_from_stock (GTK_IMAGE (widget), seahorse_key_get_stock_id (key), 
                                  GTK_ICON_SIZE_DIALOG);

    /* Description */
    widget = seahorse_widget_get_widget (swidget, "description-field");
    if (widget) {
        text = seahorse_gkeyring_item_get_description (git);
        gtk_entry_set_text (GTK_ENTRY (widget), text ? text : "");
        g_free (text);
    }
    
    /* Window title */
    text = seahorse_key_get_display_name (key);
    widget = seahorse_widget_get_top (swidget);
    gtk_window_set_title (GTK_WINDOW (widget), text);
    g_free (text);

    /* Use and type */
    switch (seahorse_gkeyring_item_get_use (git)) {
    case SEAHORSE_GKEYRING_USE_NETWORK:
        label = _("Access a network share or resource");
        network = TRUE;
        break;
    case SEAHORSE_GKEYRING_USE_WEB:
        label = _("Access a website");
        network = TRUE;
        break;
    case SEAHORSE_GKEYRING_USE_PGP:
        label = _("Unlocks a PGP key");
        break;
    case SEAHORSE_GKEYRING_USE_SSH:
        label = _("Unlocks a Secure Shell key");
        break;
    case SEAHORSE_GKEYRING_USE_OTHER:
        label = _("Saved password or login");
        break;
    }
    
    widget = seahorse_widget_get_widget (swidget, "use-field");
    if (widget)
        gtk_label_set_text (GTK_LABEL (widget), label);
    
    /* The type */
    widget = seahorse_widget_get_widget (swidget, "type-field");
    if (widget)
        gtk_label_set_text (GTK_LABEL (widget), 
                network ? _("Network Credentials") : _("Password"));
    
    /* Network */
    seahorse_widget_set_visible (swidget, "server-label", network);
    seahorse_widget_set_visible (swidget, "server-field", network);
    seahorse_widget_set_visible (swidget, "login-label", network);
    seahorse_widget_set_visible (swidget, "login-field", network);
    
    if (network) {
        label = seahorse_gkeyring_item_get_attribute (git, "server");
        widget = seahorse_widget_get_widget (swidget, "server-field");
        if (widget)
            gtk_label_set_text (GTK_LABEL (widget), label);
        
        label = seahorse_gkeyring_item_get_attribute (git, "user");
        widget = seahorse_widget_get_widget (swidget, "login-field");
        if (widget)
            gtk_label_set_text (GTK_LABEL (widget), label);
    } 
    
    load_password (swidget, git);
}

static void
password_activate (SeahorseSecureEntry *entry, SeahorseWidget *swidget)
{
    SeahorseKey *key;
    SeahorseGKeyringItem *git;
    SeahorseOperation *op;
    GnomeKeyringItemInfo *info;
    GtkWidget *widget;
    GError *err = NULL;
    
    key = SEAHORSE_KEY_WIDGET (swidget)->skey;
    git = SEAHORSE_GKEYRING_ITEM (key);

    widget = seahorse_widget_get_widget (swidget, "password-expander");
    g_return_if_fail (widget);
    if (!gtk_expander_get_expanded (GTK_EXPANDER (widget)))
        return;

    entry = SEAHORSE_SECURE_ENTRY (g_object_get_data (G_OBJECT (swidget), 
                                                "secure-password-entry"));
    if (!entry || !seahorse_secure_entry_get_changed (entry))
        return;
        
    /* Setup for saving */
    WITH_SECURE_MEM (info = gnome_keyring_item_info_copy (git->info));
    WITH_SECURE_MEM (gnome_keyring_item_info_set_secret (info, 
                            seahorse_secure_entry_get_text (entry)));

    gtk_widget_set_sensitive (GTK_WIDGET (entry), FALSE);
    
    op = seahorse_gkeyring_operation_update_info (git, info);
    gnome_keyring_item_info_free (info);
    
    /* This is usually a quick operation */
    seahorse_operation_wait (op);
    
    if (!seahorse_operation_is_successful (op)) {
        seahorse_operation_copy_error (op, &err);
        seahorse_util_handle_error (err, _("Couldn't change password."));
        load_password (swidget, git);
    }
    
    gtk_widget_set_sensitive (GTK_WIDGET (entry), TRUE);
}

static gboolean
password_focus_out (SeahorseSecureEntry* entry, GdkEventFocus *event, SeahorseWidget *swidget)
{
    password_activate (entry, swidget);
    return FALSE;
}

static void 
show_password_toggled (GtkToggleButton *button, SeahorseWidget *swidget)
{
    GtkWidget *widget;
    
    widget = GTK_WIDGET (g_object_get_data (G_OBJECT (swidget), "secure-password-entry"));
    if (!widget)
        return;
    
    seahorse_secure_entry_set_visibility (SEAHORSE_SECURE_ENTRY (widget), 
                                          gtk_toggle_button_get_active (button));
}

static void
password_expander_activate (GtkExpander *expander, SeahorseWidget *swidget)
{
    SeahorseKey *key;
    SeahorseGKeyringItem *git;
    GtkWidget *widget;
    GtkWidget *box;

    key = SEAHORSE_KEY_WIDGET (swidget)->skey;
    git = SEAHORSE_GKEYRING_ITEM (key);

    if (!gtk_expander_get_expanded (expander))
        return;

    /* 
     * TODO: Once gnome-keyring-daemon has support for not reading 
     * the secret, then we'd retrieve the secret when the password 
     * expander is opened 
     */
    
    widget = GTK_WIDGET (g_object_get_data (G_OBJECT (swidget), "secure-password-entry"));
    if (!widget) {
        widget = seahorse_secure_entry_new ();
        
        box = seahorse_widget_get_widget (swidget, "password-box-area");
        g_return_if_fail (box != NULL);
        gtk_container_add (GTK_CONTAINER (box), widget);
        g_object_set_data (G_OBJECT (swidget), "secure-password-entry", widget);
        gtk_widget_show (widget);
        
        /* Retrieve initial password */
        load_password (swidget, git);
        
        /* Now watch for changes in the password */
        g_signal_connect (widget, "activate", G_CALLBACK (password_activate), swidget);
        g_signal_connect (widget, "focus-out-event", G_CALLBACK (password_focus_out), swidget);
    }
    
    /* Always have a hidden password when opening box */
    widget = seahorse_widget_get_widget (swidget, "show-password-check");
    g_return_if_fail (widget != NULL);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), FALSE);
}

static void
description_activate (GtkWidget *entry, SeahorseWidget *swidget)
{
    SeahorseKey *key;
    SeahorseGKeyringItem *git;
    SeahorseOperation *op;
    GnomeKeyringItemInfo *info;
    const gchar *text;
    gchar *original;
    GError *err = NULL;
    
    key = SEAHORSE_KEY_WIDGET (swidget)->skey;
    git = SEAHORSE_GKEYRING_ITEM (key);

    text = gtk_entry_get_text (GTK_ENTRY (entry));
    original = seahorse_gkeyring_item_get_description (git);
    
    /* Make sure not the same */
    if (text == original || g_utf8_collate (text, original ? original : "") == 0) {
        g_free (original);
        return;
    }

    gtk_widget_set_sensitive (entry, FALSE);
    
    WITH_SECURE_MEM (info = gnome_keyring_item_info_copy (git->info));
    gnome_keyring_item_info_set_display_name (info, text);
    
    op = seahorse_gkeyring_operation_update_info (git, info);
    gnome_keyring_item_info_free (info);
    
    /* This is usually a quick operation */
    seahorse_operation_wait (op);
    
    if (!seahorse_operation_is_successful (op)) {
        seahorse_operation_copy_error (op, &err);
        seahorse_util_handle_error (err, _("Couldn't set description."));
        gtk_entry_set_text (GTK_ENTRY (entry), original);
    }
    
    gtk_widget_set_sensitive (entry, TRUE);
    
    g_free (original);
}

static gboolean
description_focus_out (GtkWidget* widget, GdkEventFocus *event, SeahorseWidget *swidget)
{
    description_activate (widget, swidget);
    return FALSE;
}

static void 
do_details (SeahorseWidget *swidget)
{
    SeahorseKey *key;
    SeahorseGKeyringItem *git;
    GString *details;
    GtkWidget *widget;
    guint i;

    key = SEAHORSE_KEY_WIDGET (swidget)->skey;
    git = SEAHORSE_GKEYRING_ITEM (key);

    details = g_string_new (NULL);
    
    g_string_append_printf (details, "<b>identifier</b>: %u\n", git->item_id);
    
    /* Build up the display string */
    if (git->attributes) {
        for(i = 0; i < git->attributes->len; ++i) {
            GnomeKeyringAttribute *attr = &(gnome_keyring_attribute_list_index (git->attributes, i));
            g_string_append_printf (details, "<b>%s</b>: ", attr->name);
            switch (attr->type) {
            case GNOME_KEYRING_ATTRIBUTE_TYPE_STRING:
                g_string_append_printf (details, "%s\n", attr->value.string);
                break;
            case GNOME_KEYRING_ATTRIBUTE_TYPE_UINT32:
                g_string_append_printf (details, "%u\n", attr->value.integer);
                break;
            default:
                g_string_append (details, "<i>[invalid]</i>\n");
                break;
            }
        }
    }
    
    widget = seahorse_widget_get_widget (swidget, "details-box");
    g_return_if_fail (widget != NULL);
    gtk_label_set_markup (GTK_LABEL (widget), details->str);
    
    g_string_free (details, TRUE);
}

static void
key_changed (SeahorseKey *skey, SeahorseKeyChange change, SeahorseWidget *swidget)
{
    do_main (swidget);
    do_details (swidget);
}

static void
key_destroyed (GtkObject *object, SeahorseWidget *swidget)
{
    seahorse_widget_destroy (swidget);
}

static void
properties_destroyed (GtkObject *object, SeahorseWidget *swidget)
{
    g_signal_handlers_disconnect_by_func (SEAHORSE_KEY_WIDGET (swidget)->skey,
                                          key_changed, swidget);
    g_signal_handlers_disconnect_by_func (SEAHORSE_KEY_WIDGET (swidget)->skey,
                                          key_destroyed, swidget);
}

static void
properties_response (GtkDialog *dialog, int response, SeahorseWidget *swidget)
{
    if (response == GTK_RESPONSE_HELP) {
        seahorse_widget_show_help(swidget);
        return;
    }
   
    seahorse_widget_destroy (swidget);
}

void
seahorse_gkeyring_item_properties_new (SeahorseGKeyringItem *git, GtkWindow *parent)
{
    SeahorseKey *key = SEAHORSE_KEY (git);
    SeahorseKeySource *sksrc;
    SeahorseWidget *swidget = NULL;
    GtkWidget *widget;

    swidget = seahorse_key_widget_new ("gkeyring-item-properties",
                                       parent, 
                                       key);    
    
    /* This happens if the window is already open */
    if (swidget == NULL)
        return;

    /* This causes the key source to get any specific info about the key */
    if (seahorse_key_get_loaded (key) < SKEY_INFO_COMPLETE) {
        sksrc = seahorse_key_get_source (key);
        seahorse_key_source_load_async (sksrc, seahorse_key_get_keyid (key));
    }

    widget = glade_xml_get_widget (swidget->xml, swidget->name);
    g_signal_connect (widget, "response", G_CALLBACK (properties_response), swidget);
    g_signal_connect (GTK_OBJECT (widget), "destroy", G_CALLBACK (properties_destroyed), swidget);
    g_signal_connect_after (git, "changed", G_CALLBACK (key_changed), swidget);
    g_signal_connect_after (git, "destroy", G_CALLBACK (key_destroyed), swidget);

    /* 
     * The signals don't need to keep getting connected. Everytime a key changes the
     * do_* functions get called. Therefore, seperate functions connect the signals
     * have been created
     */

    do_main (swidget);
    do_details (swidget);
    
    widget = seahorse_widget_get_widget (swidget, "password-expander");
    g_return_if_fail (widget);
    g_signal_connect_after (widget, "activate", G_CALLBACK (password_expander_activate), swidget);

    glade_xml_signal_connect_data (swidget->xml, "show_password_toggled", 
                                   G_CALLBACK (show_password_toggled), swidget);

    widget = seahorse_widget_get_widget (swidget, "description-field");
    g_return_if_fail (widget != NULL);
    g_signal_connect (widget, "activate", G_CALLBACK (description_activate), swidget);
    g_signal_connect (widget, "focus-out-event", G_CALLBACK (description_focus_out), swidget);

    if (swidget)
        seahorse_widget_show (swidget);
}
