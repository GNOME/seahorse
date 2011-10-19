/*
 * Seahorse
 *
 * Copyright (C) 2005 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
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

#include "seahorse-bind.h"
#include "seahorse-icons.h"
#include "seahorse-object.h"
#include "seahorse-object-widget.h"
#include "seahorse-util.h"
#include "seahorse-validity.h"

#include "ssh/seahorse-ssh-dialogs.h"
#include "ssh/seahorse-ssh-key.h"
#include "ssh/seahorse-ssh-operation.h"

#include <glib/gi18n.h>

#define NOTEBOOK "notebook"

void            on_ssh_comment_activate            (GtkWidget *entry,
                                                    SeahorseWidget *swidget);

gboolean        on_ssh_comment_focus_out           (GtkWidget* widget,
                                                    GdkEventFocus *event,
                                                    SeahorseWidget *swidget);

void            on_ssh_trust_toggled               (GtkToggleButton *button,
                                                    SeahorseWidget *swidget);

void            on_ssh_passphrase_button_clicked   (GtkWidget *widget,
                                                    SeahorseWidget *swidget);

void            on_ssh_export_button_clicked       (GtkWidget *widget,
                                                    SeahorseWidget *swidget);

typedef struct {
	SeahorseWidget *swidget;
	GtkEntry *entry;
	gchar *original;
} ssh_rename_closure;

static void
ssh_rename_free (gpointer data)
{
	ssh_rename_closure *closure = data;
	g_object_unref (closure->swidget);
	g_free (closure->original);
	g_free (closure);
}

static void
on_rename_complete (GObject *source,
                    GAsyncResult *result,
                    gpointer user_data)
{
	ssh_rename_closure *closure = user_data;
	GError *error = NULL;

	if (!seahorse_ssh_op_rename_finish (SEAHORSE_SSH_SOURCE (source), result, &error)) {
		seahorse_util_handle_error (&error, closure->swidget, _("Couldn't rename key."));
		gtk_entry_set_text (closure->entry, closure->original);
	}

	gtk_widget_set_sensitive (GTK_WIDGET (closure->entry), TRUE);
	ssh_rename_free (closure);
}

G_MODULE_EXPORT void
on_ssh_comment_activate (GtkWidget *entry,
                         SeahorseWidget *swidget)
{
	SeahorseSSHKey *skey;
	SeahorseSSHSource *source;
	const gchar *text;
	ssh_rename_closure *closure;

	skey = SEAHORSE_SSH_KEY (SEAHORSE_OBJECT_WIDGET (swidget)->object);
	source = SEAHORSE_SSH_SOURCE (seahorse_object_get_place (SEAHORSE_OBJECT (skey)));

	text = gtk_entry_get_text (GTK_ENTRY (entry));

	/* Make sure not the same */
	if (skey->keydata->comment && g_utf8_collate (text, skey->keydata->comment) == 0)
		return;

	gtk_widget_set_sensitive (entry, FALSE);

	closure = g_new0 (ssh_rename_closure, 1);
	closure->swidget = g_object_ref (swidget);
	closure->entry = GTK_ENTRY (entry);
	closure->original = g_strdup (skey->keydata->comment ? skey->keydata->comment : "");
	seahorse_ssh_op_rename_async (source, skey, text,
	                              NULL, on_rename_complete, closure);
}

G_MODULE_EXPORT gboolean
on_ssh_comment_focus_out (GtkWidget* widget, GdkEventFocus *event, SeahorseWidget *swidget)
{
    on_ssh_comment_activate (widget, swidget);
    return FALSE;
}

static void
on_authorize_complete (GObject *source,
                       GAsyncResult *result,
                       gpointer user_data)
{
	GtkToggleButton *button = GTK_TOGGLE_BUTTON (user_data);
	GError *error = NULL;

	if (!seahorse_ssh_op_authorize_finish (SEAHORSE_SSH_SOURCE (source), result, &error)) {
		seahorse_util_handle_error (&error, GTK_WIDGET (button),
		                            _("Couldn't change authorization for key."));
	}

	gtk_widget_set_sensitive (GTK_WIDGET (button), TRUE);
	g_object_unref (button);
}

G_MODULE_EXPORT void
on_ssh_trust_toggled (GtkToggleButton *button,
                      SeahorseWidget *swidget)
{
	SeahorseSSHSource *source;
	SeahorseSSHKey *skey;
	gboolean authorize;

	skey = SEAHORSE_SSH_KEY (SEAHORSE_OBJECT_WIDGET (swidget)->object);
	source = SEAHORSE_SSH_SOURCE (seahorse_object_get_place (SEAHORSE_OBJECT (skey)));

	authorize = gtk_toggle_button_get_active (button);
	gtk_widget_set_sensitive (GTK_WIDGET (button), FALSE);

	seahorse_ssh_op_authorize_async (source, skey, authorize,
	                                 NULL, on_authorize_complete, g_object_ref (button));
}

static void
on_passphrase_complete (GObject *source,
                        GAsyncResult *result,
                        gpointer user_data)
{
	GtkWidget *button = GTK_WIDGET (user_data);
	GError *error = NULL;

	if (!seahorse_ssh_op_change_passphrase_finish (SEAHORSE_SSH_KEY (source),
	                                               result, &error)) {
		seahorse_util_handle_error (&error, button, _("Couldn't change passphrase for key."));
	}

	gtk_widget_set_sensitive (button, TRUE);
	g_object_unref (button);
}

G_MODULE_EXPORT void
on_ssh_passphrase_button_clicked (GtkWidget *widget,
                                  SeahorseWidget *swidget)
{
	GObject *object;
	GtkWidget *button;

	object = SEAHORSE_OBJECT_WIDGET (swidget)->object;

	button = seahorse_widget_get_widget (swidget, "passphrase-button");
	gtk_widget_set_sensitive (button, FALSE);

	seahorse_ssh_op_change_passphrase_async (SEAHORSE_SSH_KEY (object), NULL,
	                                         on_passphrase_complete, g_object_ref (button));
}

static void
export_complete (GFile *file, GAsyncResult *result, guchar *contents)
{
	GError *err = NULL;
	gchar *uri, *unesc_uri;
	
	g_free (contents);
	
	if (!g_file_replace_contents_finish (file, result, NULL, &err)) {
		uri = g_file_get_uri (file);
		unesc_uri = g_uri_unescape_string (seahorse_util_uri_get_last (uri), NULL);
		seahorse_util_handle_error (&err, NULL, _("Couldn't export key to \"%s\""),
		                            unesc_uri);
        g_free (uri);
        g_free (unesc_uri);
	}
}

G_MODULE_EXPORT void
on_ssh_export_button_clicked (GtkWidget *widget, SeahorseWidget *swidget)
{
	SeahorsePlace *sksrc;
	GObject *object;
	GFile *file;
	GtkDialog *dialog;
	guchar *results;
	gsize n_results;
	gchar* uri = NULL;
	GError *err = NULL;

	object = SEAHORSE_OBJECT_WIDGET (swidget)->object;
	g_return_if_fail (SEAHORSE_IS_SSH_KEY (object));
	g_object_get (object, "place", &sksrc, NULL);
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
	
	if (err)
		seahorse_util_handle_error (&err, swidget, _("Couldn't export key."));

	g_free (uri);
}

static void
do_main (SeahorseWidget *swidget)
{
    SeahorseObject *object;
    SeahorseSSHKey *skey;
    GtkWidget *widget;
    gchar *text;
    const gchar *label;
    const gchar *template;

    object = SEAHORSE_OBJECT (SEAHORSE_OBJECT_WIDGET (swidget)->object);
    skey = SEAHORSE_SSH_KEY (object);

    /* Image */
    widget = GTK_WIDGET (gtk_builder_get_object (swidget->gtkbuilder, "key-image"));
    if (widget)
        gtk_image_set_from_icon_name (GTK_IMAGE (widget), SEAHORSE_ICON_KEY_SSH, GTK_ICON_SIZE_DIALOG);

    /* Name and title */
    label = seahorse_object_get_label (object);
    widget = GTK_WIDGET (gtk_builder_get_object (swidget->gtkbuilder, "comment-entry"));
    if (widget)
        gtk_entry_set_text (GTK_ENTRY (widget), label);
    widget = seahorse_widget_get_toplevel (swidget);
    gtk_window_set_title (GTK_WINDOW (widget), label);

    /* Key id */
    widget = GTK_WIDGET (gtk_builder_get_object (swidget->gtkbuilder, "id-label"));
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
    
    g_signal_handlers_block_by_func (widget, on_ssh_trust_toggled, swidget);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), 
                                  seahorse_ssh_key_get_trust (skey) >= SEAHORSE_VALIDITY_FULL);
    g_signal_handlers_unblock_by_func (widget, on_ssh_trust_toggled, swidget);
}

static void 
do_details (SeahorseWidget *swidget)
{
    SeahorseObject *object;
    SeahorseSSHKey *skey;
    GtkWidget *widget;
    const gchar *label;
    gchar *text;

    object = SEAHORSE_OBJECT (SEAHORSE_OBJECT_WIDGET (swidget)->object);
    skey = SEAHORSE_SSH_KEY (object);

    widget = GTK_WIDGET (gtk_builder_get_object (swidget->gtkbuilder, "fingerprint-label"));
    if (widget) {
        text = seahorse_ssh_key_get_fingerprint (skey);
        gtk_label_set_text (GTK_LABEL (widget), text);
        g_free (text);
    }

    widget = GTK_WIDGET (gtk_builder_get_object (swidget->gtkbuilder, "algo-label"));
    if (widget) {
        label = seahorse_ssh_key_get_algo_str (skey);
        gtk_label_set_text (GTK_LABEL (widget), label);
    }

    widget = GTK_WIDGET (gtk_builder_get_object (swidget->gtkbuilder, "location-label"));
    if (widget) {
        label = seahorse_ssh_key_get_location (skey);
        gtk_label_set_text (GTK_LABEL (widget), label);  
    }

    widget = GTK_WIDGET (gtk_builder_get_object (swidget->gtkbuilder, "strength-label"));
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
seahorse_ssh_key_properties_show (SeahorseSSHKey *skey,
                                  GtkWindow *parent)
{
    GObject *object = G_OBJECT (skey);
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
    
    widget = seahorse_widget_get_widget (swidget, "comment-entry");
    g_return_if_fail (widget != NULL);

    /* A public key only */
    if (seahorse_object_get_usage (SEAHORSE_OBJECT (skey)) != SEAHORSE_USAGE_PRIVATE_KEY) {
        seahorse_widget_set_visible (swidget, "passphrase-button", FALSE);
        seahorse_widget_set_visible (swidget, "export-button", FALSE);
    }

    widget = GTK_WIDGET (seahorse_widget_get_widget (swidget, swidget->name));
    g_signal_connect (widget, "response", G_CALLBACK (properties_response), swidget);
    seahorse_bind_objects (NULL, skey, (SeahorseTransfer)key_notify, swidget);

    seahorse_widget_show (swidget);
}
