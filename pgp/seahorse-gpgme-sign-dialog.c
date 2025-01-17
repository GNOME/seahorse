/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
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
 * along with this program; if not, see
 * <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "seahorse-gpgme-sign-dialog.h"
#include "seahorse-gpgme-key-op.h"
#include "seahorse-pgp-keysets.h"

#include "seahorse-common.h"

#include "libseahorse/seahorse-util.h"

#include <glib/gi18n.h>

struct _SeahorseGpgmeSignDialog {
    GtkApplicationWindow parent_instance;

    SeahorseItem *to_sign;

    GtkWidget *to_sign_name_label;

    GtkWidget *sign_display_not;
    GtkWidget *sign_display_casual;
    GtkWidget *sign_display_careful;

    GtkWidget *sign_choice_not;
    GtkWidget *sign_choice_casual;
    GtkWidget *sign_choice_careful;

    GtkWidget *sign_option_local;
    GtkWidget *sign_option_revocable;

    GtkWidget *signer_row;
    GtkWidget *signer_frame;
};

enum {
    PROP_0,
    PROP_TO_SIGN,
    N_PROPS
};
static GParamSpec *obj_props[N_PROPS] = { NULL, };

G_DEFINE_TYPE (SeahorseGpgmeSignDialog, seahorse_gpgme_sign_dialog, GTK_TYPE_APPLICATION_WINDOW)


static void
on_collection_changed (GListModel *model,
                       unsigned int position,
                       unsigned int removed,
                       unsigned int added,
                       gpointer user_data)
{
    SeahorseGpgmeSignDialog *self = SEAHORSE_GPGME_SIGN_DIALOG (user_data);

    gtk_widget_set_visible (self->signer_frame,
                            g_list_model_get_n_items (model) > 1);
}

static void
on_gpgme_sign_choice_toggled (GtkToggleButton *toggle,
                              gpointer user_data)
{
    SeahorseGpgmeSignDialog *self = SEAHORSE_GPGME_SIGN_DIALOG (user_data);

    /* Figure out choice */
    gtk_widget_set_visible (self->sign_display_not,
                            gtk_check_button_get_active (GTK_CHECK_BUTTON (self->sign_choice_not)));

    gtk_widget_set_visible (self->sign_display_casual,
                            gtk_check_button_get_active (GTK_CHECK_BUTTON (self->sign_choice_casual)));

    gtk_widget_set_visible (self->sign_display_careful,
                            gtk_check_button_get_active (GTK_CHECK_BUTTON (self->sign_choice_careful)));
}

static void
sign_action (GtkWidget *widget, const char *action_name, GVariant *param)
{
    SeahorseGpgmeSignDialog *self = SEAHORSE_GPGME_SIGN_DIALOG (widget);
    SeahorseSignCheck check;
    SeahorseSignOptions options = 0;
    SeahorsePgpKey *signer;
    gpgme_error_t err;

    /* Figure out choice */
    check = SIGN_CHECK_NO_ANSWER;
    if (gtk_check_button_get_active (GTK_CHECK_BUTTON (self->sign_choice_not)))
        check = SIGN_CHECK_NONE;
    else {
        if (gtk_check_button_get_active (GTK_CHECK_BUTTON (self->sign_choice_casual)))
            check = SIGN_CHECK_CASUAL;
        else {
            if (gtk_check_button_get_active (GTK_CHECK_BUTTON (self->sign_choice_careful)))
                check = SIGN_CHECK_CAREFUL;
        }
    }

    /* Local signature */
    if (gtk_switch_get_active (GTK_SWITCH (self->sign_option_local)))
        options |= SIGN_LOCAL;

    /* Revocable signature */
    if (!gtk_switch_get_active (GTK_SWITCH (self->sign_option_revocable)))
        options |= SIGN_NO_REVOKE;

    /* Signer */
    signer = adw_combo_row_get_selected_item (ADW_COMBO_ROW (self->signer_row));

    g_assert (!signer || (SEAHORSE_GPGME_IS_KEY (signer) &&
                          seahorse_pgp_key_is_private_key (signer)));

    if (SEAHORSE_GPGME_IS_UID (self->to_sign))
        err = seahorse_gpgme_key_op_sign_uid (SEAHORSE_GPGME_UID (self->to_sign), SEAHORSE_GPGME_KEY (signer), check, options);
    else if (SEAHORSE_GPGME_IS_KEY (self->to_sign))
        err = seahorse_gpgme_key_op_sign (SEAHORSE_GPGME_KEY (self->to_sign), SEAHORSE_GPGME_KEY (signer), check, options);
    else
        g_assert (FALSE);


    if (!GPG_IS_OK (err)) {
        if (gpgme_err_code (err) == GPG_ERR_EALREADY) {
            AdwDialog *dialog;
            const char *title;

            title = seahorse_item_get_title (SEAHORSE_ITEM (signer));
            dialog = adw_alert_dialog_new (NULL, NULL);
            adw_alert_dialog_format_body (ADW_ALERT_DIALOG (dialog),
                                          _("This key was already signed by\n“%s”"),
                                          title);
            adw_dialog_present (ADW_DIALOG (dialog), GTK_WIDGET (self));
        } else
            seahorse_gpgme_handle_error (err, _("Couldn’t sign key"));
    }

    gtk_window_close (GTK_WINDOW (self));
}

static void
seahorse_gpgme_sign_dialog_get_property (GObject *object,
                                         guint prop_id,
                                         GValue *value,
                                         GParamSpec *pspec)
{
    SeahorseGpgmeSignDialog *self = SEAHORSE_GPGME_SIGN_DIALOG (object);

    switch (prop_id) {
    case PROP_TO_SIGN:
        g_value_set_object (value, self->to_sign);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
seahorse_gpgme_sign_dialog_set_property (GObject *object,
                                         guint prop_id,
                                         const GValue *value,
                                         GParamSpec *pspec)
{
    SeahorseGpgmeSignDialog *self = SEAHORSE_GPGME_SIGN_DIALOG (object);

    switch (prop_id) {
    case PROP_TO_SIGN:
        g_clear_object (&self->to_sign);
        self->to_sign = g_value_dup_object (value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
seahorse_gpgme_sign_dialog_finalize (GObject *obj)
{
    SeahorseGpgmeSignDialog *self = SEAHORSE_GPGME_SIGN_DIALOG (obj);

    g_clear_object (&self->to_sign);

    G_OBJECT_CLASS (seahorse_gpgme_sign_dialog_parent_class)->finalize (obj);
}

static void
seahorse_gpgme_sign_dialog_constructed (GObject *obj)
{
    SeahorseGpgmeSignDialog *self = SEAHORSE_GPGME_SIGN_DIALOG (obj);

    G_OBJECT_CLASS (seahorse_gpgme_sign_dialog_parent_class)->constructed (obj);

    gtk_label_set_text (GTK_LABEL (self->to_sign_name_label),
                        seahorse_item_get_title (self->to_sign));

    /* Initial choice */
    on_gpgme_sign_choice_toggled (NULL, self);

    /* Other question's default state */
    gtk_switch_set_active (GTK_SWITCH (self->sign_option_local), FALSE);
    gtk_switch_set_active (GTK_SWITCH (self->sign_option_revocable), TRUE);
}

static void
seahorse_gpgme_sign_dialog_init (SeahorseGpgmeSignDialog *self)
{
    gtk_widget_init_template (GTK_WIDGET (self));
}

static void
seahorse_gpgme_sign_dialog_class_init (SeahorseGpgmeSignDialogClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    gobject_class->constructed = seahorse_gpgme_sign_dialog_constructed;
    gobject_class->get_property = seahorse_gpgme_sign_dialog_get_property;
    gobject_class->set_property = seahorse_gpgme_sign_dialog_set_property;
    gobject_class->finalize = seahorse_gpgme_sign_dialog_finalize;

    obj_props[PROP_TO_SIGN] =
        g_param_spec_object ("to-sign", "Item to be signed",
                             "The GPGME key or uid you want to sign",
                             SEAHORSE_TYPE_ITEM,
                             G_PARAM_READWRITE |
                             G_PARAM_CONSTRUCT_ONLY |
                             G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (gobject_class, N_PROPS, obj_props);

    gtk_widget_class_install_action (widget_class, "sign", NULL, sign_action);

    gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Seahorse/seahorse-gpgme-sign-dialog.ui");
    gtk_widget_class_bind_template_child (widget_class, SeahorseGpgmeSignDialog, to_sign_name_label);
    gtk_widget_class_bind_template_child (widget_class, SeahorseGpgmeSignDialog, sign_display_not);
    gtk_widget_class_bind_template_child (widget_class, SeahorseGpgmeSignDialog, sign_display_casual);
    gtk_widget_class_bind_template_child (widget_class, SeahorseGpgmeSignDialog, sign_display_careful);
    gtk_widget_class_bind_template_child (widget_class, SeahorseGpgmeSignDialog, sign_choice_not);
    gtk_widget_class_bind_template_child (widget_class, SeahorseGpgmeSignDialog, sign_choice_casual);
    gtk_widget_class_bind_template_child (widget_class, SeahorseGpgmeSignDialog, sign_choice_careful);
    gtk_widget_class_bind_template_child (widget_class, SeahorseGpgmeSignDialog, sign_option_local);
    gtk_widget_class_bind_template_child (widget_class, SeahorseGpgmeSignDialog, sign_option_revocable);
    gtk_widget_class_bind_template_child (widget_class, SeahorseGpgmeSignDialog, signer_frame);
    gtk_widget_class_bind_template_child (widget_class, SeahorseGpgmeSignDialog, signer_row);
    gtk_widget_class_bind_template_callback (widget_class, on_gpgme_sign_choice_toggled);
}

SeahorseGpgmeSignDialog *
seahorse_gpgme_sign_dialog_new (SeahorseItem *to_sign)
{
    g_autoptr(SeahorseGpgmeSignDialog) self = NULL;
    g_autoptr(GListModel) model = NULL;

    g_return_val_if_fail (SEAHORSE_GPGME_IS_KEY (to_sign) ||
                          SEAHORSE_GPGME_IS_UID (to_sign), NULL);

    /* If no signing keys then we can't sign */
    model = seahorse_keyset_pgp_signers_new ();
    if (g_list_model_get_n_items (model) == 0) {
        /* TODO: We should be giving an error message that allows them to
           generate or import a key */
        seahorse_util_show_error (NULL, _("No keys usable for signing"),
                _("You have no personal PGP keys that can be used to indicate your trust of this key."));
        return NULL;
    }

    self = g_object_new (SEAHORSE_GPGME_TYPE_SIGN_DIALOG,
                         "to-sign", to_sign,
                         NULL);

    /* Signature area */
    g_signal_connect_object (model, "items-changed",
                             G_CALLBACK (on_collection_changed), self, 0);
    on_collection_changed (model, 0, 0, 0, self);

    /* Signer box */
    adw_combo_row_set_model (ADW_COMBO_ROW (self->signer_row), model);

    return g_steal_pointer (&self);
}
