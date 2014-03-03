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
 * along with this program; if not, see
 * <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "seahorse-gpgme-dialogs.h"

#include "seahorse-pgp.h"
#include "seahorse-pgp-backend.h"
#include "seahorse-gpgme.h"
#include "seahorse-gpgme-key.h"
#include "seahorse-gpgme-key-op.h"
#include "seahorse-gpgme-keyring.h"

#include "seahorse-common.h"

#include "libegg/egg-datetime.h"

#include "libseahorse/seahorse-passphrase.h"
#include "libseahorse/seahorse-progress.h"
#include "libseahorse/seahorse-util.h"
#include "libseahorse/seahorse-widget.h"

#include <glib/gi18n.h>

#include <string.h>
#include <time.h>

/**
 * SECTION:seahorse-gpgme-generate
 * @short_description: This file contains creation dialogs for pgp key creation.
 *
 **/

void           on_gpgme_generate_response                    (GtkDialog *dialog,
                                                              gint response,
                                                              gpointer user_data);

void           on_gpgme_generate_entry_changed               (GtkEditable *editable,
                                                              gpointer user_data);

void           on_gpgme_generate_expires_toggled             (GtkToggleButton *button,
                                                              gpointer user_data);

void           on_gpgme_generate_algorithm_changed           (GtkComboBox *combo,
                                                              gpointer user_data);

/* --------------------------------------------------------------------------
 * ACTIONS
 */

/**
 * on_pgp_generate_key:
 * @action: verified to be an action, not more
 * @unused: not used
 *
 * Calls the function that displays the key creation dialog
 *
 */
static void
on_pgp_generate_key (GtkAction *action, gpointer unused)
{
	SeahorseGpgmeKeyring* keyring;

	g_return_if_fail (GTK_IS_ACTION (action));

	keyring = seahorse_pgp_backend_get_default_keyring (NULL);
	g_return_if_fail (keyring != NULL);

	seahorse_gpgme_generate_show (keyring,
	                              seahorse_action_get_window (action),
	                              NULL, NULL, NULL);
}

static const GtkActionEntry ACTION_ENTRIES[] = {
	{ "pgp-generate-key", GCR_ICON_KEY_PAIR, N_ ("PGP Key"), "",
	  N_("Used to encrypt email and files"), G_CALLBACK (on_pgp_generate_key) }
};

/**
 * seahorse_gpgme_generate_register:
 *
 * Registers the action group for the pgp key creation dialog
 *
 */
void
seahorse_gpgme_generate_register (void)
{
	GtkActionGroup *actions;
	
	actions = gtk_action_group_new ("gpgme-generate");

	gtk_action_group_set_translation_domain (actions, GETTEXT_PACKAGE);
	gtk_action_group_add_actions (actions, ACTION_ENTRIES, G_N_ELEMENTS (ACTION_ENTRIES), NULL);
	
	/* Register this as a generator */
	seahorse_registry_register_object (G_OBJECT (actions), "generator");
}

/* --------------------------------------------------------------------------
 * DIALOGS
 */

typedef struct _AlgorithmDesc {
    const gchar* desc;
    guint type;
    guint min;
    guint max;
    guint def;
} AlgorithmDesc;

static AlgorithmDesc available_algorithms[] = {
    { N_("RSA"),             RSA_RSA,     RSA_MIN,     LENGTH_MAX, LENGTH_DEFAULT  },
    { N_("DSA ElGamal"),     DSA_ELGAMAL, ELGAMAL_MIN, LENGTH_MAX, LENGTH_DEFAULT  },
    { N_("DSA (sign only)"), DSA,         DSA_MIN,     DSA_MAX,    LENGTH_DEFAULT  },
    { N_("RSA (sign only)"), RSA_SIGN,    RSA_MIN,     LENGTH_MAX, LENGTH_DEFAULT  }
};

/**
 * completion_handler:
 * @op: the seahorse operation
 * @data: ignored
 *
 * If the key generation caused errors, these will be displayed in this function
 */

/**
 * get_expiry_date:
 * @swidget: the seahorse main widget
 *
 * Returns: the expiry date widget
 */
static GtkWidget *
get_expiry_date (SeahorseWidget *swidget)
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
on_generate_key_complete (GObject *source,
                          GAsyncResult *result,
                          gpointer user_data)
{
	GError *error = NULL;

	if (!seahorse_gpgme_key_op_generate_finish (SEAHORSE_GPGME_KEYRING (source), result, &error))
		seahorse_util_handle_error (&error, NULL, _("Couldn't generate PGP key"));
}

/**
 * gpgme_generate_key:
 * @sksrc: the seahorse source
 * @name: the user's full name
 * @email: the user's email address
 * @comment: a comment, added to the key
 * @type: key type like DSA_ELGAMAL
 * @bits: the number of bits for the key to generate (2048)
 * @expires: expiry date can be 0
 *
 * Displays a password generation box and creates a key afterwards. For the key
 * data it uses @name @email and @comment ncryption is chosen by @type and @bits
 * @expire sets the expiry date
 *
 */
void
seahorse_gpgme_generate_key (SeahorseGpgmeKeyring *keyring,
                             const gchar *name,
                             const gchar *email,
                             const gchar *comment,
                             guint type,
                             guint bits,
                             time_t expires,
                             GtkWindow *parent)
{
	GCancellable *cancellable;
	const gchar *pass;
	GtkDialog *dialog;
	const gchar *notice;

	dialog = seahorse_passphrase_prompt_show (_("Passphrase for New PGP Key"),
	                                          _("Enter the passphrase for your new key twice."),
	                                          NULL, NULL, TRUE);
	gtk_window_set_transient_for (GTK_WINDOW (dialog), parent);
	if (gtk_dialog_run (dialog) == GTK_RESPONSE_ACCEPT) {
		pass = seahorse_passphrase_prompt_get (dialog);
		cancellable = g_cancellable_new ();
		seahorse_gpgme_key_op_generate_async (keyring, name, email, comment,
		                                      pass, type, bits, expires,
		                                      cancellable, on_generate_key_complete,
		                                      NULL);

		/* Has line breaks because GtkLabel is completely broken WRT wrapping */
		notice = _("When creating a key we need to generate a lot of\n"
		           "random data and we need you to help. It's a good\n"
		           "idea to perform some other action like typing on\n"
		           "the keyboard, moving the mouse, using applications.\n"
		           "This gives the system the random data that it needs.");
		seahorse_progress_show_with_notice (cancellable, _("Generating key"), notice, FALSE);
		g_object_unref (cancellable);
	}

	gtk_widget_destroy (GTK_WIDGET (dialog));
}


/**
 * on_gpgme_generate_response:
 * @dialog:
 * @response: the user's response
 * @swidget: the seahorse main widget
 *
 * If everything is allright, a passphrase is requested and the key will be
 * generated
 *
 */
G_MODULE_EXPORT void
on_gpgme_generate_response (GtkDialog *dialog,
                            gint response,
                            gpointer user_data)
{
    SeahorseWidget *swidget = SEAHORSE_WIDGET (user_data);
    SeahorseGpgmeKeyring *keyring;
    GtkWidget *widget;
    gchar *name;
    const gchar *email;
    const gchar *comment;
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
    g_assert (sel <= (gint) G_N_ELEMENTS(available_algorithms));
    type = available_algorithms[sel].type;

    /* The number of bits */
    widget = seahorse_widget_get_widget (swidget, "bits-entry");
    g_return_if_fail (widget != NULL);
    bits = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (widget));
    if (bits < available_algorithms[sel].min || bits > available_algorithms[sel].max) {
        bits = available_algorithms[sel].def;
        g_message ("invalid key size: %s defaulting to %u", available_algorithms[sel].desc, bits);
    }
    
    /* The expiry */
    widget = seahorse_widget_get_widget (swidget, "expires-check");
    g_return_if_fail (widget != NULL);
    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)))
        expires = 0;
    else {
        widget = get_expiry_date (swidget);
        g_return_if_fail (widget);

        egg_datetime_get_as_time_t (EGG_DATETIME (widget), &expires);
    }

    keyring = SEAHORSE_GPGME_KEYRING (g_object_get_data (G_OBJECT (swidget), "source"));
    g_assert (SEAHORSE_IS_GPGME_KEYRING (keyring));

    /* Less confusing with less on the screen */
    gtk_widget_hide (seahorse_widget_get_toplevel (swidget));

    seahorse_gpgme_generate_key (keyring, name, email, comment, type, bits, expires,
                                 gtk_window_get_transient_for (GTK_WINDOW (dialog)));

    seahorse_widget_destroy (swidget);
    g_free (name);
}

/**
 * on_gpgme_generate_entry_changed:
 * @editable: ignored
 * @swidget: The main seahorse widget
 *
 * If the name has more than 5 characters, this sets the ok button sensitive
 *
 */
G_MODULE_EXPORT void
on_gpgme_generate_entry_changed (GtkEditable *editable,
                                 gpointer user_data)
{
    SeahorseWidget *swidget = SEAHORSE_WIDGET (user_data);
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

/**
 * on_gpgme_generate_expires_toggled:
 * @button: the toggle button
 * @swidget: Main seahorse widget
 *
 * Handles the expires toggle button feedback
 */
G_MODULE_EXPORT void
on_gpgme_generate_expires_toggled (GtkToggleButton *button,
                                   gpointer user_data)
{
    SeahorseWidget *swidget = SEAHORSE_WIDGET (user_data);
    GtkWidget *widget;
    
    widget = get_expiry_date (swidget);
    g_return_if_fail (widget);

    gtk_widget_set_sensitive (widget, !gtk_toggle_button_get_active (button));
}

/**
 * on_gpgme_generate_algorithm_changed:
 * @combo: the algorithm combo
 * @swidget: the main seahorse widget
 *
 * Sets the bit range depending on the algorithm set
 *
 */
G_MODULE_EXPORT void
on_gpgme_generate_algorithm_changed (GtkComboBox *combo,
                                     gpointer user_data)
{
    SeahorseWidget *swidget = SEAHORSE_WIDGET (user_data);
    GtkWidget *widget;
    gint sel;
    
    sel = gtk_combo_box_get_active (combo);
    g_assert (sel < (int)G_N_ELEMENTS (available_algorithms));
    
    widget = seahorse_widget_get_widget (swidget, "bits-entry");
    g_return_if_fail (widget != NULL);
    
    gtk_spin_button_set_range (GTK_SPIN_BUTTON (widget), 
                               available_algorithms[sel].min,
                               available_algorithms[sel].max);

    /* Set sane default key length */
    if (available_algorithms[sel].def > available_algorithms[sel].max)
        gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), available_algorithms[sel].max);
    else
        gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), available_algorithms[sel].def);
}

/**
 * seahorse_gpgme_generate_show:
 * @keyring: the gpgme source
 * @parent: the parent window
 * @name: The user name, can be NULL if not available
 * @email: The user's email address, can be NULL if not available
 * @comment: The comment to add to the key. Can be NULL
 *
 * Shows the gpg key generation dialog, sets default entries.
 *
 */
void
seahorse_gpgme_generate_show (SeahorseGpgmeKeyring *keyring,
                              GtkWindow *parent,
                              const gchar * name,
                              const gchar *email,
                              const gchar *comment)
{
    SeahorseWidget *swidget;
    GtkWidget *widget, *datetime;
    time_t expires;
    guint i;
    
    swidget = seahorse_widget_new ("pgp-generate", parent);
    
    /* Widget already present */
    if (swidget == NULL)
        return;

    if (name)
    {
        widget = seahorse_widget_get_widget (swidget, "name-entry");
        g_return_if_fail (widget != NULL);
        gtk_entry_set_text(GTK_ENTRY(widget),name);
    }

    if (email)
    {
        widget = seahorse_widget_get_widget (swidget, "email-entry");
        g_return_if_fail (widget != NULL);
        gtk_entry_set_text(GTK_ENTRY(widget),email);
    }

    if (comment)
    {
        widget = seahorse_widget_get_widget (swidget, "comment-entry");
        g_return_if_fail (widget != NULL);
        gtk_entry_set_text(GTK_ENTRY(widget),comment);
    }

    /* The algoritms */
    widget = seahorse_widget_get_widget (swidget, "algorithm-choice");
    g_return_if_fail (widget != NULL);
    gtk_combo_box_text_remove (GTK_COMBO_BOX_TEXT (widget), 0);
    for(i = 0; i < G_N_ELEMENTS(available_algorithms); i++)
        gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (widget), _(available_algorithms[i].desc));
    gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);
    on_gpgme_generate_algorithm_changed (GTK_COMBO_BOX (widget), swidget);
    
    expires = time (NULL);
    expires += (60 * 60 * 24 * 365); /* Seconds in a year */

    /* Default expiry date */
    widget = seahorse_widget_get_widget (swidget, "expiry-date-container");
    g_return_if_fail (widget != NULL);
    datetime = egg_datetime_new_from_time_t (expires);
    gtk_box_pack_start (GTK_BOX (widget), datetime, TRUE, TRUE, 0);
    gtk_widget_set_sensitive (datetime, FALSE);
    gtk_widget_show_all (widget);

    g_object_set_data_full (G_OBJECT (swidget), "source", g_object_ref (keyring), g_object_unref);

    on_gpgme_generate_entry_changed (NULL, swidget);
}
