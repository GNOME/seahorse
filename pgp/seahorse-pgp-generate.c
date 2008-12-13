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

#include "config.h"

#include <time.h>
#include <string.h>
 
#include <glib/gi18n.h>
 
#include "egg-datetime.h"

#include "seahorse-widget.h"
#include "seahorse-util.h"
#include "seahorse-progress.h"
#include "seahorse-gtkstock.h"
#include "seahorse-passphrase.h"
#include "seahorse-gtkstock.h"

#include "seahorse-pgp.h"
#include "seahorse-pgp-dialogs.h"
#include "seahorse-pgp-key.h"
#include "seahorse-pgp-key-op.h"
#include "seahorse-pgp-source.h"

#include "common/seahorse-registry.h"

/* --------------------------------------------------------------------------
 * ACTIONS
 */

static void
on_pgp_generate_key (GtkAction *action, gpointer unused)
{
	SeahorseSource* sksrc;
	
	g_return_if_fail (GTK_IS_ACTION (action));
	
	sksrc = seahorse_context_find_source (seahorse_context_for_app (), SEAHORSE_PGP_TYPE, SEAHORSE_LOCATION_LOCAL);
	g_return_if_fail (sksrc != NULL);
	
	seahorse_pgp_generate_show (SEAHORSE_PGP_SOURCE (sksrc), NULL);
}

static const GtkActionEntry ACTION_ENTRIES[] = {
	{ "pgp-generate-key", SEAHORSE_PGP_STOCK_ICON, N_ ("PGP Key"), "", 
	  N_("Used to encrypt email and files"), G_CALLBACK (on_pgp_generate_key) }
};

void
seahorse_pgp_generate_register (void)
{
	GtkActionGroup *actions;
	
	actions = gtk_action_group_new ("pgp-generate");

	gtk_action_group_set_translation_domain (actions, GETTEXT_PACKAGE);
	gtk_action_group_add_actions (actions, ACTION_ENTRIES, G_N_ELEMENTS (ACTION_ENTRIES), NULL);
	
	/* Register this as a generator */
	seahorse_registry_register_object (NULL, actions, SEAHORSE_PGP_TYPE_STR, "generator", NULL);
}

/* --------------------------------------------------------------------------
 * DIALOGS
 */

typedef struct _AlgorithmDesc {
    const gchar* desc;
    guint type;
    guint min;
    guint max;
} AlgorithmDesc;

static AlgorithmDesc available_algorithms[] = {
    { N_("DSA Elgamal"),     DSA_ELGAMAL, ELGAMAL_MIN, LENGTH_MAX  },
    { N_("DSA (sign only)"), DSA,         DSA_MIN,     DSA_MAX     },
    { N_("RSA (sign only)"), RSA_SIGN,    RSA_MIN,     LENGTH_MAX  }
};

static void
completion_handler (SeahorseOperation *op, gpointer data)
{
    GError *error = NULL;
    if (!seahorse_operation_is_successful (op)) {
        seahorse_operation_copy_error (op, &error);
        seahorse_util_handle_error (error, _("Couldn't generate PGP key"));
        g_clear_error (&error);
    }
}

static GtkWidget *
_seahorse_pgp_generate_get_expiry_date (SeahorseWidget *swidget)
{
    GtkWidget *widget;
    GList *children;

    g_return_val_if_fail (swidget != NULL, NULL);

    widget = seahorse_widget_get_widget (swidget, "expiry-date-container");
    g_return_val_if_fail (widget != NULL, NULL);

    children = gtk_container_get_children (GTK_CONTAINER (widget));
    g_return_val_if_fail (children, NULL);

    /* The first widget should be the expiry-date */
    widget = g_list_nth_data (children, 0);

    g_list_free (children);

    return widget;
}

static void
on_response (GtkDialog *dialog, guint response, SeahorseWidget *swidget)
{
    SeahorsePGPSource *sksrc;
    SeahorseOperation *op;
    GtkWidget *widget;
    gchar *name;
    const gchar *email;
    const gchar *comment;
    const gchar *pass;
    gpgme_error_t gerr;
    gint sel;
    guint type;
    time_t expires;
    guint bits;
    
    if (response == GTK_RESPONSE_HELP) {
        seahorse_widget_show_help (swidget);
        return;
    }
    
    if (response != GTK_RESPONSE_OK) {
        seahorse_widget_destroy (swidget);
        return;
    }
    
    /* The name */
    widget = seahorse_widget_get_widget (swidget, "name-entry");
    g_return_if_fail (widget != NULL);
    name = g_strdup(gtk_entry_get_text (GTK_ENTRY (widget)));
    g_return_if_fail (name);
    
    /* Make sure it's the right length. Should have been checked earlier */
    name = g_strstrip (name);
    g_return_if_fail (strlen(name) >= 5);
    
    /* The email address */
    widget = seahorse_widget_get_widget (swidget, "email-entry");
    g_return_if_fail (widget != NULL);
    email = gtk_entry_get_text (GTK_ENTRY (widget));
    
    /* The comment */
    widget = seahorse_widget_get_widget (swidget, "comment-entry");
    g_return_if_fail (widget != NULL);
    comment = gtk_entry_get_text (GTK_ENTRY (widget));
    
    /* The algorithm */
    widget = seahorse_widget_get_widget (swidget, "algorithm-choice");
    g_return_if_fail (widget != NULL);
    sel = gtk_combo_box_get_active (GTK_COMBO_BOX (widget));
    g_assert (sel <= G_N_ELEMENTS(available_algorithms));
    type = available_algorithms[sel].type;

    /* The number of bits */
    widget = seahorse_widget_get_widget (swidget, "bits-entry");
    g_return_if_fail (widget != NULL);
    bits = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (widget));
    if (bits < 512 || bits > 8192) {
        g_message ("invalid key size: %s defaulting to 2048", available_algorithms[sel].desc);
        bits = 2048;
    }
    
    /* The expiry */
    widget = seahorse_widget_get_widget (swidget, "expires-check");
    g_return_if_fail (widget != NULL);
    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)))
        expires = 0;
    else {
        widget = _seahorse_pgp_generate_get_expiry_date (swidget);
        g_return_if_fail (widget);

        egg_datetime_get_as_time_t (EGG_DATETIME (widget), &expires);
    }

    sksrc = SEAHORSE_PGP_SOURCE (g_object_get_data (G_OBJECT (swidget), "source"));
    g_assert (SEAHORSE_IS_PGP_SOURCE (sksrc));
    
    /* Less confusing with less on the screen */
    gtk_widget_hide (seahorse_widget_get_toplevel (swidget));
    
    dialog = seahorse_passphrase_prompt_show (_("Passphrase for New PGP Key"), 
                                              _("Enter the passphrase for your new key twice."), 
                                              NULL, NULL, TRUE);
    if (gtk_dialog_run (dialog) == GTK_RESPONSE_ACCEPT)
    {
        pass = seahorse_passphrase_prompt_get (dialog);
        op = seahorse_pgp_key_op_generate (sksrc, name, email, comment,
                                           pass, type, bits, expires, &gerr);
    
        if (!GPG_IS_OK (gerr)) {
            seahorse_pgp_handle_gpgme_error (gerr, _("Couldn't generate key"));
        } else {
            seahorse_progress_show (op, _("Generating key"), TRUE);
            seahorse_operation_watch (op, (SeahorseDoneFunc)completion_handler, NULL, NULL, NULL);
            g_object_unref (op);
        }
    }
        
    gtk_widget_destroy (GTK_WIDGET (dialog));
    seahorse_widget_destroy (swidget);
    g_free (name);
}

static void
entry_changed(GtkEditable *editable, SeahorseWidget *swidget)
{
    GtkWidget *widget;
    gchar *name;

    /* A 5 character name is required */
    widget = seahorse_widget_get_widget (swidget, "name-entry");
    g_return_if_fail (widget != NULL);
    name = g_strdup (gtk_entry_get_text (GTK_ENTRY (widget)));
    
    widget = seahorse_widget_get_widget (swidget, "ok");
    g_return_if_fail (widget != NULL);
    
    gtk_widget_set_sensitive (widget, name && strlen (g_strstrip (name)) >= 5);
    g_free (name);
}

static void
expires_toggled (GtkToggleButton *button, SeahorseWidget *swidget)
{
    GtkWidget *widget;
    
    widget = _seahorse_pgp_generate_get_expiry_date (swidget);
    g_return_if_fail (widget);

    gtk_widget_set_sensitive (widget, !gtk_toggle_button_get_active (button));
}

static void
algorithm_changed (GtkComboBox *combo, SeahorseWidget *swidget)
{
    GtkWidget *widget;
    gint idx;
    
    idx = gtk_combo_box_get_active (combo);
    g_assert (idx < G_N_ELEMENTS (available_algorithms));
    
    widget = seahorse_widget_get_widget (swidget, "bits-entry");
    g_return_if_fail (widget != NULL);
    
    gtk_spin_button_set_range (GTK_SPIN_BUTTON (widget), 
                               available_algorithms[idx].min, 
                               available_algorithms[idx].max);
}

void
seahorse_pgp_generate_show (SeahorsePGPSource *sksrc, GtkWindow *parent)
{
    SeahorseWidget *swidget;
    GtkWidget *widget, *datetime;
    time_t expires;
    guint i;
    
    swidget = seahorse_widget_new ("pgp-generate", parent);
    
    /* Widget already present */
    if (swidget == NULL)
        return;
    
    widget = seahorse_widget_get_widget (swidget, "pgp-image");
    g_return_if_fail (widget != NULL);
    gtk_image_set_from_stock (GTK_IMAGE (widget), SEAHORSE_STOCK_SECRET, 
                              GTK_ICON_SIZE_DIALOG);
    
    /* The algoritms */
    widget = seahorse_widget_get_widget (swidget, "algorithm-choice");
    g_return_if_fail (widget != NULL);
    gtk_combo_box_remove_text (GTK_COMBO_BOX (widget), 0);
    for(i = 0; i < G_N_ELEMENTS(available_algorithms); i++)
        gtk_combo_box_append_text (GTK_COMBO_BOX (widget), _(available_algorithms[i].desc));
    gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);
    algorithm_changed (GTK_COMBO_BOX (widget), swidget);
    
    expires = time (NULL);
    expires += (60 * 60 * 24 * 365); /* Seconds in a year */

    /* Default expiry date */
    widget = seahorse_widget_get_widget (swidget, "expiry-date-container");
    g_return_if_fail (widget != NULL);
    datetime = egg_datetime_new_from_time_t (expires);
    gtk_box_pack_start (GTK_BOX (widget), datetime, TRUE, TRUE, 0);
    gtk_widget_set_sensitive (datetime, FALSE);
    gtk_widget_show_all (widget);
    
    g_object_ref (sksrc);
    g_object_set_data_full (G_OBJECT (swidget), "source", sksrc, g_object_unref);
    
    glade_xml_signal_connect_data (swidget->xml, "on_entry_changed",
                                   G_CALLBACK (entry_changed), swidget);
    glade_xml_signal_connect_data (swidget->xml, "on_expires_toggled",
                                   G_CALLBACK (expires_toggled), swidget);
    glade_xml_signal_connect_data (swidget->xml, "on_algorithm_changed", 
                                   G_CALLBACK (algorithm_changed), swidget);
    entry_changed (NULL, swidget);

    g_signal_connect (seahorse_widget_get_toplevel (swidget), "response", 
                      G_CALLBACK (on_response), swidget);
}
