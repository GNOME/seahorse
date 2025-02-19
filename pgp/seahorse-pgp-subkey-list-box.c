/*
 * Seahorse
 *
 * Copyright (C) 2021 Niels De Graef
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

#include "libseahorse/seahorse-util.h"

#include "seahorse-gpgme-add-subkey.h"
#include "seahorse-gpgme-expires-dialog.h"
#include "seahorse-gpgme-key-op.h"
#include "seahorse-gpgme-revoke-dialog.h"
#include "seahorse-pgp-key.h"
#include "seahorse-pgp-subkey-list-box.h"
#include "seahorse-pgp-subkey.h"

#include <glib/gi18n.h>

struct _SeahorsePgpSubkeyListBox {
    AdwPreferencesGroup parent_instance;

    SeahorsePgpKey *key;

    GPtrArray *rows;
};

enum {
    PROP_0,
    PROP_KEY,
    N_PROPS
};
static GParamSpec *obj_props[N_PROPS] = { NULL, };

G_DEFINE_TYPE (SeahorsePgpSubkeyListBox, seahorse_pgp_subkey_list_box, ADW_TYPE_PREFERENCES_GROUP)

static void
on_add_subkey_completed (GObject *object, GAsyncResult *res, void *user_data)
{
    SeahorsePgpSubkeyListBox *self = SEAHORSE_PGP_SUBKEY_LIST_BOX (user_data);
    g_autoptr(GError) error = NULL;

    if (!seahorse_gpgme_key_op_add_subkey_finish (SEAHORSE_GPGME_KEY (self->key),
                                                  res, &error)) {
        seahorse_util_show_error (GTK_WIDGET (self), NULL, error->message);
    }
}

static void
on_add_subkey_response (GtkDialog *dialog, int response, void *user_data)
{
    SeahorsePgpSubkeyListBox *self = SEAHORSE_PGP_SUBKEY_LIST_BOX (user_data);
    SeahorseGpgmeAddSubkey *add_dialog = SEAHORSE_GPGME_ADD_SUBKEY (dialog);
    SeahorsePgpKeyAlgorithm algo;
    SeahorsePgpSubkeyUsage usage;
    unsigned int length;
    GDateTime *expires;

    if (response != GTK_RESPONSE_OK) {
        gtk_window_destroy (GTK_WINDOW (add_dialog));
        return;
    }

    length = seahorse_gpgme_add_subkey_get_keysize (add_dialog);
    algo = seahorse_gpgme_add_subkey_get_selected_algo (add_dialog);
    usage = seahorse_gpgme_add_subkey_get_selected_algo (add_dialog);
    expires = seahorse_gpgme_add_subkey_get_expires (add_dialog);
    seahorse_gpgme_key_op_add_subkey_async (SEAHORSE_GPGME_KEY (self->key),
                                            algo, usage, length, expires, NULL,
                                            on_add_subkey_completed, self);

    gtk_window_destroy (GTK_WINDOW (add_dialog));
}

static void
on_add_subkey_clicked (GtkButton *button, void *user_data)
{
    SeahorsePgpSubkeyListBox *self = SEAHORSE_PGP_SUBKEY_LIST_BOX (user_data);
    GtkRoot *root;
    SeahorseGpgmeAddSubkey *dialog;

    g_return_if_fail (SEAHORSE_GPGME_IS_KEY (self->key));

    root = gtk_widget_get_root (GTK_WIDGET (button));
    dialog = seahorse_gpgme_add_subkey_new (SEAHORSE_GPGME_KEY (self->key),
                                            GTK_WINDOW (root));
    g_signal_connect (dialog, "response", G_CALLBACK (on_add_subkey_response), self);
    gtk_window_present (GTK_WINDOW (dialog));
}

static void
seahorse_pgp_subkey_list_box_set_property (GObject      *object,
                                           unsigned int  prop_id,
                                           const GValue *value,
                                           GParamSpec   *pspec)
{
    SeahorsePgpSubkeyListBox *self = SEAHORSE_PGP_SUBKEY_LIST_BOX (object);

    switch (prop_id) {
    case PROP_KEY:
        g_set_object (&self->key, g_value_get_object (value));
        break;
    }
}

static void
seahorse_pgp_subkey_list_box_get_property (GObject      *object,
                                           unsigned int  prop_id,
                                           GValue       *value,
                                           GParamSpec   *pspec)
{
    SeahorsePgpSubkeyListBox *self = SEAHORSE_PGP_SUBKEY_LIST_BOX (object);

    switch (prop_id) {
    case PROP_KEY:
        g_value_set_object (value, seahorse_pgp_subkey_list_box_get_key (self));
        break;
    }
}

static void
on_key_subkeys_changed (GListModel *subkeys,
                        unsigned int position,
                        unsigned int removed,
                        unsigned int added,
                        void *user_data)
{
    SeahorsePgpSubkeyListBox *self = SEAHORSE_PGP_SUBKEY_LIST_BOX (user_data);

    for (unsigned int i = position; i < position + removed; i++) {
        GtkWidget *row = g_ptr_array_index (self->rows, position);

        adw_preferences_group_remove (ADW_PREFERENCES_GROUP (self),
                                      row);
        g_ptr_array_remove_index (self->rows, position);
    }

    for (unsigned int i = position; i < position + added; i++) {
        GtkWidget *row;
        g_autoptr(SeahorsePgpSubkey) subkey = NULL;

        subkey = g_list_model_get_item (subkeys, i);
        row = g_object_new (SEAHORSE_PGP_TYPE_SUBKEY_LIST_BOX_ROW,
                            "subkey", subkey,
                            NULL);
        g_ptr_array_insert (self->rows, i, row);
        adw_preferences_group_add (ADW_PREFERENCES_GROUP (self),
                                   row);
    }
}

static void
seahorse_pgp_subkey_list_box_constructed (GObject *obj)
{
    SeahorsePgpSubkeyListBox *self = SEAHORSE_PGP_SUBKEY_LIST_BOX (obj);
    GListModel *subkeys;
    SeahorseUsage usage;

    G_OBJECT_CLASS (seahorse_pgp_subkey_list_box_parent_class)->constructed (obj);

    /* Setup the rows for each subkey */
    subkeys = seahorse_pgp_key_get_subkeys (self->key);
    g_signal_connect_object (subkeys,
                             "items-changed",
                             G_CALLBACK (on_key_subkeys_changed),
                             self,
                             0);
    on_key_subkeys_changed (subkeys, 0, 0, g_list_model_get_n_items (subkeys), self);

    /* If applicable, add a button to add a new subkey too */
    usage = seahorse_item_get_usage (SEAHORSE_ITEM (self->key));
    if (usage == SEAHORSE_USAGE_PRIVATE_KEY) {
        GtkWidget *button_content;
        GtkWidget *button;

        button_content = adw_button_content_new ();
        adw_button_content_set_icon_name (ADW_BUTTON_CONTENT (button_content),
                                          "list-add-symbolic");
        adw_button_content_set_label (ADW_BUTTON_CONTENT (button_content),
                                      _("Add subkey"));

        button = gtk_button_new ();
        gtk_button_set_child (GTK_BUTTON (button), button_content);
        gtk_widget_add_css_class (button, "flat");
        g_signal_connect (button, "clicked", G_CALLBACK (on_add_subkey_clicked), self);
        adw_preferences_group_set_header_suffix (ADW_PREFERENCES_GROUP (self),
                                                 button);
    }
}

static void
seahorse_pgp_subkey_list_box_init (SeahorsePgpSubkeyListBox *self)
{
    self->rows = g_ptr_array_new ();

    adw_preferences_group_set_title (ADW_PREFERENCES_GROUP (self), _("Subkeys"));
}

static void
seahorse_pgp_subkey_list_box_finalize (GObject *obj)
{
    SeahorsePgpSubkeyListBox *self = SEAHORSE_PGP_SUBKEY_LIST_BOX (obj);

    g_clear_pointer (&self->rows, g_ptr_array_unref);
    g_clear_object (&self->key);

    G_OBJECT_CLASS (seahorse_pgp_subkey_list_box_parent_class)->finalize (obj);
}

static void
seahorse_pgp_subkey_list_box_class_init (SeahorsePgpSubkeyListBoxClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->get_property = seahorse_pgp_subkey_list_box_get_property;
    gobject_class->set_property = seahorse_pgp_subkey_list_box_set_property;
    gobject_class->constructed = seahorse_pgp_subkey_list_box_constructed;
    gobject_class->finalize = seahorse_pgp_subkey_list_box_finalize;

    obj_props[PROP_KEY] =
        g_param_spec_object ("key", "Pgp Key", "The key to list subkeys for",
                             SEAHORSE_PGP_TYPE_KEY,
                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (gobject_class, N_PROPS, obj_props);
}

GtkWidget *
seahorse_pgp_subkey_list_box_new (SeahorsePgpKey *key)
{
    return g_object_new (SEAHORSE_PGP_TYPE_SUBKEY_LIST_BOX,
                         "key", key,
                         NULL);
}

SeahorsePgpKey *
seahorse_pgp_subkey_list_box_get_key (SeahorsePgpSubkeyListBox *self)
{
    g_return_val_if_fail (SEAHORSE_PGP_IS_SUBKEY_LIST_BOX (self), NULL);
    return self->key;
}

/* SeahorsePhpSubkeyListBoxRow */

struct _SeahorsePgpSubkeyListBoxRow {
    AdwExpanderRow parent_instance;

    SeahorsePgpSubkey *subkey;

    GSimpleActionGroup *action_group;

    GtkWidget *status_box;
    GtkWidget *usages_box;
    GtkWidget *algo_label;
    GtkWidget *created_label;
    GtkWidget *fingerprint_label;
    GtkWidget *actions_button;
};

enum {
    ROW_PROP_0,
    ROW_PROP_SUBKEY,
    ROW_N_PROPS
};
static GParamSpec *row_props[ROW_N_PROPS] = { NULL, };

G_DEFINE_TYPE (SeahorsePgpSubkeyListBoxRow, seahorse_pgp_subkey_list_box_row, ADW_TYPE_EXPANDER_ROW)

static GtkWindow *
get_root (SeahorsePgpSubkeyListBoxRow *row)
{
    GtkRoot *toplevel = NULL;

    toplevel = gtk_widget_get_root (GTK_WIDGET (row));
    if (GTK_IS_WINDOW (toplevel))
        return GTK_WINDOW (toplevel);

    return NULL;
}

static void
on_subkey_delete_finished (GObject *object, GAsyncResult *result, void *user_data)
{
    SeahorsePgpSubkeyListBoxRow *row = SEAHORSE_PGP_SUBKEY_LIST_BOX_ROW (user_data);
    g_autoptr(SeahorseDeleteOperation) delete_op = SEAHORSE_DELETE_OPERATION (object);
    gboolean res;
    g_autoptr(GError) error = NULL;

    res = seahorse_delete_operation_execute_interactively_finish (delete_op,
                                                                  result,
                                                                  &error);
    if (!res) {
        GtkWidget *window;

        if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
            g_debug ("Subkey delete cancelled by user");
            return;
        }

        window = GTK_WIDGET (gtk_widget_get_root (GTK_WIDGET (row)));
        seahorse_util_show_error (window, _("Couldn't Delete Subkey"), error->message);
    }
}

static void
on_subkey_delete (GSimpleAction *action, GVariant *param, void *user_data)
{
    SeahorsePgpSubkeyListBoxRow *row = SEAHORSE_PGP_SUBKEY_LIST_BOX_ROW (user_data);
    g_autoptr(SeahorseDeleteOperation) delete_op = NULL;
    GtkWindow *window;
    g_autofree char *message = NULL;

    g_return_if_fail (SEAHORSE_GPGME_IS_SUBKEY (row->subkey));

    delete_op = seahorse_deletable_create_delete_operation (SEAHORSE_DELETABLE (row->subkey));

    window = GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (row)));
    seahorse_delete_operation_execute_interactively (g_object_ref (delete_op),
                                                     window,
                                                     NULL,
                                                     on_subkey_delete_finished,
                                                     row);
}

static void
on_subkey_revoke (GSimpleAction *action, GVariant *param, void *user_data)
{
    SeahorsePgpSubkeyListBoxRow *row = SEAHORSE_PGP_SUBKEY_LIST_BOX_ROW (user_data);
    GtkWidget *dialog = NULL;

    g_return_if_fail (SEAHORSE_GPGME_IS_SUBKEY (row->subkey));

    dialog = seahorse_gpgme_revoke_dialog_new (SEAHORSE_GPGME_SUBKEY (row->subkey),
                                               get_root (row));
    gtk_window_present (GTK_WINDOW (dialog));
}

static void
on_subkey_change_expires (GSimpleAction *action, GVariant *param, void *user_data)
{
    SeahorsePgpSubkeyListBoxRow *row = SEAHORSE_PGP_SUBKEY_LIST_BOX_ROW (user_data);
    GtkWidget *dialog;
    GtkWindow *root;

    g_return_if_fail (SEAHORSE_GPGME_IS_SUBKEY (row->subkey));

    dialog = seahorse_gpgme_expires_dialog_new (SEAHORSE_GPGME_SUBKEY (row->subkey));
    root = get_root (row);
    adw_dialog_present (ADW_DIALOG (dialog), root? GTK_WIDGET (root) : NULL);
}

static const GActionEntry SUBKEY_ACTIONS[] = {
    { "delete", on_subkey_delete },
    { "revoke", on_subkey_revoke },
    { "change-expires", on_subkey_change_expires },
};

static void
update_row (SeahorsePgpSubkeyListBoxRow *row)
{
    unsigned int flags;
    g_auto(GStrv) usages = NULL, descriptions = NULL;
    g_autofree char *algo_str = NULL;
    GDateTime *expires, *created;
    g_autofree char *expires_str = NULL, *created_str = NULL;
    SeahorsePgpKey *parent_key;

    /* Start with the flags, since these can largely affect what we show */
    flags = seahorse_pgp_subkey_get_flags (row->subkey);
    if (flags & SEAHORSE_FLAG_REVOKED) {
        GAction *action;

        gtk_widget_set_visible (row->status_box, TRUE);
        gtk_widget_set_tooltip_text (row->status_box, _("Subkey was revoked"));

        action = g_action_map_lookup_action (G_ACTION_MAP (row->action_group),
                                             "revoke");
        g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);
    } else if (flags & SEAHORSE_FLAG_EXPIRED) {
        gtk_widget_set_visible (row->status_box, TRUE);
        gtk_widget_set_tooltip_text (row->status_box, _("Subkey has expired"));
    } else if (flags & SEAHORSE_FLAG_DISABLED) {
        gtk_widget_set_visible (row->status_box, TRUE);
        gtk_widget_set_tooltip_text (row->status_box, _("Subkey is disabled"));
    } else if (flags & SEAHORSE_FLAG_IS_VALID) {
        gtk_widget_set_visible (row->status_box, FALSE);
    }

    /* Set the key id */
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row),
                                   seahorse_pgp_subkey_get_keyid (row->subkey));
    expires = seahorse_pgp_subkey_get_expires (row->subkey);
    expires_str = expires? g_date_time_format (expires, "Expires on %x")
                         : g_strdup (_("Never expires"));
    adw_expander_row_set_subtitle (ADW_EXPANDER_ROW (row), expires_str);

    /* Add the usage tags */
    usages = seahorse_pgp_subkey_get_usages (row->subkey, &descriptions);
    for (unsigned i = 0; i < g_strv_length (usages); i++) {
        GtkWidget *label;

        label = gtk_label_new (usages[i]);
        gtk_widget_set_tooltip_text (label, descriptions[i]);
        gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
        gtk_box_append (GTK_BOX (row->usages_box), label);
        gtk_widget_add_css_class (label, "pgp-subkey-usage-label");
    }

    /* Stuff that is hidden by default (but can be shown with the revealer) */

    /* Full fingerprint */
    gtk_label_set_text (GTK_LABEL (row->fingerprint_label),
                        seahorse_pgp_subkey_get_fingerprint (row->subkey));

    /* Translators: first part is the algorithm, second part is the length,
     * e.g. "RSA" (2048 bit) */
    algo_str = g_strdup_printf (_("%s (%d bit)"),
                                seahorse_pgp_subkey_get_algorithm (row->subkey),
                                seahorse_pgp_subkey_get_length (row->subkey));
    gtk_label_set_text (GTK_LABEL (row->algo_label), algo_str);

    /* Creation date */
    created = seahorse_pgp_subkey_get_created (row->subkey);
    created_str = created? g_date_time_format (created, "%x") : g_strdup (_("Unknown"));
    gtk_label_set_text (GTK_LABEL (row->created_label), created_str);

    parent_key = seahorse_pgp_subkey_get_parent_key (SEAHORSE_PGP_SUBKEY (row->subkey));
    if (seahorse_item_get_usage (SEAHORSE_ITEM (parent_key)) != SEAHORSE_USAGE_PRIVATE_KEY)
        gtk_widget_set_visible (row->actions_button, FALSE);
}

static void
seahorse_pgp_subkey_list_box_row_set_property (GObject      *object,
                                               unsigned int  prop_id,
                                               const GValue *value,
                                               GParamSpec   *pspec)
{
    SeahorsePgpSubkeyListBoxRow *row = SEAHORSE_PGP_SUBKEY_LIST_BOX_ROW (object);

    switch (prop_id) {
    case ROW_PROP_SUBKEY:
        g_set_object (&row->subkey, g_value_dup_object (value));
        break;
    }
}

static void
seahorse_pgp_subkey_list_box_row_get_property (GObject      *object,
                                               unsigned int  prop_id,
                                               GValue       *value,
                                               GParamSpec   *pspec)
{
    SeahorsePgpSubkeyListBoxRow *row = SEAHORSE_PGP_SUBKEY_LIST_BOX_ROW (object);

    switch (prop_id) {
    case ROW_PROP_SUBKEY:
        g_value_set_object (value, seahorse_pgp_subkey_list_box_row_get_subkey (row));
        break;
    }
}

static void
on_subkey_notify (GObject *object, GParamSpec *pspec, void *user_data)
{
    SeahorsePgpSubkeyListBoxRow *row = SEAHORSE_PGP_SUBKEY_LIST_BOX_ROW (user_data);

    update_row (row);
}

static void
seahorse_pgp_subkey_list_box_row_constructed (GObject *obj)
{
    SeahorsePgpSubkeyListBoxRow *row = SEAHORSE_PGP_SUBKEY_LIST_BOX_ROW (obj);

    G_OBJECT_CLASS (seahorse_pgp_subkey_list_box_row_parent_class)->constructed (obj);

    g_signal_connect_object (row->subkey, "notify",
                             G_CALLBACK (on_subkey_notify), row, 0);
    update_row (row);
}

static void
seahorse_pgp_subkey_list_box_row_init (SeahorsePgpSubkeyListBoxRow *row)
{
    gtk_widget_init_template (GTK_WIDGET (row));

    row->action_group = g_simple_action_group_new ();
    g_action_map_add_action_entries (G_ACTION_MAP (row->action_group),
                                     SUBKEY_ACTIONS,
                                     G_N_ELEMENTS (SUBKEY_ACTIONS),
                                     row);
    gtk_widget_insert_action_group (GTK_WIDGET (row),
                                    "subkey",
                                    G_ACTION_GROUP (row->action_group));
}

static void
seahorse_pgp_subkey_list_box_row_finalize (GObject *obj)
{
    SeahorsePgpSubkeyListBoxRow *row = SEAHORSE_PGP_SUBKEY_LIST_BOX_ROW (obj);

    g_clear_object (&row->subkey);
    g_clear_object (&row->action_group);

    G_OBJECT_CLASS (seahorse_pgp_subkey_list_box_row_parent_class)->finalize (obj);
}

static void
seahorse_pgp_subkey_list_box_row_class_init (SeahorsePgpSubkeyListBoxRowClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    gobject_class->get_property = seahorse_pgp_subkey_list_box_row_get_property;
    gobject_class->set_property = seahorse_pgp_subkey_list_box_row_set_property;
    gobject_class->constructed = seahorse_pgp_subkey_list_box_row_constructed;
    gobject_class->finalize = seahorse_pgp_subkey_list_box_row_finalize;

    row_props[ROW_PROP_SUBKEY] =
        g_param_spec_object ("subkey", "PGP subkey", "The subkey this row shows",
                             SEAHORSE_PGP_TYPE_SUBKEY,
                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (gobject_class, ROW_N_PROPS, row_props);

    /* GtkWidget template */
    gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Seahorse/seahorse-pgp-subkey-list-box-row.ui");

    gtk_widget_class_bind_template_child (widget_class, SeahorsePgpSubkeyListBoxRow, status_box);
    gtk_widget_class_bind_template_child (widget_class, SeahorsePgpSubkeyListBoxRow, usages_box);
    gtk_widget_class_bind_template_child (widget_class, SeahorsePgpSubkeyListBoxRow, algo_label);
    gtk_widget_class_bind_template_child (widget_class, SeahorsePgpSubkeyListBoxRow, created_label);
    gtk_widget_class_bind_template_child (widget_class, SeahorsePgpSubkeyListBoxRow, fingerprint_label);
    gtk_widget_class_bind_template_child (widget_class, SeahorsePgpSubkeyListBoxRow, actions_button);
}

SeahorsePgpSubkey *
seahorse_pgp_subkey_list_box_row_get_subkey (SeahorsePgpSubkeyListBoxRow *row)
{
    g_return_val_if_fail (SEAHORSE_PGP_IS_SUBKEY_LIST_BOX_ROW (row), NULL);
    return row->subkey;
}
