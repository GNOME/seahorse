/*
 * Seahorse
 *
 * Copyright (C) 2005 Stefan Walter
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

#include "seahorse-gtkstock.h"
#include "seahorse-object.h"
#include "seahorse-object-widget.h"
#include "seahorse-util.h"
#include "seahorse-validity.h"

#include "common/seahorse-bind.h"

#include "ssh/seahorse-ssh-key.h"
#include "ssh/seahorse-ssh-operation.h"

#include <glib/gi18n.h>

#define NOTEBOOK "notebook"

static void
do_main (SeahorseWidget *swidget)
{
    SeahorseObject *object;
    SeahorseSSHKey *skey;
    GtkWidget *widget;
    gchar *text;
    const gchar *label;
    const gchar *template;

    object = SEAHORSE_OBJECT_WIDGET (swidget)->object;
    skey = SEAHORSE_SSH_KEY (object);

    /* Image */
    widget = seahorse_widget_get_widget (swidget, "key-image");
    if (widget)
        gtk_image_set_from_stock (GTK_IMAGE (widget), SEAHORSE_STOCK_KEY_SSH, GTK_ICON_SIZE_DIALOG);

    /* Name and title */
    label = seahorse_object_get_label (object);
    widget = seahorse_widget_get_widget (swidget, "comment-entry");
    if (widget)
        gtk_entry_set_text (GTK_ENTRY (widget), label);
    widget = seahorse_widget_get_toplevel (swidget);
    gtk_window_set_title (GTK_WINDOW (widget), label);

    /* Key id */
    widget = glade_xml_get_widget (swidget->xml, "id-label");
    if (widget) {
        label = seahorse_object_get_identifier (object);
        gtk_label_set_text (GTK_LABEL (widget), label);
    }
    
    /* Put in message */
    widget = seahorse_widget_get_widget (swidget, "trust-message");
    g_return_if_fail (widget != NULL);
    template = gtk_label_get_label (GTK_LABEL (widget));
    text = g_strdup_printf (template, g_get_user_name ());
    gtk_label_set_markup (GTK_LABEL (widget), text);
    g_free (text);
    
    /* Setup the check */
    widget = seahorse_widget_get_widget (swidget, "trust-check");
    g_return_if_fail (widget != NULL);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), 
                                  seahorse_ssh_key_get_trust (skey) >= SEAHORSE_VALIDITY_FULL);
}

static void
comment_activate (GtkWidget *entry, SeahorseWidget *swidget)
{
    SeahorseObject *object;
    SeahorseSSHKey *skey;
    SeahorseSource *sksrc;
    SeahorseOperation *op;
    const gchar *text;
    gchar *comment;
    GError *err = NULL;
    
    object = SEAHORSE_OBJECT_WIDGET (swidget)->object;
    skey = SEAHORSE_SSH_KEY (object);
    sksrc = seahorse_object_get_source (object);
    g_return_if_fail (SEAHORSE_IS_SSH_SOURCE (sksrc));
    
    text = gtk_entry_get_text (GTK_ENTRY (entry));
    
    /* Make sure not the same */
    if (skey->keydata->comment && g_utf8_collate (text, skey->keydata->comment) == 0)
        return;

    gtk_widget_set_sensitive (entry, FALSE);
    
    comment = g_strdup (text);
    op = seahorse_ssh_operation_rename (SEAHORSE_SSH_SOURCE (sksrc), skey, comment);
    g_free (comment);
    
    /* This is usually a quick operation */
    seahorse_operation_wait (op);
    
    if (!seahorse_operation_is_successful (op)) {
        seahorse_operation_copy_error (op, &err);
        seahorse_util_handle_error (err, _("Couldn't rename key."));
        g_clear_error (&err);
        gtk_entry_set_text (GTK_ENTRY (entry), skey->keydata->comment ? skey->keydata->comment : "");
    }
    
    gtk_widget_set_sensitive (entry, TRUE);
}

static gboolean
comment_focus_out (GtkWidget* widget, GdkEventFocus *event, SeahorseWidget *swidget)
{
    comment_activate (widget, swidget);
    return FALSE;
}

static void 
trust_toggled (GtkToggleButton *button, SeahorseWidget *swidget)
{
    SeahorseSource *sksrc;
    SeahorseOperation *op;
    SeahorseObject *object;
    SeahorseSSHKey *skey;
    gboolean authorize;
    GError *err = NULL;

    object = SEAHORSE_OBJECT_WIDGET (swidget)->object;
    skey = SEAHORSE_SSH_KEY (object);
    sksrc = seahorse_object_get_source (object);
    g_return_if_fail (SEAHORSE_IS_SSH_SOURCE (sksrc));
    
    authorize = gtk_toggle_button_get_active (button);
    gtk_widget_set_sensitive (GTK_WIDGET (button), FALSE);
    
    op = seahorse_ssh_operation_authorize (SEAHORSE_SSH_SOURCE (sksrc), skey, authorize);
    g_return_if_fail (op);
    
    /* A very fast op, so just wait */
    seahorse_operation_wait (op);
    
    if (!seahorse_operation_is_successful (op)) {
        seahorse_operation_copy_error (op, &err);
        seahorse_util_handle_error (err, _("Couldn't change authorization for key."));
        g_clear_error (&err);
    }
    
    gtk_widget_set_sensitive (GTK_WIDGET (button), TRUE);
}

static void
passphrase_done (SeahorseOperation *op, SeahorseWidget *swidget)
{
    GError *err = NULL;
    GtkWidget *w;

    if (!seahorse_operation_is_successful (op)) {
        seahorse_operation_copy_error (op, &err);
        seahorse_util_handle_error (err, _("Couldn't change passphrase for key."));
        g_clear_error (&err);
    }
    
    w = glade_xml_get_widget (swidget->xml, "passphrase-button");
    g_return_if_fail (w != NULL);
    gtk_widget_set_sensitive (w, TRUE);
}

static void
passphrase_button_clicked (GtkWidget *widget, SeahorseWidget *swidget)
{
    SeahorseOperation *op;
    SeahorseObject *object;
    GtkWidget *w;
    
    object = SEAHORSE_OBJECT_WIDGET (swidget)->object;
    g_assert (SEAHORSE_IS_SSH_KEY (object));

    w = glade_xml_get_widget (swidget->xml, "passphrase-button");
    g_return_if_fail (w != NULL);
    gtk_widget_set_sensitive (w, FALSE);
    
    op = seahorse_ssh_operation_change_passphrase (SEAHORSE_SSH_KEY (object));
    seahorse_operation_watch (op, (SeahorseDoneFunc)passphrase_done, swidget, NULL, NULL);

    /* Running operations ref themselves */
    g_object_unref (op);
}

static void
export_complete (GFile *file, GAsyncResult *result, guchar *contents)
{
	GError *err = NULL;
	gchar *uri;
	
	g_free (contents);
	
	if (!g_file_replace_contents_finish (file, result, NULL, &err)) {
		uri = g_file_get_uri (file);
	        seahorse_util_handle_error (err, _("Couldn't export key to \"%s\""),
	                                    seahorse_util_uri_get_last (uri));
	        g_clear_error (&err);
	        g_free (uri);
	}
}

static void
export_button_clicked (GtkWidget *widget, SeahorseWidget *swidget)
{
	SeahorseSource *sksrc;
	SeahorseObject *object;
	GFile *file;
	GtkDialog *dialog;
	guchar *results;
	gsize n_results;
	gchar* uri = NULL;
	GError *err = NULL;

	object = SEAHORSE_OBJECT_WIDGET (swidget)->object;
	g_return_if_fail (SEAHORSE_IS_SSH_KEY (object));
	sksrc = seahorse_object_get_source (object);
	g_return_if_fail (SEAHORSE_IS_SSH_SOURCE (sksrc));
	
	dialog = seahorse_util_chooser_save_new (_("Export Complete Key"), 
	                                         GTK_WINDOW (seahorse_widget_get_toplevel (swidget)));
	seahorse_util_chooser_show_key_files (dialog);
	seahorse_util_chooser_set_filename (dialog, object);

	uri = seahorse_util_chooser_save_prompt (dialog);
	if (!uri) 
		return;
	
	results = seahorse_ssh_source_export_private (SEAHORSE_SSH_SOURCE (sksrc), 
	                                              SEAHORSE_SSH_KEY (object),
	                                              &n_results, &err);
	
	if (results) {
		g_return_if_fail (err == NULL);
		file = g_file_new_for_uri (uri);
		g_file_replace_contents_async (file, (gchar*)results, n_results, NULL, FALSE, 
		                               G_FILE_CREATE_PRIVATE, NULL, 
		                               (GAsyncReadyCallback)export_complete, results);
	}
	
	if (err) {
	        seahorse_util_handle_error (err, _("Couldn't export key."));
	        g_clear_error (&err);
	}
	
	g_free (uri);
}

static void 
do_details (SeahorseWidget *swidget)
{
    SeahorseObject *object;
    SeahorseSSHKey *skey;
    GtkWidget *widget;
    const gchar *label;
    gchar *text;

    object = SEAHORSE_OBJECT_WIDGET (swidget)->object;
    skey = SEAHORSE_SSH_KEY (object);

    widget = glade_xml_get_widget (swidget->xml, "fingerprint-label");
    if (widget) {
        text = seahorse_ssh_key_get_fingerprint (skey);
        gtk_label_set_text (GTK_LABEL (widget), text);
        g_free (text);
    }

    widget = glade_xml_get_widget (swidget->xml, "algo-label");
    if (widget) {
        label = seahorse_ssh_key_get_algo_str (skey);
        gtk_label_set_text (GTK_LABEL (widget), label);
    }

    widget = glade_xml_get_widget (swidget->xml, "location-label");
    if (widget) {
        label = seahorse_ssh_key_get_location (skey);
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
key_notify (SeahorseObject *object, SeahorseWidget *swidget)
{
    do_main (swidget);
    do_details (swidget);
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
seahorse_ssh_key_properties_show (SeahorseSSHKey *skey, GtkWindow *parent)
{
    SeahorseObject *object = SEAHORSE_OBJECT (skey);
    SeahorseWidget *swidget = NULL;
    GtkWidget *widget;

    swidget = seahorse_object_widget_new ("ssh-key-properties", parent, object);
    
    /* This happens if the window is already open */
    if (swidget == NULL)
        return;

    /* 
     * The signals don't need to keep getting connected. Everytime a key changes the
     * do_* functions get called. Therefore, seperate functions connect the signals
     * have been created
     */

    do_main (swidget);
    do_details (swidget);
    
    glade_xml_signal_connect_data (swidget->xml, "trust_toggled", 
                                   G_CALLBACK (trust_toggled), swidget);
    
    widget = seahorse_widget_get_widget (swidget, "comment-entry");
    g_return_if_fail (widget != NULL);
    g_signal_connect (widget, "activate", G_CALLBACK (comment_activate), swidget);
    g_signal_connect (widget, "focus-out-event", G_CALLBACK (comment_focus_out), swidget);

    if (seahorse_object_get_usage (object) == SEAHORSE_USAGE_PRIVATE_KEY) {
        glade_xml_signal_connect_data (swidget->xml, "export_button_clicked",
                                       G_CALLBACK (export_button_clicked), swidget);
        glade_xml_signal_connect_data (swidget->xml, "passphrase_button_clicked",
                                       G_CALLBACK (passphrase_button_clicked), swidget);
        
    /* A public key only */
    } else {
        seahorse_widget_set_visible (swidget, "passphrase-button", FALSE);
        seahorse_widget_set_visible (swidget, "export-button", FALSE);
    }

    widget = glade_xml_get_widget (swidget->xml, swidget->name);
    g_signal_connect (widget, "response", G_CALLBACK (properties_response), swidget);
    seahorse_bind_objects (NULL, skey, (SeahorseTransfer)key_notify, swidget);

    seahorse_widget_show (swidget);
}
