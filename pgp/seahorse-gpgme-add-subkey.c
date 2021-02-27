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

#include <glib/gi18n.h>

#include "config.h"
#include "seahorse-common.h"
#include "libseahorse/seahorse-util.h"
#include "seahorse-gpgme-add-subkey.h"
#include "seahorse-gpgme-key-op.h"


/**
 * SECTION:seahorse-gpgme-add-subkey
 * @short_description: A dialog that allows a user to add a new subkey to a
 * key
 **/

struct _SeahorseGpgmeAddSubkey {
    GtkDialog parent_instance;

    SeahorseGpgmeKey *key;

    GtkTreeModel *types_model;
    GtkWidget *type_combo;

    GtkWidget *length_spinner;

    GtkWidget *expires_datepicker;
    GtkWidget *never_expires_check;
};

enum {
    PROP_0,
    PROP_KEY,
};

G_DEFINE_TYPE (SeahorseGpgmeAddSubkey, seahorse_gpgme_add_subkey, GTK_TYPE_DIALOG)

enum {
    COMBO_STRING,
    COMBO_INT,
    N_COLUMNS
};

static void
handler_gpgme_add_subkey_type_changed (GtkComboBox *combo,
                                       gpointer user_data)
{
    SeahorseGpgmeAddSubkey *self = SEAHORSE_GPGME_ADD_SUBKEY (user_data);
    int type;
    GtkSpinButton *length;
    GtkTreeModel *model;
    GtkTreeIter iter;

    length = GTK_SPIN_BUTTON (self->length_spinner);

    model = gtk_combo_box_get_model (combo);
    gtk_combo_box_get_active_iter (combo, &iter);
    gtk_tree_model_get (model, &iter,
                        COMBO_INT, &type,
                        -1);

    switch (type) {
        /* DSA */
        case 0:
            gtk_spin_button_set_range (length, DSA_MIN, DSA_MAX);
            gtk_spin_button_set_value (length, LENGTH_DEFAULT < DSA_MAX ? LENGTH_DEFAULT : DSA_MAX);
            break;
        /* ElGamal */
        case 1:
            gtk_spin_button_set_range (length, ELGAMAL_MIN, LENGTH_MAX);
            gtk_spin_button_set_value (length, LENGTH_DEFAULT);
            break;
        /* RSA */
        default:
            gtk_spin_button_set_range (length, RSA_MIN, LENGTH_MAX);
            gtk_spin_button_set_value (length, LENGTH_DEFAULT);
            break;
    }
}

static void
on_gpgme_add_subkey_never_expires_toggled (GtkToggleButton *togglebutton,
                                           gpointer user_data)
{
    SeahorseGpgmeAddSubkey *self = SEAHORSE_GPGME_ADD_SUBKEY (user_data);

    gtk_widget_set_sensitive (self->expires_datepicker,
                              !gtk_toggle_button_get_active (togglebutton));
}

SeahorseKeyEncType
seahorse_gpgme_add_subkey_get_active_type (SeahorseGpgmeAddSubkey *self)
{
    GtkTreeIter iter;
    int type;

    g_return_val_if_fail (SEAHORSE_GPGME_IS_ADD_SUBKEY (self), 0);

    gtk_combo_box_get_active_iter (GTK_COMBO_BOX (self->type_combo), &iter);
    gtk_tree_model_get (self->types_model, &iter,
                        COMBO_INT, &type,
                        -1);

    switch (type) {
        case 0:
            return DSA;
        case 1:
            return ELGAMAL;
        case 2:
            return RSA_SIGN;
        default:
            return RSA_ENCRYPT;
    }
}

GDateTime *
seahorse_gpgme_add_subkey_get_expires (SeahorseGpgmeAddSubkey *self)
{
    g_return_val_if_fail (SEAHORSE_GPGME_IS_ADD_SUBKEY (self), 0);

    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->never_expires_check)))
        return 0;

    return seahorse_date_picker_get_datetime (SEAHORSE_DATE_PICKER(self->expires_datepicker));
}

guint
seahorse_gpgme_add_subkey_get_keysize (SeahorseGpgmeAddSubkey *self)
{
    g_return_val_if_fail (SEAHORSE_GPGME_IS_ADD_SUBKEY (self), 0);

    return gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (self->length_spinner));
}

static void
seahorse_gpgme_add_subkey_get_property (GObject *object,
                                        guint prop_id,
                                        GValue *value,
                                        GParamSpec *pspec)
{
    SeahorseGpgmeAddSubkey *self = SEAHORSE_GPGME_ADD_SUBKEY (object);

    switch (prop_id) {
    case PROP_KEY:
        g_value_set_object (value, self->key);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
seahorse_gpgme_add_subkey_set_property (GObject *object,
                                        guint prop_id,
                                        const GValue *value,
                                        GParamSpec *pspec)
{
    SeahorseGpgmeAddSubkey *self = SEAHORSE_GPGME_ADD_SUBKEY (object);

    switch (prop_id) {
    case PROP_KEY:
        g_clear_object (&self->key);
        self->key = g_value_dup_object (value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
seahorse_gpgme_add_subkey_finalize (GObject *obj)
{
    SeahorseGpgmeAddSubkey *self = SEAHORSE_GPGME_ADD_SUBKEY (obj);

    g_clear_object (&self->key);

    G_OBJECT_CLASS (seahorse_gpgme_add_subkey_parent_class)->finalize (obj);
}

static void
seahorse_gpgme_add_subkey_constructed (GObject *obj)
{
    SeahorseGpgmeAddSubkey *self = SEAHORSE_GPGME_ADD_SUBKEY (obj);
    g_autofree char *title = NULL;

    G_OBJECT_CLASS (seahorse_gpgme_add_subkey_parent_class)->constructed (obj);

    title = g_strdup_printf (_("Add subkey to %s"),
                             seahorse_object_get_label (SEAHORSE_OBJECT (self->key)));
    gtk_window_set_title (GTK_WINDOW (self), title);
}

static void
seahorse_gpgme_add_subkey_init (SeahorseGpgmeAddSubkey *self)
{
    GtkTreeIter iter;
    GtkCellRenderer *renderer;
    g_autoptr (GDateTime) now = NULL;
    g_autoptr (GDateTime) next_year = NULL;

    gtk_widget_init_template (GTK_WIDGET (self));

    self->types_model = GTK_TREE_MODEL (gtk_list_store_new (N_COLUMNS, G_TYPE_STRING, G_TYPE_INT));
    gtk_combo_box_set_model (GTK_COMBO_BOX (self->type_combo),
                             GTK_TREE_MODEL (self->types_model));

    gtk_cell_layout_clear (GTK_CELL_LAYOUT (self->type_combo));
    renderer = gtk_cell_renderer_text_new ();

    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (self->type_combo), renderer, TRUE);
    gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (self->type_combo), renderer,
                                    "text", COMBO_STRING);

    gtk_list_store_append (GTK_LIST_STORE (self->types_model), &iter);
    gtk_list_store_set (GTK_LIST_STORE (self->types_model), &iter,
                        COMBO_STRING, _("DSA (sign only)"),
                        COMBO_INT, 0,
                        -1);

    gtk_combo_box_set_active_iter (GTK_COMBO_BOX (self->type_combo), &iter);

    gtk_list_store_append (GTK_LIST_STORE (self->types_model), &iter);
    gtk_list_store_set (GTK_LIST_STORE (self->types_model), &iter,
                        COMBO_STRING, _("ElGamal (encrypt only)"),
                        COMBO_INT, 1,
                        -1);

    gtk_list_store_append (GTK_LIST_STORE (self->types_model), &iter);
    gtk_list_store_set (GTK_LIST_STORE (self->types_model), &iter,
                        COMBO_STRING, _("RSA (sign only)"),
                        COMBO_INT, 2,
                        -1);

    gtk_list_store_append (GTK_LIST_STORE (self->types_model), &iter);
    gtk_list_store_set (GTK_LIST_STORE (self->types_model), &iter,
                        COMBO_STRING, _("RSA (encrypt only)"),
                        COMBO_INT, 3,
                        -1);

    now = g_date_time_new_now_utc ();
    next_year = g_date_time_add_years (now, 1);
    seahorse_date_picker_set_datetime (SEAHORSE_DATE_PICKER (self->expires_datepicker),
                                       next_year);
}

static void
seahorse_gpgme_add_subkey_class_init (SeahorseGpgmeAddSubkeyClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    gobject_class->constructed = seahorse_gpgme_add_subkey_constructed;
    gobject_class->get_property = seahorse_gpgme_add_subkey_get_property;
    gobject_class->set_property = seahorse_gpgme_add_subkey_set_property;
    gobject_class->finalize = seahorse_gpgme_add_subkey_finalize;

    g_object_class_install_property (gobject_class, PROP_KEY,
        g_param_spec_object ("key", "GPGME key",
                             "The GPGME key which we're adding a new subkey to",
                             SEAHORSE_GPGME_TYPE_KEY,
                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

    gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Seahorse/seahorse-gpgme-add-subkey.ui");
    gtk_widget_class_bind_template_child (widget_class, SeahorseGpgmeAddSubkey, type_combo);
    gtk_widget_class_bind_template_child (widget_class, SeahorseGpgmeAddSubkey, length_spinner);
    gtk_widget_class_bind_template_child (widget_class, SeahorseGpgmeAddSubkey, expires_datepicker);
    gtk_widget_class_bind_template_child (widget_class, SeahorseGpgmeAddSubkey, never_expires_check);
    gtk_widget_class_bind_template_callback (widget_class, handler_gpgme_add_subkey_type_changed);
    gtk_widget_class_bind_template_callback (widget_class, on_gpgme_add_subkey_never_expires_toggled);
}

/**
 * seahorse_add_subkey_new:
 * @key: A #SeahorseGpgmeKey
 *
 * Creates a new #SeahorseGpgmeAddSubkey dialog for adding a user ID to @skey.
 */
SeahorseGpgmeAddSubkey *
seahorse_gpgme_add_subkey_new (SeahorseGpgmeKey *key, GtkWindow *parent)
{
    g_autoptr(SeahorseGpgmeAddSubkey) self = NULL;

    g_return_val_if_fail (SEAHORSE_GPGME_IS_KEY (key), NULL);

    self = g_object_new (SEAHORSE_GPGME_TYPE_ADD_SUBKEY,
                         "key", key,
                         "transient-for", parent,
                         "use-header-bar", 1,
                         NULL);

    return g_steal_pointer (&self);
}
