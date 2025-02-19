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
#include "seahorse-pgp-enums.h"


/**
 * SECTION:seahorse-gpgme-add-subkey
 * @short_description: A dialog that allows a user to add a new subkey to a
 * key
 **/

struct _SeahorseGpgmeAddSubkey {
    GtkDialog parent_instance;

    SeahorseGpgmeKey *key;

    GtkWidget *algo_row;
    GtkCustomFilter *algo_filter;

    GtkWidget *usage_row;
    GtkCustomFilter *usage_filter;

    GtkWidget *length_row;
    GtkAdjustment *length_row_adjustment;

    GtkWidget *expires_datepicker;
    GtkWidget *never_expires_check;
};

enum {
    PROP_0,
    PROP_KEY,
    N_PROPS
};

G_DEFINE_TYPE (SeahorseGpgmeAddSubkey, seahorse_gpgme_add_subkey, GTK_TYPE_DIALOG)

static void
on_algo_row_notify_selected (GObject *object, GParamSpec *pspec, void *user_data)
{
    SeahorseGpgmeAddSubkey *self = SEAHORSE_GPGME_ADD_SUBKEY (user_data);
    SeahorsePgpKeyAlgorithm algo;
    AdwSpinRow *length_row;
    GtkAdjustment *adjustment;
    unsigned int default_val, lower, upper;

    /* Change the key length row based on the selected algo */
    algo = seahorse_gpgme_add_subkey_get_selected_algo (self);
    length_row = ADW_SPIN_ROW (self->length_row);
    adjustment = self->length_row_adjustment;

    if (seahorse_pgp_key_algorithm_get_length_values (algo, &default_val, &lower, &upper)) {
        gtk_adjustment_set_lower (adjustment, lower);
        gtk_adjustment_set_upper (adjustment, upper);
        adw_spin_row_set_value (length_row, default_val);
    }

    gtk_filter_changed (GTK_FILTER (self->usage_filter), GTK_FILTER_CHANGE_DIFFERENT);
}

SeahorsePgpKeyAlgorithm
seahorse_gpgme_add_subkey_get_selected_algo (SeahorseGpgmeAddSubkey *self)
{
    AdwComboRow *algo_row;
    GObject *selected_item;

    g_return_val_if_fail (SEAHORSE_GPGME_IS_ADD_SUBKEY (self), 0);

    algo_row = ADW_COMBO_ROW (self->algo_row);
    selected_item = adw_combo_row_get_selected_item (algo_row);
    return adw_enum_list_item_get_value (ADW_ENUM_LIST_ITEM (selected_item));
}

SeahorsePgpSubkeyUsage
seahorse_gpgme_add_subkey_get_selected_usage (SeahorseGpgmeAddSubkey *self)
{
    AdwComboRow *usage_row;
    GObject *selected_item;

    g_return_val_if_fail (SEAHORSE_GPGME_IS_ADD_SUBKEY (self), 0);

    usage_row = ADW_COMBO_ROW (self->usage_row);
    selected_item = adw_combo_row_get_selected_item (usage_row);
    return adw_enum_list_item_get_value (ADW_ENUM_LIST_ITEM (selected_item));
}

GDateTime *
seahorse_gpgme_add_subkey_get_expires (SeahorseGpgmeAddSubkey *self)
{
    g_return_val_if_fail (SEAHORSE_GPGME_IS_ADD_SUBKEY (self), NULL);

    if (gtk_check_button_get_active (GTK_CHECK_BUTTON (self->never_expires_check)))
        return NULL;

    return seahorse_date_picker_get_datetime (SEAHORSE_DATE_PICKER (self->expires_datepicker));
}

guint
seahorse_gpgme_add_subkey_get_keysize (SeahorseGpgmeAddSubkey *self)
{
    g_return_val_if_fail (SEAHORSE_GPGME_IS_ADD_SUBKEY (self), 0);

    return (guint) adw_spin_row_get_value (ADW_SPIN_ROW (self->length_row));
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

static gboolean
filter_algos (void *item, void *user_data)
{
    AdwEnumListItem *enum_item = ADW_ENUM_LIST_ITEM (item);

    switch (adw_enum_list_item_get_value (enum_item)) {
        case SEAHORSE_PGP_KEY_ALGORITHM_RSA:
        case SEAHORSE_PGP_KEY_ALGORITHM_DSA:
        case SEAHORSE_PGP_KEY_ALGORITHM_ELGAMAL:
            return TRUE;
    }

    return FALSE;
}

static char *
algo_to_string (void                    *user_data,
                SeahorsePgpKeyAlgorithm  algo)
{
    const char *str;

    str = seahorse_pgp_key_algorithm_to_string (algo);
    g_return_val_if_fail (str != NULL, NULL);
    return g_strdup (str);
}

static gboolean
filter_usages (void *item, void *user_data)
{
    SeahorseGpgmeAddSubkey *self = SEAHORSE_GPGME_ADD_SUBKEY (user_data);
    AdwEnumListItem *enum_item = ADW_ENUM_LIST_ITEM (item);
    SeahorsePgpKeyAlgorithm algo;
    SeahorsePgpSubkeyUsage usage;

    algo = seahorse_gpgme_add_subkey_get_selected_algo (self);
    usage = adw_enum_list_item_get_value (enum_item);
    switch (algo) {
        case SEAHORSE_PGP_KEY_ALGORITHM_RSA:
            return usage == SEAHORSE_PGP_SUBKEY_USAGE_SIGN_ONLY
                || usage == SEAHORSE_PGP_SUBKEY_USAGE_ENCRYPT_ONLY;
        case SEAHORSE_PGP_KEY_ALGORITHM_DSA:
            return usage == SEAHORSE_PGP_SUBKEY_USAGE_SIGN_ONLY;
        case SEAHORSE_PGP_KEY_ALGORITHM_ELGAMAL:
            return usage == SEAHORSE_PGP_SUBKEY_USAGE_SIGN_ONLY;
        default:
            return FALSE;
    }
}

static char *
usage_to_string (void                   *user_data,
                 SeahorsePgpSubkeyUsage  usage)
{
    const char *str;

    str = seahorse_pgp_subkey_usage_to_string (usage);
    g_return_val_if_fail (str != NULL, NULL);
    return g_strdup (str);
}

static void
seahorse_gpgme_add_subkey_constructed (GObject *obj)
{
    SeahorseGpgmeAddSubkey *self = SEAHORSE_GPGME_ADD_SUBKEY (obj);
    g_autofree char *title = NULL;

    G_OBJECT_CLASS (seahorse_gpgme_add_subkey_parent_class)->constructed (obj);

    title = g_strdup_printf (_("Add subkey to %s"),
                             seahorse_item_get_title (SEAHORSE_ITEM (self->key)));
    gtk_window_set_title (GTK_WINDOW (self), title);
}

static void
seahorse_gpgme_add_subkey_init (SeahorseGpgmeAddSubkey *self)
{
    g_autoptr (GDateTime) now = NULL;
    g_autoptr (GDateTime) next_year = NULL;

    g_type_ensure (SEAHORSE_TYPE_PGP_KEY_ALGORITHM);
    g_type_ensure (SEAHORSE_TYPE_PGP_SUBKEY_USAGE);

    gtk_widget_init_template (GTK_WIDGET (self));

    gtk_custom_filter_set_filter_func (self->algo_filter, filter_algos, self, NULL);
    gtk_custom_filter_set_filter_func (self->usage_filter, filter_usages, self, NULL);

    on_algo_row_notify_selected (G_OBJECT (self->algo_row), NULL, self);

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
    gtk_widget_class_bind_template_child (widget_class, SeahorseGpgmeAddSubkey, algo_row);
    gtk_widget_class_bind_template_child (widget_class, SeahorseGpgmeAddSubkey, algo_filter);
    gtk_widget_class_bind_template_child (widget_class, SeahorseGpgmeAddSubkey, usage_row);
    gtk_widget_class_bind_template_child (widget_class, SeahorseGpgmeAddSubkey, usage_filter);
    gtk_widget_class_bind_template_child (widget_class, SeahorseGpgmeAddSubkey, length_row);
    gtk_widget_class_bind_template_child (widget_class, SeahorseGpgmeAddSubkey, length_row_adjustment);
    gtk_widget_class_bind_template_child (widget_class, SeahorseGpgmeAddSubkey, expires_datepicker);
    gtk_widget_class_bind_template_child (widget_class, SeahorseGpgmeAddSubkey, never_expires_check);
    gtk_widget_class_bind_template_callback (widget_class, algo_to_string);
    gtk_widget_class_bind_template_callback (widget_class, usage_to_string);
    gtk_widget_class_bind_template_callback (widget_class, on_algo_row_notify_selected);
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
