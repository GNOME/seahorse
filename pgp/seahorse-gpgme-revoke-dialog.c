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

#include "libseahorse/seahorse-util.h"

#include <glib/gi18n.h>

#include <string.h>

enum {
  COLUMN_TEXT,
  COLUMN_TOOLTIP,
  COLUMN_INT,
  N_COLUMNS
};

struct _SeahorseGpgmeRevokeDialog {
    GtkDialog parent_instance;

    SeahorseGpgmeSubkey *subkey;

    GtkWidget *reason_combo;
    GtkWidget *description_entry;
};

enum {
    PROP_0,
    PROP_SUBKEY,
    N_PROPS
};
static GParamSpec *obj_props[N_PROPS] = { NULL, };

G_DEFINE_TYPE (SeahorseGpgmeRevokeDialog, seahorse_gpgme_revoke_dialog, GTK_TYPE_DIALOG)


static void
on_gpgme_revoke_ok_clicked (GtkButton *button,
                            gpointer user_data)
{
    SeahorseGpgmeRevokeDialog *self = SEAHORSE_GPGME_REVOKE_DIALOG (user_data);
    SeahorseRevokeReason reason;
    const char *description;
    gpgme_error_t err;
    GtkTreeModel *model;
    GtkTreeIter iter;
    GValue value = G_VALUE_INIT;

    model = gtk_combo_box_get_model (GTK_COMBO_BOX (self->reason_combo));
    gtk_combo_box_get_active_iter (GTK_COMBO_BOX (self->reason_combo), &iter);

    gtk_tree_model_get_value (model, &iter, COLUMN_INT, &value);
    reason = g_value_get_int (&value);
    g_value_unset (&value);

    description = gtk_entry_get_text (GTK_ENTRY (self->description_entry));

    err = seahorse_gpgme_key_op_revoke_subkey (self->subkey, reason, description);
    if (!GPG_IS_OK (err))
        seahorse_gpgme_handle_error (err, _("Couldn’t revoke subkey"));
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
    GtkListStore *store;
    GtkTreeIter iter;
    GtkCellRenderer *renderer;

    gtk_widget_init_template (GTK_WIDGET (self));

    /* Initialize List Store for the Combo Box */
    store = gtk_list_store_new (N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT);

    gtk_list_store_append (store, &iter);
    gtk_list_store_set (store, &iter,
                        COLUMN_TEXT, _("No reason"),
                        COLUMN_TOOLTIP, _("No reason for revoking key"),
                        COLUMN_INT, REVOKE_NO_REASON,
                        -1);

    gtk_list_store_append (store, &iter);
    gtk_list_store_set (store, &iter,
                        COLUMN_TEXT, _("Compromised"),
                        COLUMN_TOOLTIP, _("Key has been compromised"),
                        COLUMN_INT, REVOKE_COMPROMISED,
                        -1);

    gtk_list_store_append (store, &iter);
    gtk_list_store_set (store, &iter,
                        COLUMN_TEXT, _("Superseded"),
                        COLUMN_TOOLTIP, _("Key has been superseded"),
                        COLUMN_INT, REVOKE_SUPERSEDED,
                        -1);

    gtk_list_store_append (store, &iter);
    gtk_list_store_set (store, &iter,
                        COLUMN_TEXT, _("Not Used"),
                        COLUMN_TOOLTIP, _("Key is no longer used"),
                        COLUMN_INT, REVOKE_NOT_USED,
                        -1);

    /* Finish Setting Up Combo Box */
    gtk_combo_box_set_model (GTK_COMBO_BOX (self->reason_combo),
                             GTK_TREE_MODEL (store));
    gtk_combo_box_set_active (GTK_COMBO_BOX (self->reason_combo), 0);

    renderer = gtk_cell_renderer_text_new ();
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (self->reason_combo), renderer, TRUE);
    gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (self->reason_combo), renderer,
                                    "text", COLUMN_TEXT,
                                    NULL);
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

    gtk_widget_class_set_template_from_resource (widget_class,
                                                 "/org/gnome/Seahorse/seahorse-gpgme-revoke-dialog.ui");
    gtk_widget_class_bind_template_child (widget_class,
                                          SeahorseGpgmeRevokeDialog,
                                          reason_combo);
    gtk_widget_class_bind_template_child (widget_class,
                                          SeahorseGpgmeRevokeDialog,
                                          description_entry);
    gtk_widget_class_bind_template_callback (widget_class,
                                             on_gpgme_revoke_ok_clicked);
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
                         "use-header-bar", 1,
                         NULL);
}
