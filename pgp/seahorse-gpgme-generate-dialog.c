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
    AdwDialog parent_instance;

    SeahorseGpgmeKeyring *keyring;

    GtkWidget *name_row;
    GtkWidget *name_row_warning;
    GtkWidget *email_row;
    GtkWidget *comment_row;

    GtkWidget *algorithm_row;
    GtkWidget *bits_spin;

    GtkWidget *expires_switch;
    GtkWidget *expires_date_row;
    GtkWidget *expires_datepicker;
};

enum {
    PROP_0,
    PROP_KEYRING,
    N_PROPS
};
static GParamSpec *obj_props[N_PROPS] = { NULL, };

G_DEFINE_TYPE (SeahorseGpgmeGenerateDialog, seahorse_gpgme_generate_dialog, ADW_TYPE_DIALOG)

typedef struct _AlgorithmDesc {
    const char* desc;
    unsigned int type;
    unsigned int min;
    unsigned int max;
    unsigned int def;
} AlgorithmDesc;

static AlgorithmDesc available_algorithms[] = {
    { N_("RSA"),             SEAHORSE_PGP_KEY_ALGO_RSA_RSA,     RSA_MIN,     LENGTH_MAX, LENGTH_DEFAULT  },
    { N_("DSA ElGamal"),     SEAHORSE_PGP_KEY_ALGO_DSA_ELGAMAL, ELGAMAL_MIN, LENGTH_MAX, LENGTH_DEFAULT  },
    { N_("DSA (sign only)"), SEAHORSE_PGP_KEY_ALGO_DSA,         DSA_MIN,     DSA_MAX,    LENGTH_DEFAULT  },
    { N_("RSA (sign only)"), SEAHORSE_PGP_KEY_ALGO_RSA_SIGN,    RSA_MIN,     LENGTH_MAX, LENGTH_DEFAULT  }
};

static void
on_generate_key_complete (GObject *source,
                          GAsyncResult *result,
                          gpointer user_data)
{
    SeahorseCatalog *catalog = SEAHORSE_CATALOG (user_data);
    GError *error = NULL;

    if (!seahorse_gpgme_key_op_generate_finish (SEAHORSE_GPGME_KEYRING (source), result, &error)) {
        seahorse_util_show_error (GTK_WIDGET (catalog),
                                  _("Couldn’t generate PGP key"),
                                  error->message);
        return;
    }

    g_action_group_activate_action (G_ACTION_GROUP (catalog),
                                    "focus-place",
                                    g_variant_new_string ("gnupg"));
}

typedef struct _GenerateClosure {
    SeahorseGpgmeKeyring *keyring;
    char *name;
    char *email;
    char *comment;
    unsigned int type;
    unsigned int bits;
    GDateTime *expires;
    GtkWindow *parent;
} GenerateClosure;

static void
generate_closure_free (void *data)
{
    GenerateClosure *closure = data;
    g_clear_object (&closure->keyring);
    g_clear_pointer (&closure->name, g_free);
    g_clear_pointer (&closure->email, g_free);
    g_clear_pointer (&closure->comment, g_free);
    g_clear_object (&closure->expires);
    g_clear_object (&closure->parent);
}

static void
on_pass_prompted (GObject *source,
                  GAsyncResult *result,
                  gpointer user_data)
{
    SeahorsePassphrasePrompt *pdialog = SEAHORSE_PASSPHRASE_PROMPT (source);
    GenerateClosure *closure = user_data;
    g_autofree char *pass = NULL;
    g_autoptr(GCancellable) cancellable = NULL;
    const char *notice;

    pass = seahorse_passphrase_prompt_prompt_finish (pdialog, result);
    if (pass != NULL) {
        cancellable = g_cancellable_new ();
        seahorse_gpgme_key_op_generate_async (closure->keyring,
                                              closure->name,
                                              closure->email,
                                              closure->comment,
                                              pass,
                                              closure->type,
                                              closure->bits,
                                              closure->expires,
                                              cancellable, on_generate_key_complete,
                                              closure->parent);

        /* Has line breaks because GtkLabel is completely broken WRT wrapping */
        notice = _("When creating a key we need to generate a lot of\n"
                   "random data and we need you to help. It’s a good\n"
                   "idea to perform some other action like typing on\n"
                   "the keyboard, moving the mouse, using applications.\n"
                   "This gives the system the random data that it needs.");
        seahorse_progress_show_with_notice (cancellable, _("Generating key"), notice, FALSE);
    }

    adw_dialog_close (ADW_DIALOG (pdialog));
    generate_closure_free (closure);
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
    SeahorsePassphrasePrompt *dialog;
    GenerateClosure *closure;

    dialog = seahorse_passphrase_prompt_new (_("Passphrase for New PGP Key"),
                                             _("Enter the passphrase for your new key twice."),
                                             NULL, NULL, TRUE);

    closure = g_new0 (GenerateClosure, 1);
    closure->keyring = g_object_ref (keyring);
    closure->name = g_strdup (name);
    closure->email = g_strdup (email);
    closure->comment = g_strdup (comment);
    closure->type = type;
    closure->bits = bits;
    closure->expires = expires? g_object_ref (expires) : NULL;
    closure->parent = parent? g_object_ref (parent) : NULL;

    seahorse_passphrase_prompt_prompt (dialog, GTK_WIDGET (parent), NULL, on_pass_prompted, closure);
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
    name = g_strdup (gtk_editable_get_text (GTK_EDITABLE (self->name_row)));
    name_long_enough = name && strlen (g_strstrip (name)) >= 5;

    /* If not, show the user and disable the create button */
    gtk_widget_set_visible (self->name_row_warning, !name_long_enough);
    gtk_widget_action_set_enabled (GTK_WIDGET (self), "create-key", name_long_enough);
}

/* Changes the bit range depending on the algorithm set */
static void
on_algo_row_notify_selected (GObject    *object,
                             GParamSpec *pspec,
                             void       *user_data)
{
    SeahorseGpgmeGenerateDialog *self = SEAHORSE_GPGME_GENERATE_DIALOG (user_data);
    unsigned int sel;

    sel = adw_combo_row_get_selected (ADW_COMBO_ROW (self->algorithm_row));
    g_assert (sel < G_N_ELEMENTS (available_algorithms));

    gtk_spin_button_set_range (GTK_SPIN_BUTTON (self->bits_spin),
                               available_algorithms[sel].min,
                               available_algorithms[sel].max);

    /* Set sane default key length */
    if (available_algorithms[sel].def > available_algorithms[sel].max)
        gtk_spin_button_set_value (GTK_SPIN_BUTTON (self->bits_spin), available_algorithms[sel].max);
    else
        gtk_spin_button_set_value (GTK_SPIN_BUTTON (self->bits_spin), available_algorithms[sel].def);
}

static void
create_key_action (GtkWidget *widget, const char *action_name, GVariant *param)
{
    SeahorseGpgmeGenerateDialog *self = SEAHORSE_GPGME_GENERATE_DIALOG (widget);
    g_autofree char *name = NULL;
    const char *email, *comment;
    unsigned int sel;
    unsigned int type;
    g_autoptr(GDateTime) expires = NULL;
    unsigned int bits;

    /* Make sure the name is the right length. Should've been checked earlier */
    name = g_strdup (gtk_editable_get_text (GTK_EDITABLE (self->name_row)));
    g_return_if_fail (name);
    name = g_strstrip (name);
    g_return_if_fail (strlen(name) >= 5);

    email = gtk_editable_get_text (GTK_EDITABLE (self->email_row));
    comment = gtk_editable_get_text (GTK_EDITABLE (self->comment_row));

    /* The algorithm */
    sel = adw_combo_row_get_selected (ADW_COMBO_ROW (self->algorithm_row));
    g_assert (sel <= (int) G_N_ELEMENTS(available_algorithms));
    type = available_algorithms[sel].type;

    /* The number of bits */
    bits = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (self->bits_spin));
    if (bits < available_algorithms[sel].min || bits > available_algorithms[sel].max) {
        bits = available_algorithms[sel].def;
        g_message ("invalid key size: %s defaulting to %u", available_algorithms[sel].desc, bits);
    }

    /* The expiry */
    if (!gtk_switch_get_active (GTK_SWITCH (self->expires_switch)))
        expires = seahorse_date_picker_get_datetime (SEAHORSE_DATE_PICKER (self->expires_datepicker));

    seahorse_gpgme_generate_key (self->keyring,
                                 name, email, comment, type, bits, expires,
                                 gtk_window_get_transient_for (GTK_WINDOW (self)));

    adw_dialog_close (ADW_DIALOG (self));
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
        g_set_object (&self->keyring, g_value_get_object (value));
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
    g_autoptr(GDateTime) now = NULL;
    g_autoptr(GDateTime) next_year = NULL;
    g_autoptr(GtkStringList) algos = NULL;

    gtk_widget_init_template (GTK_WIDGET (self));

    /* The algorithms */
    algos = gtk_string_list_new (NULL);
    for (guint i = 0; i < G_N_ELEMENTS (available_algorithms); i++)
        gtk_string_list_append (algos, _(available_algorithms[i].desc));
    adw_combo_row_set_model (ADW_COMBO_ROW (self->algorithm_row),
                             G_LIST_MODEL (algos));
    on_algo_row_notify_selected (G_OBJECT (self->algorithm_row), NULL, self);

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

    gobject_class->get_property = seahorse_gpgme_generate_dialog_get_property;
    gobject_class->set_property = seahorse_gpgme_generate_dialog_set_property;
    gobject_class->finalize = seahorse_gpgme_generate_dialog_finalize;

    obj_props[PROP_KEYRING] =
        g_param_spec_object ("keyring", NULL, NULL,
                             SEAHORSE_TYPE_GPGME_KEYRING,
                             G_PARAM_READWRITE |
                             G_PARAM_CONSTRUCT_ONLY |
                             G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (gobject_class, N_PROPS, obj_props);

    gtk_widget_class_install_action (widget_class, "create-key", NULL, create_key_action);

    gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Seahorse/seahorse-gpgme-generate-dialog.ui");
    gtk_widget_class_bind_template_child (widget_class, SeahorseGpgmeGenerateDialog, name_row);
    gtk_widget_class_bind_template_child (widget_class, SeahorseGpgmeGenerateDialog, name_row_warning);
    gtk_widget_class_bind_template_child (widget_class, SeahorseGpgmeGenerateDialog, email_row);
    gtk_widget_class_bind_template_child (widget_class, SeahorseGpgmeGenerateDialog, comment_row);
    gtk_widget_class_bind_template_child (widget_class, SeahorseGpgmeGenerateDialog, algorithm_row);
    gtk_widget_class_bind_template_child (widget_class, SeahorseGpgmeGenerateDialog, bits_spin);
    gtk_widget_class_bind_template_child (widget_class, SeahorseGpgmeGenerateDialog, expires_date_row);
    gtk_widget_class_bind_template_child (widget_class, SeahorseGpgmeGenerateDialog, expires_datepicker);
    gtk_widget_class_bind_template_child (widget_class, SeahorseGpgmeGenerateDialog, expires_switch);
    gtk_widget_class_bind_template_callback (widget_class, on_gpgme_generate_entry_changed);
    gtk_widget_class_bind_template_callback (widget_class, on_algo_row_notify_selected);
}

SeahorseGpgmeGenerateDialog *
seahorse_gpgme_generate_dialog_new (SeahorseGpgmeKeyring *keyring)
{
    g_return_val_if_fail (SEAHORSE_IS_GPGME_KEYRING (keyring), NULL);

    return g_object_new (SEAHORSE_GPGME_TYPE_GENERATE_DIALOG,
                         "keyring", keyring,
                         NULL);
}
