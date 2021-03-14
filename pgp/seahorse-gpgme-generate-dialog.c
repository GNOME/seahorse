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

#include "seahorse-gpgme-generate-dialog.h"

#include "seahorse-pgp-backend.h"
#include "seahorse-gpgme.h"
#include "seahorse-gpgme-key.h"
#include "seahorse-gpgme-key-op.h"
#include "seahorse-gpgme-keyring.h"

#include "seahorse-common.h"

#include "libseahorse/seahorse-progress.h"
#include "libseahorse/seahorse-util.h"

#include <glib/gi18n.h>

#include <string.h>
#include <time.h>

/**
 * SECTION:seahorse-gpgme-generate
 * @short_description: This file contains creation dialogs for pgp key creation.
 *
 **/

struct _SeahorseGpgmeGenerateDialog {
    GtkDialog parent_instance;

    SeahorseGpgmeKeyring *keyring;

    GtkWidget *name_entry;
    GtkWidget *email_entry;
    GtkWidget *comment_entry;

    GtkWidget *algorithm_choice;
    GtkWidget *bits_entry;

    GtkWidget *expires_check;
    GtkWidget *expires_datepicker;
};

enum {
    PROP_0,
    PROP_KEYRING,
    N_PROPS
};
static GParamSpec *obj_props[N_PROPS] = { NULL, };

G_DEFINE_TYPE (SeahorseGpgmeGenerateDialog, seahorse_gpgme_generate_dialog, GTK_TYPE_DIALOG)

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

static void
on_generate_key_complete (GObject *source,
                          GAsyncResult *result,
                          gpointer user_data)
{
    SeahorseCatalog *catalog = SEAHORSE_CATALOG (user_data);
    GError *error = NULL;

    if (!seahorse_gpgme_key_op_generate_finish (SEAHORSE_GPGME_KEYRING (source), result, &error)) {
        seahorse_util_handle_error (&error,
                                    GTK_WINDOW (catalog),
                                    _("Couldn’t generate PGP key"));
        return;
    }

    g_action_group_activate_action (G_ACTION_GROUP (catalog),
                                    "focus-place",
                                    g_variant_new_string ("gnupg"));
}

/**
 * gpgme_generate_key:
 * @sksrc: the seahorse source
 * @name: the user's full name
 * @email: the user's email address
 * @comment: a comment, added to the key
 * @type: key type like DSA_ELGAMAL
 * @bits: the number of bits for the key to generate (2048)
 * @expires: expiry date (or %NULL if none)
 *
 * Displays a password generation box and creates a key afterwards. For the key
 * data it uses @name @email and @comment ncryption is chosen by @type and @bits
 * @expire sets the expiry date
 *
 */
void
seahorse_gpgme_generate_key (SeahorseGpgmeKeyring *keyring,
                             const char           *name,
                             const char           *email,
                             const char           *comment,
                             unsigned int         type,
                             unsigned int         bits,
                             GDateTime           *expires,
                             GtkWindow           *parent)
{
    GCancellable *cancellable;
    const gchar *pass;
    SeahorsePassphrasePrompt *dialog;
    const gchar *notice;

    dialog = seahorse_passphrase_prompt_show_dialog (_("Passphrase for New PGP Key"),
                                              _("Enter the passphrase for your new key twice."),
                                              NULL, NULL, TRUE);
    gtk_window_set_transient_for (GTK_WINDOW (dialog), parent);
    if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
        pass = seahorse_passphrase_prompt_get_text (dialog);
        cancellable = g_cancellable_new ();
        seahorse_gpgme_key_op_generate_async (keyring, name, email, comment,
                                              pass, type, bits, expires,
                                              cancellable, on_generate_key_complete,
                                              parent);

        /* Has line breaks because GtkLabel is completely broken WRT wrapping */
        notice = _("When creating a key we need to generate a lot of\n"
                   "random data and we need you to help. It’s a good\n"
                   "idea to perform some other action like typing on\n"
                   "the keyboard, moving the mouse, using applications.\n"
                   "This gives the system the random data that it needs.");
        seahorse_progress_show_with_notice (cancellable, _("Generating key"), notice, FALSE);
        g_object_unref (cancellable);
    }

    gtk_widget_destroy (GTK_WIDGET (dialog));
}

/* If the name has more than 5 characters, this sets the ok button sensitive */
static void
on_gpgme_generate_entry_changed (GtkEditable *editable,
                                 gpointer user_data)
{
    SeahorseGpgmeGenerateDialog *self = SEAHORSE_GPGME_GENERATE_DIALOG (user_data);
    g_autofree char *name = NULL;
    gboolean name_long_enough;

    /* A 5 character name is required */
    name = g_strdup (gtk_entry_get_text (GTK_ENTRY (self->name_entry)));
    name_long_enough = name && strlen (g_strstrip (name)) >= 5;

    /* If not, show the user and disable the create button */
    if (!name_long_enough) {
        g_object_set (self->name_entry,
            "secondary-icon-name", "dialog-warning-symbolic",
            "secondary-icon-tooltip-text", _("Name must be at least 5 characters long."),
            NULL);
    } else {
        g_object_set (self->name_entry, "secondary-icon-name", NULL, NULL);
    }

    gtk_dialog_set_response_sensitive (GTK_DIALOG (self), GTK_RESPONSE_OK, name_long_enough);
}

/* Handles the expires toggle button feedback */
static void
on_gpgme_generate_expires_toggled (GtkToggleButton *button,
                                   gpointer user_data)
{
    SeahorseGpgmeGenerateDialog *self = SEAHORSE_GPGME_GENERATE_DIALOG (user_data);

    gtk_widget_set_sensitive (self->expires_datepicker,
                              !gtk_toggle_button_get_active (button));
}

/* Changes the bit range depending on the algorithm set */
static void
on_gpgme_generate_algorithm_changed (GtkComboBox *combo,
                                     gpointer user_data)
{
    SeahorseGpgmeGenerateDialog *self = SEAHORSE_GPGME_GENERATE_DIALOG (user_data);
    int sel;

    sel = gtk_combo_box_get_active (combo);
    g_assert (sel < (int)G_N_ELEMENTS (available_algorithms));

    gtk_spin_button_set_range (GTK_SPIN_BUTTON (self->bits_entry),
                               available_algorithms[sel].min,
                               available_algorithms[sel].max);

    /* Set sane default key length */
    if (available_algorithms[sel].def > available_algorithms[sel].max)
        gtk_spin_button_set_value (GTK_SPIN_BUTTON (self->bits_entry), available_algorithms[sel].max);
    else
        gtk_spin_button_set_value (GTK_SPIN_BUTTON (self->bits_entry), available_algorithms[sel].def);
}

static void
seahorse_gpgme_generate_dialog_response (GtkDialog *dialog, int response)
{
    SeahorseGpgmeGenerateDialog *self = SEAHORSE_GPGME_GENERATE_DIALOG (dialog);
    g_autofree char *name = NULL;
    const char *email, *comment;
    int sel;
    guint type;
    g_autoptr(GDateTime) expires = NULL;
    guint bits;

    if (response != GTK_RESPONSE_OK)
        return;

    /* Make sure the name is the right length. Should've been checked earlier */
    name = g_strdup (gtk_entry_get_text (GTK_ENTRY (self->name_entry)));
    g_return_if_fail (name);
    name = g_strstrip (name);
    g_return_if_fail (strlen(name) >= 5);

    email = gtk_entry_get_text (GTK_ENTRY (self->email_entry));
    comment = gtk_entry_get_text (GTK_ENTRY (self->comment_entry));

    /* The algorithm */
    sel = gtk_combo_box_get_active (GTK_COMBO_BOX (self->algorithm_choice));
    g_assert (sel <= (int) G_N_ELEMENTS(available_algorithms));
    type = available_algorithms[sel].type;

    /* The number of bits */
    bits = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (self->bits_entry));
    if (bits < available_algorithms[sel].min || bits > available_algorithms[sel].max) {
        bits = available_algorithms[sel].def;
        g_message ("invalid key size: %s defaulting to %u", available_algorithms[sel].desc, bits);
    }

    /* The expiry */
    if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->expires_check)))
        expires = seahorse_date_picker_get_datetime (SEAHORSE_DATE_PICKER (self->expires_datepicker));

    /* Less confusing with less on the screen */
    gtk_widget_hide (GTK_WIDGET (self));

    seahorse_gpgme_generate_key (self->keyring,
                                 name, email, comment, type, bits, expires,
                                 gtk_window_get_transient_for (GTK_WINDOW (self)));
}

static void
seahorse_gpgme_generate_dialog_get_property (GObject *object,
                                             guint prop_id,
                                             GValue *value,
                                             GParamSpec *pspec)
{
    SeahorseGpgmeGenerateDialog *self = SEAHORSE_GPGME_GENERATE_DIALOG (object);

    switch (prop_id) {
    case PROP_KEYRING:
        g_value_set_object (value, self->keyring);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
seahorse_gpgme_generate_dialog_set_property (GObject *object,
                                             guint prop_id,
                                             const GValue *value,
                                             GParamSpec *pspec)
{
    SeahorseGpgmeGenerateDialog *self = SEAHORSE_GPGME_GENERATE_DIALOG (object);

    switch (prop_id) {
    case PROP_KEYRING:
        g_clear_object (&self->keyring);
        self->keyring = g_value_dup_object (value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
seahorse_gpgme_generate_dialog_finalize (GObject *obj)
{
    SeahorseGpgmeGenerateDialog *self = SEAHORSE_GPGME_GENERATE_DIALOG (obj);

    g_clear_object (&self->keyring);

    G_OBJECT_CLASS (seahorse_gpgme_generate_dialog_parent_class)->finalize (obj);
}

static void
seahorse_gpgme_generate_dialog_init (SeahorseGpgmeGenerateDialog *self)
{
    g_autoptr (GDateTime) now = NULL;
    g_autoptr (GDateTime) next_year = NULL;
    guint i;

    gtk_widget_init_template (GTK_WIDGET (self));

    /* The algorithms */
    gtk_combo_box_text_remove (GTK_COMBO_BOX_TEXT (self->algorithm_choice), 0);
    for (i = 0; i < G_N_ELEMENTS(available_algorithms); i++) {
        gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (self->algorithm_choice),
                                        _(available_algorithms[i].desc));
    }
    gtk_combo_box_set_active (GTK_COMBO_BOX (self->algorithm_choice), 0);
    on_gpgme_generate_algorithm_changed (GTK_COMBO_BOX (self->algorithm_choice),
                                         self);

    /* Default expiry date */
    now = g_date_time_new_now_utc ();
    next_year = g_date_time_add_years (now, 1);
    seahorse_date_picker_set_datetime (SEAHORSE_DATE_PICKER (self->expires_datepicker),
                                       next_year);

    on_gpgme_generate_entry_changed (NULL, self);
}

static void
seahorse_gpgme_generate_dialog_class_init (SeahorseGpgmeGenerateDialogClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
    GtkDialogClass *dialog_class = GTK_DIALOG_CLASS (klass);

    gobject_class->get_property = seahorse_gpgme_generate_dialog_get_property;
    gobject_class->set_property = seahorse_gpgme_generate_dialog_set_property;
    gobject_class->finalize = seahorse_gpgme_generate_dialog_finalize;

    obj_props[PROP_KEYRING] =
        g_param_spec_object ("keyring", "keyring",
                             "Keyring to use for generating",
                             SEAHORSE_TYPE_GPGME_KEYRING,
                             G_PARAM_READWRITE |
                             G_PARAM_CONSTRUCT_ONLY |
                             G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (gobject_class, N_PROPS, obj_props);

    gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Seahorse/seahorse-gpgme-generate-dialog.ui");
    gtk_widget_class_bind_template_child (widget_class, SeahorseGpgmeGenerateDialog, name_entry);
    gtk_widget_class_bind_template_child (widget_class, SeahorseGpgmeGenerateDialog, email_entry);
    gtk_widget_class_bind_template_child (widget_class, SeahorseGpgmeGenerateDialog, comment_entry);
    gtk_widget_class_bind_template_child (widget_class, SeahorseGpgmeGenerateDialog, algorithm_choice);
    gtk_widget_class_bind_template_child (widget_class, SeahorseGpgmeGenerateDialog, bits_entry);
    gtk_widget_class_bind_template_child (widget_class, SeahorseGpgmeGenerateDialog, expires_datepicker);
    gtk_widget_class_bind_template_child (widget_class, SeahorseGpgmeGenerateDialog, expires_check);
    gtk_widget_class_bind_template_callback (widget_class, on_gpgme_generate_entry_changed);
    gtk_widget_class_bind_template_callback (widget_class, on_gpgme_generate_expires_toggled);
    gtk_widget_class_bind_template_callback (widget_class, on_gpgme_generate_algorithm_changed);

    dialog_class->response = seahorse_gpgme_generate_dialog_response;
}

GtkDialog *
seahorse_gpgme_generate_dialog_new (SeahorseGpgmeKeyring *keyring,
                                    GtkWindow *parent)
{
    g_return_val_if_fail (SEAHORSE_IS_GPGME_KEYRING (keyring), NULL);
    g_return_val_if_fail (!parent || GTK_IS_WINDOW (parent), NULL);

    return g_object_new (SEAHORSE_GPGME_TYPE_GENERATE_DIALOG,
                         "keyring", keyring,
                         "transient-for", parent,
                         "use-header-bar", 1,
                         NULL);
}
