/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
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

#include "seahorse-gpgme-revoke-dialog.h"
#include "seahorse-gpgme-key-op.h"
#include "seahorse-pgp-dialogs.h"
#include "seahorse-pgp-enums.h"

#include "libseahorse/seahorse-util.h"

#include <glib/gi18n.h>

#include <string.h>

struct _SeahorseGpgmeRevokeDialog {
    GtkApplicationWindow parent_instance;

    SeahorseGpgmeSubkey *subkey;

    GtkWidget *reason_row;
    GtkWidget *description_row;
};

enum {
    PROP_0,
    PROP_SUBKEY,
    N_PROPS
};
static GParamSpec *obj_props[N_PROPS] = { NULL, };

G_DEFINE_TYPE (SeahorseGpgmeRevokeDialog, seahorse_gpgme_revoke_dialog, GTK_TYPE_APPLICATION_WINDOW)

static SeahorsePgpRevokeReason
get_selected_revoke_reason (SeahorseGpgmeRevokeDialog *self)
{
    AdwComboRow *reason_row;
    GObject *selected_item;

    g_return_val_if_fail (SEAHORSE_GPGME_IS_REVOKE_DIALOG (self), 0);

    reason_row = ADW_COMBO_ROW (self->reason_row);
    selected_item = adw_combo_row_get_selected_item (reason_row);
    return adw_enum_list_item_get_value (ADW_ENUM_LIST_ITEM (selected_item));
}

static void
revoke_action (GtkWidget *widget, const char *action_name, GVariant *param)
{
    SeahorseGpgmeRevokeDialog *self = SEAHORSE_GPGME_REVOKE_DIALOG (widget);
    SeahorsePgpRevokeReason reason;
    const char *description;
    gpgme_error_t err;
    GObject *item;

    item = adw_combo_row_get_selected_item (ADW_COMBO_ROW (self->reason_row));
    reason = adw_enum_list_item_get_value (ADW_ENUM_LIST_ITEM (item));

    description = gtk_editable_get_text (GTK_EDITABLE (self->description_row));

    err = seahorse_gpgme_key_op_revoke_subkey (self->subkey, reason, description);
    if (!GPG_IS_OK (err))
        seahorse_gpgme_handle_error (err, _("Couldn’t revoke subkey"));

    gtk_window_close (GTK_WINDOW (self));
}

static char *
reason_to_string (void                   *user_data,
                SeahorsePgpRevokeReason reason)
{
    switch (reason) {
        case SEAHORSE_PGP_REVOKE_REASON_NONE:
            return g_strdup (_("No reason"));
        case SEAHORSE_PGP_REVOKE_REASON_COMPROMISED:
            return g_strdup (_("Compromised"));
        case SEAHORSE_PGP_REVOKE_REASON_SUPERSEDED:
            return g_strdup (_("Superseded"));
        case SEAHORSE_PGP_REVOKE_REASON_NOT_USED:
            return g_strdup (_("Not Used"));
    }

    g_return_val_if_reached (NULL);
}

static void
reason_row_notify_selected (GObject *object, GParamSpec *pspec, void *user_data)
{
    SeahorseGpgmeRevokeDialog *self = SEAHORSE_GPGME_REVOKE_DIALOG (user_data);
    const char *subtitle;

    switch (get_selected_revoke_reason (self)) {
        case SEAHORSE_PGP_REVOKE_REASON_NONE:
            subtitle = _("No reason for revoking key");
            break;
        case SEAHORSE_PGP_REVOKE_REASON_COMPROMISED:
            subtitle = _("Key has been compromised");
            break;
        case SEAHORSE_PGP_REVOKE_REASON_SUPERSEDED:
            subtitle = _("Key has been superseded");
            break;
        case SEAHORSE_PGP_REVOKE_REASON_NOT_USED:
            subtitle = _("Key is no longer used");
            break;
    }

    adw_action_row_set_subtitle (ADW_ACTION_ROW (self->reason_row), subtitle);
}

static void
seahorse_gpgme_revoke_dialog_get_property (GObject *object,
                                           guint prop_id,
                                           GValue *value,
                                           GParamSpec *pspec)
{
    SeahorseGpgmeRevokeDialog *self = SEAHORSE_GPGME_REVOKE_DIALOG (object);

    switch (prop_id) {
    case PROP_SUBKEY:
        g_value_set_object (value, self->subkey);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
seahorse_gpgme_revoke_dialog_set_property (GObject *object,
                                           guint prop_id,
                                           const GValue *value,
                                           GParamSpec *pspec)
{
    SeahorseGpgmeRevokeDialog *self = SEAHORSE_GPGME_REVOKE_DIALOG (object);

    switch (prop_id) {
    case PROP_SUBKEY:
        g_clear_object (&self->subkey);
        self->subkey = g_value_dup_object (value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
seahorse_gpgme_revoke_dialog_finalize (GObject *obj)
{
    SeahorseGpgmeRevokeDialog *self = SEAHORSE_GPGME_REVOKE_DIALOG (obj);

    g_clear_object (&self->subkey);

    G_OBJECT_CLASS (seahorse_gpgme_revoke_dialog_parent_class)->finalize (obj);
}

static void
seahorse_gpgme_revoke_dialog_constructed (GObject *obj)
{
    SeahorseGpgmeRevokeDialog *self = SEAHORSE_GPGME_REVOKE_DIALOG (obj);
    g_autofree char *title = NULL;
    const char *label;

    G_OBJECT_CLASS (seahorse_gpgme_revoke_dialog_parent_class)->constructed (obj);

    label = seahorse_pgp_subkey_get_description (SEAHORSE_PGP_SUBKEY (self->subkey));
    title = g_strdup_printf (_("Revoke: %s"), label);
    gtk_window_set_title (GTK_WINDOW (self), title);
}

static void
seahorse_gpgme_revoke_dialog_init (SeahorseGpgmeRevokeDialog *self)
{
    gtk_widget_init_template (GTK_WIDGET (self));

    adw_combo_row_set_selected (ADW_COMBO_ROW (self->reason_row), 0);
    reason_row_notify_selected (G_OBJECT (self->reason_row), NULL, self);
}

static void
seahorse_gpgme_revoke_dialog_class_init (SeahorseGpgmeRevokeDialogClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    gobject_class->constructed = seahorse_gpgme_revoke_dialog_constructed;
    gobject_class->get_property = seahorse_gpgme_revoke_dialog_get_property;
    gobject_class->set_property = seahorse_gpgme_revoke_dialog_set_property;
    gobject_class->finalize = seahorse_gpgme_revoke_dialog_finalize;

    obj_props[PROP_SUBKEY] =
        g_param_spec_object ("subkey", "Subkey to revoke",
                             "The GPGME subkey you want to revoke",
                             SEAHORSE_GPGME_TYPE_SUBKEY,
                             G_PARAM_READWRITE |
                             G_PARAM_CONSTRUCT_ONLY |
                             G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (gobject_class, N_PROPS, obj_props);

    gtk_widget_class_install_action (widget_class, "revoke", NULL, revoke_action);

    gtk_widget_class_set_template_from_resource (widget_class,
                                                 "/org/gnome/Seahorse/seahorse-gpgme-revoke-dialog.ui");
    gtk_widget_class_bind_template_child (widget_class,
                                          SeahorseGpgmeRevokeDialog,
                                          reason_row);
    gtk_widget_class_bind_template_child (widget_class,
                                          SeahorseGpgmeRevokeDialog,
                                          description_row);
    gtk_widget_class_bind_template_callback (widget_class, reason_to_string);
    gtk_widget_class_bind_template_callback (widget_class,
                                             reason_row_notify_selected);
}

GtkWidget *
seahorse_gpgme_revoke_dialog_new (SeahorseGpgmeSubkey *subkey,
                                  GtkWindow           *parent)
{
    g_return_val_if_fail (SEAHORSE_GPGME_IS_SUBKEY (subkey), NULL);
    g_return_val_if_fail (!parent || GTK_IS_WINDOW (parent), NULL);

    return g_object_new (SEAHORSE_GPGME_TYPE_REVOKE_DIALOG,
                         "subkey", subkey,
                         "transient-for", parent,
                         NULL);
}
