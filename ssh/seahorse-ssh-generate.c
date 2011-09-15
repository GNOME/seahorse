/*
 * Seahorse
 *
 * Copyright (C) 2006 Stefan Walter
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

#include <string.h>
 
#include <glib/gi18n.h>

#include "seahorse-ssh-dialogs.h"
#include "seahorse-ssh-source.h"
#include "seahorse-ssh-key.h"
#include "seahorse-ssh.h"
#include "seahorse-ssh-operation.h"

#include "seahorse-gtkstock.h"
#include "seahorse-progress.h"
#include "seahorse-registry.h"
#include "seahorse-util.h"
#include "seahorse-widget.h"


/* --------------------------------------------------------------------------
 * ACTIONS
 */

static void
on_ssh_generate_key (GtkAction *action, gpointer unused)
{
	SeahorseSource* sksrc;
	
	g_return_if_fail (GTK_IS_ACTION (action));
	
	sksrc = seahorse_context_find_source (seahorse_context_instance (), SEAHORSE_SSH_TYPE, SEAHORSE_LOCATION_LOCAL);
	g_return_if_fail (sksrc != NULL);
	
	seahorse_ssh_generate_show (SEAHORSE_SSH_SOURCE (sksrc), NULL);
}

static const GtkActionEntry ACTION_ENTRIES[] = {
	{ "ssh-generate-key", SEAHORSE_SSH_STOCK_ICON, N_ ("Secure Shell Key"), "", 
	  N_("Used to access other computers (eg: via a terminal)"), G_CALLBACK (on_ssh_generate_key) }
};

void
seahorse_ssh_generate_register (void)
{
	GtkActionGroup *actions;
	
	actions = gtk_action_group_new ("ssh-generate");

	gtk_action_group_set_translation_domain (actions, GETTEXT_PACKAGE);
	gtk_action_group_add_actions (actions, ACTION_ENTRIES, G_N_ELEMENTS (ACTION_ENTRIES), NULL);
	
	/* Register this as a generator */
	seahorse_registry_register_object (NULL, G_OBJECT (actions), SEAHORSE_SSH_TYPE_STR, "generator", NULL);
}

/* --------------------------------------------------------------------
 * DIALOGS
 */

#define DSA_SIZE 1024
#define DEFAULT_RSA_SIZE 2048

static void
on_change (GtkComboBox *combo, SeahorseWidget *swidget)
{
    const gchar *t;    
    GtkWidget *widget;
    
    widget = seahorse_widget_get_widget (swidget, "bits-entry");
    g_return_if_fail (widget != NULL);

    t = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (combo));
    if (t && strstr (t, "DSA")) {
        gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), DSA_SIZE);
        gtk_widget_set_sensitive (widget, FALSE);
    } else {
        gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), DEFAULT_RSA_SIZE);
        gtk_widget_set_sensitive (widget, TRUE);
    }
}

static void
on_generate_complete (GObject *source,
                      GAsyncResult *result,
                      gpointer user_data)
{
	GError *error = NULL;

	seahorse_ssh_op_generate_finish (SEAHORSE_SSH_SOURCE (source),
	                                 result, &error);

	if (error != NULL)
		seahorse_util_handle_error (&error, NULL, _("Couldn't generate Secure Shell key"));
}

static void
on_generate_complete_and_upload (GObject *source,
                                 GAsyncResult *result,
                                 gpointer user_data)
{
	GError *error = NULL;
	SeahorseObject *object;
	GList *keys;

	object = seahorse_ssh_op_generate_finish (SEAHORSE_SSH_SOURCE (source),
	                                          result, &error);

	if (error != NULL) {
		seahorse_util_handle_error (&error, NULL, _("Couldn't generate Secure Shell key"));

	} else {
		keys = g_list_append (NULL, object);
		seahorse_ssh_upload_prompt (keys, NULL);
		g_list_free (keys);
	}
}

static void
on_response (GtkDialog *dialog, guint response, SeahorseWidget *swidget)
{
    SeahorseSSHSource *src;
    GCancellable *cancellable;
    GtkWidget *widget;
    const gchar *email;
    const gchar *t;
    gboolean upload;
    guint type;
    guint bits;
    
    if (response == GTK_RESPONSE_HELP) {
        seahorse_widget_show_help (swidget);
        return;
    }
    
    if (response == GTK_RESPONSE_OK) 
        upload = TRUE;
    else if (response == GTK_RESPONSE_CLOSE)
        upload = FALSE;
    else {
        seahorse_widget_destroy (swidget);
        return;
    }
    
    /* The email address */
    widget = seahorse_widget_get_widget (swidget, "email-entry");
    g_return_if_fail (widget != NULL);
    email = gtk_entry_get_text (GTK_ENTRY (widget));
    
    /* The algorithm */
    widget = seahorse_widget_get_widget (swidget, "algorithm-choice");
    g_return_if_fail (widget != NULL);
    t = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (widget));
    if (t && strstr (t, "DSA"))
        type = SSH_ALGO_DSA;
    else
        type = SSH_ALGO_RSA;
    
    /* The number of bits */
    widget = seahorse_widget_get_widget (swidget, "bits-entry");
    g_return_if_fail (widget != NULL);
    bits = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (widget));
    if (bits < 512 || bits > 8192) {
        g_message ("invalid key size: %s defaulting to 2048", t);
        bits = 2048;
    }
    
    src = SEAHORSE_SSH_SOURCE (g_object_get_data (G_OBJECT (swidget), "source"));
    g_return_if_fail (SEAHORSE_IS_SSH_SOURCE (src));

    /* We start creation */
    cancellable = g_cancellable_new ();
    seahorse_ssh_op_generate_async (src, email, type, bits, cancellable,
                                    upload ? on_generate_complete_and_upload : on_generate_complete,
                                    NULL);
    seahorse_progress_show (cancellable, _("Creating Secure Shell Key"), FALSE);
    g_object_unref (cancellable);

    seahorse_widget_destroy (swidget);
}

void
seahorse_ssh_generate_show (SeahorseSSHSource *src, GtkWindow *parent)
{
    SeahorseWidget *swidget;
    GtkWidget *widget;
    
    swidget = seahorse_widget_new ("ssh-generate", parent);
    
    /* Widget already present */
    if (swidget == NULL)
        return;

    g_object_ref (src);
    g_object_set_data_full (G_OBJECT (swidget), "source", src, g_object_unref);

    g_signal_connect (G_OBJECT (seahorse_widget_get_widget (swidget, "algorithm-choice")), "changed", 
                    G_CALLBACK (on_change), swidget);

    g_signal_connect (seahorse_widget_get_toplevel (swidget), "response", 
                    G_CALLBACK (on_response), swidget);

    widget = seahorse_widget_get_widget (swidget, "ssh-image");
    g_return_if_fail (widget != NULL);
    gtk_image_set_from_stock (GTK_IMAGE (widget), SEAHORSE_STOCK_KEY_SSH, GTK_ICON_SIZE_DIALOG);

    /* on_change() gets called, bits entry is setup */
    widget = seahorse_widget_get_widget (swidget, "algorithm-choice");
    g_return_if_fail (widget != NULL);
    gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);
}
