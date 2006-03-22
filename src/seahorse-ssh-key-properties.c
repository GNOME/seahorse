/*
 * Seahorse
 *
 * Copyright (C) 2005 Nate Nielsen
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

#include <gnome.h>

#include "seahorse-key-widget.h"
#include "seahorse-util.h"
#include "seahorse-key.h"
#include "seahorse-op.h"
#include "seahorse-ssh-key.h"
#include "seahorse-ssh-operation.h"

#define NOTEBOOK "notebook"

static void
do_main (SeahorseWidget *swidget)
{
    SeahorseKey *key;
    SeahorseSSHKey *skey;
    GtkWidget *widget;
    gchar *text;

    key = SEAHORSE_KEY_WIDGET (swidget)->skey;
    skey = SEAHORSE_SSH_KEY (key);

    widget = glade_xml_get_widget (swidget->xml, "comment-label");
    if (widget) {
        text = seahorse_key_get_display_name (key);
        gtk_label_set_text (GTK_LABEL (widget), text);  
        g_free (text);
    }

    widget = glade_xml_get_widget (swidget->xml, "fingerprint-label");
    if (widget) {
        text = seahorse_key_get_fingerprint (key);  
        gtk_label_set_text (GTK_LABEL (widget), text);
        g_free (text);
    }
}

static void
passphrase_done (SeahorseOperation *op, SeahorseWidget *swidget)
{
    GError *err = NULL;
    GtkWidget *w;

    if (!seahorse_operation_is_successful (op)) {
        seahorse_operation_steal_error (op, &err);
        seahorse_util_handle_error (err, _("Couldn't change passhrase for SSH key."));
    }
    
    w = glade_xml_get_widget (swidget->xml, "passphrase-button");
    gtk_widget_set_sensitive (w, TRUE);
}

static void
passphrase_button_clicked (GtkWidget *widget, SeahorseWidget *swidget)
{
    SeahorseOperation *op;
    SeahorseKey *skey;
    GtkWidget *w;
    
    skey = SEAHORSE_KEY_WIDGET (swidget)->skey;
    g_assert (SEAHORSE_IS_SSH_KEY (skey));

    w = glade_xml_get_widget (swidget->xml, "passphrase-button");
    gtk_widget_set_sensitive (w, FALSE);
    
    op = seahorse_ssh_operation_change_passphrase (SEAHORSE_SSH_KEY (skey));
    if (seahorse_operation_is_done (op))
        passphrase_done (op, swidget);
    else 
        g_signal_connect (op, "done", G_CALLBACK (passphrase_done), swidget);

    /* Running operations ref themselves */
    g_object_unref (op);
}

static void
export_button_clicked (GtkWidget *widget, SeahorseWidget *swidget)
{
    SeahorseKey *skey;
    GtkWidget *dialog;
    gchar* uri = NULL;
    GError *err = NULL;
    GList *keys = NULL;

    skey = SEAHORSE_KEY_WIDGET (swidget)->skey;
    keys = g_list_prepend (keys, skey);

    dialog = seahorse_util_chooser_save_new (_("Export Complete Key"), 
                                             GTK_WINDOW (seahorse_widget_get_top (swidget)));
    seahorse_util_chooser_show_key_files (dialog);
    seahorse_util_chooser_set_filename (dialog, keys);

    uri = seahorse_util_chooser_save_prompt (dialog);
    if(uri) {
        seahorse_op_export_file (keys, TRUE, uri, &err);

        if (err != NULL) {
            seahorse_util_handle_error (err, _("Couldn't export key to \"%s\""),
                                        seahorse_util_uri_get_last (uri));
        }
        g_free (uri);
    }
    g_list_free (keys);
}

static void 
do_details (SeahorseWidget *swidget)
{
    SeahorseKey *key;
    SeahorseSSHKey *skey;
    GtkWidget *widget;
    const gchar *label;
    gchar *text;

    key = SEAHORSE_KEY_WIDGET (swidget)->skey;
    skey = SEAHORSE_SSH_KEY (key);

    widget = glade_xml_get_widget (swidget->xml, "id-label");
    if (widget) {
        label = seahorse_key_get_short_keyid (key); 
        gtk_label_set_text (GTK_LABEL (widget), label);
    }

    widget = glade_xml_get_widget (swidget->xml, "algo-label");
    if (widget) {
        label = seahorse_ssh_key_get_algo (skey);
        gtk_label_set_text (GTK_LABEL (widget), label);
    }

    widget = glade_xml_get_widget (swidget->xml, "location-label");
    if (widget) {
        label = seahorse_ssh_key_get_filename (skey, TRUE);
        gtk_label_set_text (GTK_LABEL (widget), label);  
    }

    widget = glade_xml_get_widget (swidget->xml, "strength-label");
    if (widget) {
        text = g_strdup_printf ("%d", seahorse_ssh_key_get_strength (skey));
        gtk_label_set_text (GTK_LABEL (widget), text);
        g_free (text);
    }

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
seahorse_ssh_key_properties_new (SeahorseSSHKey *skey)
{
    SeahorseKey *key = SEAHORSE_KEY (skey);
    SeahorseWidget *swidget = NULL;
    GtkWidget *widget;

    swidget = seahorse_key_widget_new ("ssh-key-properties", key);    
    
    /* This happens if the window is already open */
    if (swidget == NULL)
        return;

    widget = glade_xml_get_widget (swidget->xml, swidget->name);
    g_signal_connect (widget, "response", G_CALLBACK (properties_response), swidget);
    g_signal_connect (GTK_OBJECT (widget), "destroy", G_CALLBACK (properties_destroyed), swidget);
    g_signal_connect_after (skey, "changed", G_CALLBACK (key_changed), swidget);
    g_signal_connect_after (skey, "destroy", G_CALLBACK (key_destroyed), swidget);

    /* 
     * The signals don't need to keep getting connected. Everytime a key changes the
     * do_* functions get called. Therefore, seperate functions connect the signals
     * have been created
     */

    do_main (swidget);
    do_details (swidget);

    glade_xml_signal_connect_data (swidget->xml, "export_button_clicked",
                                   G_CALLBACK (export_button_clicked), swidget);        
    glade_xml_signal_connect_data (swidget->xml, "passphrase_button_clicked",
                                   G_CALLBACK (passphrase_button_clicked), swidget);        

    if (swidget)
        seahorse_widget_show (swidget);
}
