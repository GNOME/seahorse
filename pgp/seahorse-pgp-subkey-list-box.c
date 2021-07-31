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

#include "seahorse-gpgme-expires-dialog.h"
#include "seahorse-gpgme-key-op.h"
#include "seahorse-gpgme-revoke-dialog.h"
#include "seahorse-pgp-key.h"
#include "seahorse-pgp-subkey-list-box.h"
#include "seahorse-pgp-subkey.h"

#include <glib/gi18n.h>

struct _SeahorsePgpSubkeyListBox {
    GtkListBox parent_instance;

    SeahorsePgpKey *key;
};

enum {
    PROP_0,
    PROP_KEY,
    N_PROPS
};
static GParamSpec *obj_props[N_PROPS] = { NULL, };

G_DEFINE_TYPE (SeahorsePgpSubkeyListBox, seahorse_pgp_subkey_list_box, GTK_TYPE_LIST_BOX)

static void
seahorse_pgp_subkey_list_box_set_property (GObject      *object,
                                           unsigned int  prop_id,
                                           const GValue *value,
                                           GParamSpec   *pspec)
{
    SeahorsePgpSubkeyListBox *self = SEAHORSE_PGP_SUBKEY_LIST_BOX (object);

    switch (prop_id) {
    case PROP_KEY:
        g_set_object (&self->key, g_value_dup_object (value));
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

static GtkWidget *
create_subkey_row (void *item,
                   void *user_data)
{
    SeahorsePgpSubkey *subkey = SEAHORSE_PGP_SUBKEY (item);

    return g_object_new (SEAHORSE_PGP_TYPE_SUBKEY_LIST_BOX_ROW,
                         "subkey", subkey,
                         NULL);
}

static void
seahorse_pgp_subkey_list_box_constructed (GObject *obj)
{
    SeahorsePgpSubkeyListBox *self = SEAHORSE_PGP_SUBKEY_LIST_BOX (obj);

    G_OBJECT_CLASS (seahorse_pgp_subkey_list_box_parent_class)->constructed (obj);

    gtk_list_box_bind_model (GTK_LIST_BOX (self),
                             seahorse_pgp_key_get_subkeys (self->key),
                             create_subkey_row,
                             self,
                             NULL);
}

static void
seahorse_pgp_subkey_list_box_init (SeahorsePgpSubkeyListBox *self)
{
    GtkStyleContext *style_context;

    style_context = gtk_widget_get_style_context (GTK_WIDGET (self));
    gtk_style_context_add_class (style_context, "content");
}

static void
seahorse_pgp_subkey_list_box_finalize (GObject *obj)
{
    SeahorsePgpSubkeyListBox *self = SEAHORSE_PGP_SUBKEY_LIST_BOX (obj);

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
                         "selection-mode", GTK_SELECTION_NONE,
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
    HdyExpanderRow parent_instance;

    SeahorsePgpSubkey *subkey;

    GSimpleActionGroup *action_group;

    GtkWidget *status_box;
    GtkWidget *usages_box;
    GtkWidget *algo_label;
    GtkWidget *created_label;
    GtkWidget *fingerprint_label;
    GtkWidget *action_box;
};

enum {
    ROW_PROP_0,
    ROW_PROP_SUBKEY,
    ROW_N_PROPS
};
static GParamSpec *row_props[ROW_N_PROPS] = { NULL, };

G_DEFINE_TYPE (SeahorsePgpSubkeyListBoxRow, seahorse_pgp_subkey_list_box_row, HDY_TYPE_EXPANDER_ROW)

static GtkWindow *
get_toplevel_window (SeahorsePgpSubkeyListBoxRow *row)
{
    GtkWidget *toplevel = NULL;

    toplevel = gtk_widget_get_toplevel (GTK_WIDGET (row));
    if (GTK_IS_WINDOW (toplevel))
        return GTK_WINDOW (toplevel);

    return NULL;
}

static void
on_subkey_delete (GSimpleAction *action, GVariant *param, void *user_data)
{
    SeahorsePgpSubkeyListBoxRow *row = SEAHORSE_PGP_SUBKEY_LIST_BOX_ROW (user_data);
    const char *fingerprint;
    g_autofree char *message = NULL;
    gpgme_error_t err;

    g_return_if_fail (SEAHORSE_GPGME_IS_SUBKEY (row->subkey));

    fingerprint = seahorse_pgp_subkey_get_fingerprint (row->subkey);
    message = g_strdup_printf (_("Are you sure you want to permanently delete subkey %s?"), fingerprint);

    if (!seahorse_delete_dialog_prompt (get_toplevel_window (row), message))
        return;

    err = seahorse_gpgme_key_op_del_subkey (SEAHORSE_GPGME_SUBKEY (row->subkey));
    if (!GPG_IS_OK (err))
        seahorse_gpgme_handle_error (err, _("Couldn’t delete subkey"));
}

static void
on_subkey_revoke (GSimpleAction *action, GVariant *param, void *user_data)
{
    SeahorsePgpSubkeyListBoxRow *row = SEAHORSE_PGP_SUBKEY_LIST_BOX_ROW (user_data);
    GtkWidget *dialog = NULL;

    g_return_if_fail (SEAHORSE_GPGME_IS_SUBKEY (row->subkey));

    dialog = seahorse_gpgme_revoke_dialog_new (SEAHORSE_GPGME_SUBKEY (row->subkey),
                                               get_toplevel_window (row));
    gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_destroy (dialog);
}

static void
on_subkey_change_expires (GSimpleAction *action, GVariant *param, void *user_data)
{
    SeahorsePgpSubkeyListBoxRow *row = SEAHORSE_PGP_SUBKEY_LIST_BOX_ROW (user_data);
    GtkDialog *dialog;

    g_return_if_fail (SEAHORSE_GPGME_IS_SUBKEY (row->subkey));

    dialog = seahorse_gpgme_expires_dialog_new (SEAHORSE_GPGME_SUBKEY (row->subkey),
                                                get_toplevel_window (row));
    gtk_dialog_run (dialog);
    gtk_widget_destroy (GTK_WIDGET (dialog));
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

        gtk_widget_show (row->status_box);
        gtk_widget_set_tooltip_text (row->status_box, _("Subkey was revoked"));

        action = g_action_map_lookup_action (G_ACTION_MAP (row->action_group),
                                             "revoke");
        g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);
    } else if (flags & SEAHORSE_FLAG_EXPIRED) {
        gtk_widget_show (row->status_box);
        gtk_widget_set_tooltip_text (row->status_box, _("Subkey has expired"));
    } else if (flags & SEAHORSE_FLAG_DISABLED) {
        gtk_widget_show (row->status_box);
        gtk_widget_set_tooltip_text (row->status_box, _("Subkey is disabled"));
    } else if (flags & SEAHORSE_FLAG_IS_VALID) {
        gtk_widget_hide (row->status_box);
    }

    /* Set the key id */
    hdy_preferences_row_set_title (HDY_PREFERENCES_ROW (row),
                                   seahorse_pgp_subkey_get_keyid (row->subkey));
    expires = seahorse_pgp_subkey_get_expires (row->subkey);
    expires_str = expires? g_date_time_format (expires, "Expires on %x")
                         : g_strdup (_("Never expires"));
    hdy_expander_row_set_subtitle (HDY_EXPANDER_ROW (row), expires_str);

    /* Add the usage tags */
    usages = seahorse_pgp_subkey_get_usages (row->subkey, &descriptions);
    for (unsigned i = 0; i < g_strv_length (usages); i++) {
        GtkWidget *label;

        label = gtk_label_new (usages[i]);
        gtk_widget_set_tooltip_text (label, descriptions[i]);
        gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
        gtk_widget_show (label);
        gtk_box_pack_start (GTK_BOX (row->usages_box), label, FALSE, FALSE, 0);
        gtk_style_context_add_class (gtk_widget_get_style_context (label),
                                     "pgp-subkey-usage-label");
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
    if (seahorse_object_get_usage (SEAHORSE_OBJECT (parent_key)) != SEAHORSE_USAGE_PRIVATE_KEY)
        gtk_widget_hide (row->action_box);
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
    GtkStyleContext *style_context;

    gtk_widget_init_template (GTK_WIDGET (row));
    style_context = gtk_widget_get_style_context (GTK_WIDGET (row));
    gtk_style_context_add_class (style_context, "pgp-subkey-row");

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
    gtk_widget_class_bind_template_child (widget_class, SeahorsePgpSubkeyListBoxRow, action_box);
}

SeahorsePgpSubkey *
seahorse_pgp_subkey_list_box_row_get_subkey (SeahorsePgpSubkeyListBoxRow *row)
{
    g_return_val_if_fail (SEAHORSE_PGP_IS_SUBKEY_LIST_BOX_ROW (row), NULL);
    return row->subkey;
}
