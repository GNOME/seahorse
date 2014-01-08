/*
 * Seahorse
 *
 * Copyright (C) 2005 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the licence or (at
 * your option) any later version.
 *
 * See the included COPYING file for more information.
 */

#include "config.h"

#include "seahorse-ssh-dialogs.h"
#include "seahorse-ssh-exporter.h"
#include "seahorse-ssh-key.h"
#include "seahorse-ssh-operation.h"

#include "seahorse-bind.h"
#include "seahorse-common.h"
#include "seahorse-object.h"
#include "seahorse-object-widget.h"
#include "seahorse-util.h"
#include "seahorse-validity.h"

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
	GtkWidget *window;

	skey = SEAHORSE_SSH_KEY (SEAHORSE_OBJECT_WIDGET (swidget)->object);
	source = SEAHORSE_SSH_SOURCE (seahorse_object_get_place (SEAHORSE_OBJECT (skey)));

	text = gtk_entry_get_text (GTK_ENTRY (entry));

	/* Make sure not the same */
	if (skey->keydata->comment && g_utf8_collate (text, skey->keydata->comment) == 0)
		return;

	gtk_widget_set_sensitive (entry, FALSE);
	window = gtk_widget_get_toplevel (entry);

	closure = g_new0 (ssh_rename_closure, 1);
	closure->swidget = g_object_ref (swidget);
	closure->entry = GTK_ENTRY (entry);
	closure->original = g_strdup (skey->keydata->comment ? skey->keydata->comment : "");
	seahorse_ssh_op_rename_async (source, skey, text, GTK_WINDOW (window),
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
	GtkWidget *window;

	skey = SEAHORSE_SSH_KEY (SEAHORSE_OBJECT_WIDGET (swidget)->object);
	source = SEAHORSE_SSH_SOURCE (seahorse_object_get_place (SEAHORSE_OBJECT (skey)));

	authorize = gtk_toggle_button_get_active (button);
	gtk_widget_set_sensitive (GTK_WIDGET (button), FALSE);
	window = gtk_widget_get_toplevel (GTK_WIDGET (button));

	seahorse_ssh_op_authorize_async (source, skey, authorize, GTK_WINDOW (window),
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
	GtkWidget *window;

	object = SEAHORSE_OBJECT_WIDGET (swidget)->object;

	button = seahorse_widget_get_widget (swidget, "passphrase-button");
	gtk_widget_set_sensitive (button, FALSE);
	window = gtk_widget_get_toplevel (widget);

	seahorse_ssh_op_change_passphrase_async (SEAHORSE_SSH_KEY (object), GTK_WINDOW (window), NULL,
	                                         on_passphrase_complete, g_object_ref (button));
}

static void
on_export_complete (GObject *source,
                    GAsyncResult *result,
                    gpointer user_data)
{
	GtkWindow *parent = GTK_WINDOW (user_data);
	GError *error = NULL;

	if (!seahorse_exporter_export_to_file_finish (SEAHORSE_EXPORTER (source), result, &error))
		seahorse_util_handle_error (&error, parent, _("Couldn't export key"));

	g_object_unref (parent);
}

G_MODULE_EXPORT void
on_ssh_export_button_clicked (GtkWidget *widget, SeahorseWidget *swidget)
{
	SeahorseExporter *exporter;
	GList *exporters = NULL;
	GObject *object;
	GtkWindow *window;
	gchar *directory = NULL;
	GFile *file;

	object = SEAHORSE_OBJECT_WIDGET (swidget)->object;

	exporters = g_list_append (exporters, seahorse_ssh_exporter_new (object, TRUE));

	window = GTK_WINDOW (seahorse_widget_get_toplevel (swidget));
	if (seahorse_exportable_prompt (exporters, window, &directory, &file, &exporter)) {
		seahorse_exporter_export_to_file (exporter, file, TRUE, NULL,
		                                  on_export_complete, g_object_ref (window));
		g_free (directory);
		g_object_unref (file);
		g_object_unref (exporter);
	}

	g_list_free_full (exporters, g_object_unref);
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

GtkWindow *
seahorse_ssh_key_properties_show (SeahorseSSHKey *skey,
                                  GtkWindow *parent)
{
    GObject *object = G_OBJECT (skey);
    SeahorseWidget *swidget = NULL;
    GtkWidget *widget;

    swidget = seahorse_object_widget_new ("ssh-key-properties", parent, object);
    
    /* This happens if the window is already open */
    if (swidget == NULL)
        return NULL;

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
    return g_object_ref (seahorse_widget_get_toplevel (swidget));
}
