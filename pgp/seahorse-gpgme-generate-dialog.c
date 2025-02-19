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
#include "seahorse-gpgme-key-parms.h"
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

    SeahorseGpgmeKeyParms *parms;

    GtkWidget *name_row;
    GtkWidget *name_row_warning;
    GtkWidget *email_row;
    GtkWidget *comment_row;

    GtkWidget *algorithm_row;
    GtkCustomFilter *algo_filter;
    GtkAdjustment *bits_spin_adjustment;

    GtkWidget *expires_switch;
    GtkWidget *expires_date_row;
    GtkWidget *expires_datepicker;
    GBinding *expires_binding;
};

enum {
    PROP_0,
    PROP_KEYRING,
    N_PROPS
};
static GParamSpec *obj_props[N_PROPS] = { NULL, };

G_DEFINE_TYPE (SeahorseGpgmeGenerateDialog, seahorse_gpgme_generate_dialog, ADW_TYPE_DIALOG)

static GtkWindow *
get_toplevel (SeahorseGpgmeGenerateDialog *self)
{
    GtkRoot *root;

    root = gtk_widget_get_root (GTK_WIDGET (self));
    return GTK_IS_WINDOW (root)? GTK_WINDOW (root) : NULL;
}

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
    SeahorseGpgmeKeyParms *parms;
    GtkWindow *parent;
} GenerateClosure;

static void
generate_closure_free (void *data)
{
    GenerateClosure *closure = data;
    g_clear_object (&closure->keyring);
    g_clear_object (&closure->parms);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GenerateClosure, generate_closure_free);

static void
on_pass_prompted (GObject *source,
                  GAsyncResult *result,
                  gpointer user_data)
{
    SeahorsePassphrasePrompt *pdialog = SEAHORSE_PASSPHRASE_PROMPT (source);
    g_autoptr(GenerateClosure) closure = user_data;
    g_autofree char *pass = NULL;
    g_autoptr(GCancellable) cancellable = NULL;
    const char *notice;

    pass = seahorse_passphrase_prompt_prompt_finish (pdialog, result);
    if (pass != NULL) {
        cancellable = g_cancellable_new ();
        seahorse_gpgme_key_parms_set_passphrase (closure->parms, pass);
        seahorse_gpgme_key_op_generate_async (closure->keyring,
                                              closure->parms,
                                              cancellable,
                                              on_generate_key_complete,
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
}

/* Changes the bit range depending on the algorithm set */
static void
on_algo_row_notify_selected (GObject    *object,
                             GParamSpec *pspec,
                             void       *user_data)
{
    SeahorseGpgmeGenerateDialog *self = SEAHORSE_GPGME_GENERATE_DIALOG (user_data);
    GObject *selected_item;
    SeahorseGpgmeKeyGenType sel;
    SeahorsePgpKeyAlgorithm algo;
    unsigned int default_val, lower, upper;

    selected_item = adw_combo_row_get_selected_item (ADW_COMBO_ROW (self->algorithm_row));
    sel = adw_enum_list_item_get_value (ADW_ENUM_LIST_ITEM (selected_item));
    seahorse_gpgme_key_parms_set_key_type (self->parms, sel);

    algo = seahorse_gpgme_key_gen_type_get_key_algo (sel);
    if (seahorse_pgp_key_algorithm_get_length_values (algo, &default_val, &lower, &upper)) {
        gtk_adjustment_configure (self->bits_spin_adjustment,
                                  default_val,
                                  lower,
                                  upper,
                                  512,
                                  0,
                                  0);
    }
}

static void
on_expires_switch_notify_active (GObject    *object,
                                 GParamSpec *pspec,
                                 void       *user_data)
{
    SeahorseGpgmeGenerateDialog *self = SEAHORSE_GPGME_GENERATE_DIALOG (user_data);

    if (gtk_switch_get_active (GTK_SWITCH (self->expires_switch))) {
        self->expires_binding =
            g_object_bind_property (self->expires_datepicker, "datetime",
                                    self->parms, "expires",
                                    G_BINDING_BIDIRECTIONAL |
                                    G_BINDING_SYNC_CREATE);
    } else {
        g_clear_object (&self->expires_binding);
        seahorse_gpgme_key_parms_set_expires (self->parms, NULL);
    }
}

static void
create_key_action (GtkWidget *widget, const char *action_name, GVariant *param)
{
    SeahorseGpgmeGenerateDialog *self = SEAHORSE_GPGME_GENERATE_DIALOG (widget);
    SeahorsePassphrasePrompt *dialog;
    GenerateClosure *closure;

    dialog = seahorse_passphrase_prompt_new (_("Passphrase for New PGP Key"),
                                             _("Enter the passphrase for your new key twice."),
                                             NULL, NULL, TRUE);

    closure = g_new0 (GenerateClosure, 1);
    closure->keyring = g_object_ref (self->keyring);
    closure->parms = g_object_ref (self->parms);
    closure->parent = get_toplevel (self);

    seahorse_passphrase_prompt_prompt (dialog, GTK_WIDGET (closure->parent), NULL, on_pass_prompted, closure);

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
seahorse_gpgme_generate_dialog_dispose (GObject *obj)
{
    SeahorseGpgmeGenerateDialog *self = SEAHORSE_GPGME_GENERATE_DIALOG (obj);

    gtk_widget_dispose_template (GTK_WIDGET (self), SEAHORSE_GPGME_TYPE_GENERATE_DIALOG);

    G_OBJECT_CLASS (seahorse_gpgme_generate_dialog_parent_class)->dispose (obj);
}

static void
seahorse_gpgme_generate_dialog_finalize (GObject *obj)
{
    SeahorseGpgmeGenerateDialog *self = SEAHORSE_GPGME_GENERATE_DIALOG (obj);

    g_clear_object (&self->expires_binding);
    g_clear_object (&self->parms);
    g_clear_object (&self->keyring);

    G_OBJECT_CLASS (seahorse_gpgme_generate_dialog_parent_class)->finalize (obj);
}

static void
on_parms_is_valid_changed (GObject    *object,
                           GParamSpec *pspec,
                           void       *user_data)
{
    SeahorseGpgmeGenerateDialog *self = SEAHORSE_GPGME_GENERATE_DIALOG (user_data);
    SeahorseGpgmeKeyParms *parms = SEAHORSE_GPGME_KEY_PARMS (object);

    gtk_widget_action_set_enabled (GTK_WIDGET (self),
                                   "create-key",
                                   seahorse_gpgme_key_parms_is_valid (parms));

    gtk_widget_set_visible (self->name_row_warning,
                            !seahorse_gpgme_key_parms_has_valid_name (parms));
}

static gboolean
filter_enums (void *item, void *user_data)
{
    AdwEnumListItem *enum_item = ADW_ENUM_LIST_ITEM (item);

    switch (adw_enum_list_item_get_value (enum_item)) {
        case SEAHORSE_GPGME_KEY_GEN_TYPE_RSA_RSA:
        case SEAHORSE_GPGME_KEY_GEN_TYPE_DSA_ELGAMAL:
        case SEAHORSE_GPGME_KEY_GEN_TYPE_DSA:
        case SEAHORSE_GPGME_KEY_GEN_TYPE_RSA_SIGN:
            return TRUE;
    }

    return FALSE;
}

static char *
algo_to_string (void                    *user_data,
                SeahorseGpgmeKeyGenType  algo)
{
    const char *str;

    str = seahorse_gpgme_key_gen_type_to_string (algo);
    g_return_val_if_fail (str != NULL, NULL);
    return g_strdup (str);
}

static void
seahorse_gpgme_generate_dialog_init (SeahorseGpgmeGenerateDialog *self)
{
    g_autoptr(GDateTime) now = NULL;
    g_autoptr(GDateTime) next_year = NULL;
    g_autoptr(GtkStringList) algos = NULL;

    gtk_widget_init_template (GTK_WIDGET (self));

    gtk_custom_filter_set_filter_func (self->algo_filter, filter_enums, self, NULL);

    self->parms = seahorse_gpgme_key_parms_new ();
    g_object_bind_property (self->parms, "name",
                            self->name_row, "text",
                            G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
    g_object_bind_property (self->parms, "email",
                            self->email_row, "text",
                            G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
    g_object_bind_property (self->parms, "comment",
                            self->comment_row, "text",
                            G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
    g_signal_connect_object (self->parms, "notify::is-valid",
                             G_CALLBACK (on_parms_is_valid_changed),
                             self,
                             G_CONNECT_DEFAULT);
    on_parms_is_valid_changed (G_OBJECT (self->parms), NULL, self);

    on_algo_row_notify_selected (G_OBJECT (self->algorithm_row), NULL, self);

    /* Default expiry date */
    now = g_date_time_new_now_utc ();
    next_year = g_date_time_add_years (now, 1);
    seahorse_date_picker_set_datetime (SEAHORSE_DATE_PICKER (self->expires_datepicker),
                                       next_year);
}

static void
seahorse_gpgme_generate_dialog_class_init (SeahorseGpgmeGenerateDialogClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    gobject_class->get_property = seahorse_gpgme_generate_dialog_get_property;
    gobject_class->set_property = seahorse_gpgme_generate_dialog_set_property;
    gobject_class->dispose = seahorse_gpgme_generate_dialog_dispose;
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
    gtk_widget_class_bind_template_child (widget_class, SeahorseGpgmeGenerateDialog, algo_filter);
    gtk_widget_class_bind_template_child (widget_class, SeahorseGpgmeGenerateDialog, bits_spin_adjustment);
    gtk_widget_class_bind_template_child (widget_class, SeahorseGpgmeGenerateDialog, expires_date_row);
    gtk_widget_class_bind_template_child (widget_class, SeahorseGpgmeGenerateDialog, expires_datepicker);
    gtk_widget_class_bind_template_child (widget_class, SeahorseGpgmeGenerateDialog, expires_switch);
    gtk_widget_class_bind_template_callback (widget_class, algo_to_string);
    gtk_widget_class_bind_template_callback (widget_class, on_algo_row_notify_selected);
    gtk_widget_class_bind_template_callback (widget_class, on_expires_switch_notify_active);
}

SeahorseGpgmeGenerateDialog *
seahorse_gpgme_generate_dialog_new (SeahorseGpgmeKeyring *keyring)
{
    g_return_val_if_fail (SEAHORSE_IS_GPGME_KEYRING (keyring), NULL);

    return g_object_new (SEAHORSE_GPGME_TYPE_GENERATE_DIALOG,
                         "keyring", keyring,
                         NULL);
}
