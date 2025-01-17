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

#include "pgp/seahorse-pgp-uid-list-box.h"

#include "seahorse-gpgme-key-op.h"
#include "pgp/seahorse-pgp-backend.h"
#include "pgp/seahorse-pgp-signature.h"
#include "pgp/seahorse-gpgme-uid.h"
#include "pgp/seahorse-gpgme-add-uid.h"
#include "pgp/seahorse-gpgme-sign-dialog.h"
#include "pgp/seahorse-unknown.h"
#include "seahorse-gpgme-dialogs.h"

#include <glib/gi18n.h>

/* ListBox object */

struct _SeahorsePgpUidListBox {
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

G_DEFINE_TYPE (SeahorsePgpUidListBox, seahorse_pgp_uid_list_box, ADW_TYPE_PREFERENCES_GROUP);

static void
on_add_uid_clicked (GtkButton *button, void *user_data)
{
    SeahorsePgpUidListBox *self = SEAHORSE_PGP_UID_LIST_BOX (user_data);
    GtkRoot *root;

    g_return_if_fail (SEAHORSE_GPGME_IS_KEY (self->key));

    root = gtk_widget_get_root (GTK_WIDGET (button));
    seahorse_gpgme_add_uid_run_dialog (SEAHORSE_GPGME_KEY (self->key),
                                       GTK_WINDOW (root));
}

static void
on_key_uids_changed (GListModel *uids,
                     unsigned int position,
                     unsigned int removed,
                     unsigned int added,
                     void *user_data)
{
    SeahorsePgpUidListBox *self = SEAHORSE_PGP_UID_LIST_BOX (user_data);

    for (unsigned int i = position; i < position + removed; i++) {
        GtkWidget *row = g_ptr_array_index (self->rows, position);

        adw_preferences_group_remove (ADW_PREFERENCES_GROUP (self),
                                      row);
        g_ptr_array_remove_index (self->rows, position);
    }

    for (unsigned int i = position; i < position + added; i++) {
        GtkWidget *row;
        g_autoptr(SeahorsePgpUid) uid = NULL;

        uid = g_list_model_get_item (uids, i);
        row = g_object_new (SEAHORSE_PGP_TYPE_UID_LIST_BOX_ROW,
                            "uid", uid,
                            NULL);
        g_ptr_array_insert (self->rows, i, row);
        adw_preferences_group_add (ADW_PREFERENCES_GROUP (self),
                                   row);
    }
}

static void
seahorse_pgp_uid_list_box_constructed (GObject *object)
{
    SeahorsePgpUidListBox *self = SEAHORSE_PGP_UID_LIST_BOX (object);
    GListModel *uids;

    G_OBJECT_CLASS (seahorse_pgp_uid_list_box_parent_class)->constructed (object);

    /* Setup the rows for each uid */
    uids = seahorse_pgp_key_get_uids (self->key);
    g_signal_connect_object (uids,
                             "items-changed",
                             G_CALLBACK (on_key_uids_changed),
                             self,
                             0);
    on_key_uids_changed (uids, 0, 0, g_list_model_get_n_items (uids), self);

    /* If applicable, add a button to add a new uid too */
    if (seahorse_pgp_key_is_private_key (self->key)) {
        GtkWidget *button_content;
        GtkWidget *button;

        button_content = adw_button_content_new ();
        adw_button_content_set_icon_name (ADW_BUTTON_CONTENT (button_content),
                                          "list-add-symbolic");
        adw_button_content_set_label (ADW_BUTTON_CONTENT (button_content),
                                      _("Add user ID"));

        button = gtk_button_new ();
        gtk_button_set_child (GTK_BUTTON (button), button_content);
        gtk_widget_add_css_class (button, "flat");
        g_signal_connect (button, "clicked", G_CALLBACK (on_add_uid_clicked), self);
        adw_preferences_group_set_header_suffix (ADW_PREFERENCES_GROUP (self),
                                                 button);
    }
}

static void
seahorse_pgp_uid_list_box_init (SeahorsePgpUidListBox *self)
{
    self->rows = g_ptr_array_new ();

    adw_preferences_group_set_title (ADW_PREFERENCES_GROUP (self), _("User IDs"));
}

static void
seahorse_pgp_uid_list_box_finalize (GObject *obj)
{
    SeahorsePgpUidListBox *self = SEAHORSE_PGP_UID_LIST_BOX (obj);

    g_clear_pointer (&self->rows, g_ptr_array_unref);
    g_clear_object (&self->key);

    G_OBJECT_CLASS (seahorse_pgp_uid_list_box_parent_class)->finalize (obj);
}

static void
seahorse_pgp_uid_list_box_get_property (GObject      *object,
                                        unsigned int  prop_id,
                                        GValue       *value,
                                        GParamSpec   *pspec)
{
    SeahorsePgpUidListBox *self = SEAHORSE_PGP_UID_LIST_BOX (object);

    switch (prop_id) {
    case PROP_KEY:
        g_value_set_object (value, self->key);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
seahorse_pgp_uid_list_box_set_property (GObject      *object,
                                        unsigned int  prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
    SeahorsePgpUidListBox *self = SEAHORSE_PGP_UID_LIST_BOX (object);

    switch (prop_id) {
    case PROP_KEY:
        g_set_object (&self->key, g_value_get_object (value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
seahorse_pgp_uid_list_box_class_init (SeahorsePgpUidListBoxClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->set_property = seahorse_pgp_uid_list_box_set_property;
    gobject_class->get_property = seahorse_pgp_uid_list_box_get_property;
    gobject_class->constructed = seahorse_pgp_uid_list_box_constructed;
    gobject_class->finalize = seahorse_pgp_uid_list_box_finalize;

    obj_props[PROP_KEY] =
        g_param_spec_object ("key", "PGP key", "The key to list the UIDs for",
                             SEAHORSE_PGP_TYPE_KEY,
                             G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT);

    g_object_class_install_properties (gobject_class, N_PROPS, obj_props);
}

/**
 * seahorse_pgp_uid_list_box_new:
 * @key: A PGP key
 *
 * Creates a new list box that shows the UIDs of @key
 *
 * Returns: (transfer floating): The new #SeahorsePgpUidListBox
 */
GtkWidget *
seahorse_pgp_uid_list_box_new (SeahorsePgpKey *key)
{
    g_return_val_if_fail (SEAHORSE_PGP_IS_KEY (key), NULL);

    /* XXX We should store the key and connect to ::notify */
    return g_object_new (SEAHORSE_PGP_TYPE_UID_LIST_BOX,
                         "key", key,
                         NULL);
}

/* Row object */

struct _SeahorsePgpUidListBoxRow {
    AdwExpanderRow parent_instance;

    SeahorsePgpUid *uid;

    GSimpleActionGroup *actions;
    GtkWidget *actions_button;

    GtkWidget *signatures_list;
    /* the key ids from the signature list that were discovered */
    GList *discovered_keys;
};

enum {
    ROW_PROP_0,
    ROW_PROP_UID,
    ROW_N_PROPS
};

G_DEFINE_TYPE (SeahorsePgpUidListBoxRow, seahorse_pgp_uid_list_box_row, ADW_TYPE_EXPANDER_ROW);

static void
update_actions (SeahorsePgpUidListBoxRow *row)
{
    SeahorsePgpKey *parent;
    GListModel *uids;
    g_autoptr(SeahorsePgpUid) primary_uid = NULL;
    gboolean is_primary;
    GAction *action;

    /* Figure out if we're the primary UID and update the make-primary action */
    parent = seahorse_pgp_uid_get_parent (row->uid);
    uids = seahorse_pgp_key_get_uids (parent);
    primary_uid = g_list_model_get_item (uids, 0);
    is_primary = (primary_uid == row->uid);

    action = g_action_map_lookup_action (G_ACTION_MAP (row->actions),
                                         "make-primary");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action), !is_primary);

    /* Don't allow deleting the last UID */
    action = g_action_map_lookup_action (G_ACTION_MAP (row->actions), "delete");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                                 g_list_model_get_n_items (uids) > 1);
}

static void
on_only_trusted_changed (GSimpleAction *action,
                         GVariant      *new_state,
                         void          *user_data)
{
    SeahorsePgpUidListBoxRow *row = SEAHORSE_PGP_UID_LIST_BOX_ROW (user_data);
    GListModel *signatures;
    unsigned int n_signatures = 0;
    unsigned int n_shown = 0;
    gboolean only_trusted;

    only_trusted = g_variant_get_boolean (new_state);

    signatures = seahorse_pgp_uid_get_signatures (row->uid);
    n_signatures = g_list_model_get_n_items (signatures);

    /* For each signature, check if we know about the key*/
    for (unsigned int i = 0; i < n_signatures; i++) {
        g_autoptr(SeahorsePgpSignature) sig = NULL;
        GtkListBoxRow *sig_row;
        SeahorseItem *signer;
        gboolean trusted = FALSE;
        gboolean should_show;

        sig = g_list_model_get_item (signatures, i);
        sig_row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (row->signatures_list), i);

        signer = g_object_get_data (G_OBJECT (sig_row), "signer");
        if (signer) {
            if (seahorse_item_get_usage (signer) == SEAHORSE_USAGE_PRIVATE_KEY)
                trusted = TRUE;
            else if (seahorse_item_get_item_flags (signer) & SEAHORSE_FLAG_TRUSTED)
                trusted = TRUE;
        }

        should_show = (trusted || !only_trusted);
        gtk_widget_set_visible (GTK_WIDGET (sig_row), should_show);
        if (should_show)
            n_shown++;
    }

    g_simple_action_set_state (G_SIMPLE_ACTION (action), new_state);

    /* Hide the signature list if there are no rows visible */
    gtk_widget_set_visible (row->signatures_list, n_shown > 0);
}

static void
on_uid_delete_finished (GObject *object, GAsyncResult *result, void *user_data)
{
    SeahorsePgpUidListBoxRow *row = SEAHORSE_PGP_UID_LIST_BOX_ROW (user_data);
    g_autoptr(SeahorseDeleteOperation) delete_op = SEAHORSE_DELETE_OPERATION (object);
    gboolean res;
    g_autoptr(GError) error = NULL;

    res = seahorse_delete_operation_execute_interactively_finish (delete_op,
                                                                  result,
                                                                  &error);
    if (!res) {
        GtkWidget *window;

        if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
            g_debug ("UID delete cancelled by user");
            return;
        }

        window = GTK_WIDGET (gtk_widget_get_root (GTK_WIDGET (row)));
        seahorse_util_show_error (window, _("Couldn't Delete User ID"), error->message);
    }
}

static void
on_uid_delete (GSimpleAction *action, GVariant *param, void *user_data)
{
    SeahorsePgpUidListBoxRow *row = SEAHORSE_PGP_UID_LIST_BOX_ROW (user_data);
    g_autoptr(SeahorseDeleteOperation) delete_op = NULL;
    GtkWindow *window;
    g_autofree char *message = NULL;

    g_return_if_fail (SEAHORSE_GPGME_IS_UID (row->uid));

    delete_op = seahorse_deletable_create_delete_operation (SEAHORSE_DELETABLE (row->uid));

    window = GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (row)));
    seahorse_delete_operation_execute_interactively (g_object_ref (delete_op),
                                                     window,
                                                     NULL,
                                                     on_uid_delete_finished,
                                                     row);
}

static void
on_uid_make_primary_cb (GObject *source, GAsyncResult *res, void *user_data)
{
    GtkWidget *toplevel = GTK_WIDGET (user_data);
    SeahorseGpgmeUid *uid = SEAHORSE_GPGME_UID (source);
    g_autoptr(GError) error = NULL;

    if (!seahorse_gpgme_key_op_make_primary_finish (uid, res, &error)) {
        seahorse_util_show_error (toplevel,
                                  _("Couldn’t change primary user ID"),
                                  error->message);
    }
}

static void
on_uid_make_primary (GSimpleAction *action, GVariant *param, void *user_data)
{
    SeahorsePgpUidListBoxRow *row = SEAHORSE_PGP_UID_LIST_BOX_ROW (user_data);
    GtkWindow *toplevel;

    g_return_if_fail (SEAHORSE_GPGME_IS_UID (row->uid));

    /* Don't pass the row itself as user_data, as that might be destroyed as
     * part of the GListModel shuffle */
    toplevel = GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (row)));

    seahorse_gpgme_key_op_make_primary_async (SEAHORSE_GPGME_UID (row->uid),
                                              NULL,
                                              on_uid_make_primary_cb,
                                              toplevel);
}

static void
on_uid_sign (GSimpleAction *action, GVariant *param, void *user_data)
{
    SeahorsePgpUidListBoxRow *row = SEAHORSE_PGP_UID_LIST_BOX_ROW (user_data);
    SeahorseGpgmeSignDialog *dialog;

    g_return_if_fail (SEAHORSE_GPGME_IS_UID (row->uid));

    dialog = seahorse_gpgme_sign_dialog_new (SEAHORSE_ITEM (row->uid));
    gtk_window_present (GTK_WINDOW (dialog));
}

static const GActionEntry UID_ACTION_ENTRIES[] = {
    { "only-trusted", seahorse_util_toggle_action, NULL, "false", on_only_trusted_changed },
    { "delete", on_uid_delete },
    { "make-primary", on_uid_make_primary },
    { "sign", on_uid_sign },
    /* { "revoke",        on_uid_revoke        }, TODO */
};

static GtkWidget *
create_row_for_signature (void *item, void *user_data)
{
    SeahorsePgpUidListBoxRow *row = SEAHORSE_PGP_UID_LIST_BOX_ROW (user_data);
    SeahorsePgpSignature *signature = SEAHORSE_PGP_SIGNATURE (item);
    GtkWidget *sig_row;
    GtkWidget *box;
    const char *sig_keyid;
    GtkWidget *keyid_label;
    g_autofree char *signer_name = NULL;
    GtkWidget *signer_label;

    sig_row = gtk_list_box_row_new ();
    box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (sig_row), box);

    sig_keyid = seahorse_pgp_signature_get_keyid (signature);
    keyid_label = gtk_label_new (sig_keyid);
    gtk_box_append (GTK_BOX (box), keyid_label);

    for (GList *l = row->discovered_keys; l; l = g_list_next (l)) {
        if (SEAHORSE_PGP_IS_KEY (l->data)) {
            SeahorsePgpKey *key = SEAHORSE_PGP_KEY (l->data);

            if (seahorse_pgp_key_has_keyid (key, sig_keyid)) {
                const char *name = seahorse_pgp_key_get_primary_name (key);
                signer_name = g_strdup_printf ("(%s)", name);

                g_object_set_data (G_OBJECT (sig_row), "signer", l->data);
                break;
            }
        } else if (SEAHORSE_IS_UNKNOWN (l->data)) {
            g_autofree char *keyid = NULL;
            g_autofree char *label = NULL;

            g_object_get (l->data, "identifier", &keyid, "label", &label, NULL);
            if (seahorse_pgp_keyid_equal (sig_keyid, keyid)) {
                signer_name = g_strdup_printf ("(%s)", label);
                g_object_set_data (G_OBJECT (sig_row), "signer", l->data);
                break;
            }
        }
    }

    if (!signer_name) {
        /* Translators: (Unknown) signature name */
        signer_name = g_strdup (_("(Unknown)"));
    }

    signer_label = gtk_label_new (signer_name);
    gtk_box_append (GTK_BOX (box), signer_label);

    return sig_row;
}

static void
on_row_expanded (GObject *object,
                 GParamSpec *pspec,
                 void *user_data)
{
    SeahorsePgpUidListBoxRow *row = SEAHORSE_PGP_UID_LIST_BOX_ROW (object);
    gboolean expanded;
    GListModel *signatures;
    unsigned int n_signatures;
    g_autofree char *signed_by_str = NULL;
    g_autoptr(GCancellable) cancellable = NULL;
    g_autoptr(GPtrArray) keyids = NULL;

    /* Lazily discover keys by only loading when user actually expands the row
     * (showing the signatures) and not reloading if already done earlier */
    expanded = adw_expander_row_get_expanded (ADW_EXPANDER_ROW (row));
    if (!expanded || row->discovered_keys)
        return;

    signatures = seahorse_pgp_uid_get_signatures (row->uid);
    n_signatures = g_list_model_get_n_items (signatures);

    /* Discover the keys, so might be able to show their owner */
    keyids = g_ptr_array_new ();
    for (unsigned i = 0; i < n_signatures; i++) {
        g_autoptr(SeahorsePgpSignature) sig = NULL;

        sig = g_list_model_get_item (signatures, i);
        g_ptr_array_add (keyids, (void *) seahorse_pgp_signature_get_keyid (sig));
    }
    g_ptr_array_add (keyids, NULL);

    /* Pass it to the PGP backend for resolution/download, cancellable ties
     * search scope together */
    cancellable = g_cancellable_new ();
    row->discovered_keys =
        seahorse_pgp_backend_discover_keys (NULL,
                                            (const char **) keyids->pdata,
                                            cancellable);

    /* Now build the list */
    gtk_list_box_bind_model (GTK_LIST_BOX (row->signatures_list),
                             signatures,
                             create_row_for_signature,
                             row,
                             NULL);

    gtk_widget_set_visible (row->signatures_list, n_signatures > 0);
}

static void
update_row (SeahorsePgpUidListBoxRow *row)
{
    SeahorsePgpKey *parent_key;
    gboolean is_editable;
    g_autoptr(GString) title = NULL;
    const char *comment, *email;

    parent_key = seahorse_pgp_uid_get_parent (row->uid);
    is_editable = seahorse_pgp_key_is_private_key (parent_key);

    /* Put "name (comment)" as title */
    title = g_string_new (seahorse_pgp_uid_get_name (row->uid));
    comment = seahorse_pgp_uid_get_comment (row->uid);
    if (comment && *comment)
        g_string_append_printf (title, " (%s)", comment);
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), title->str);

    /* Make a linkified version the email as subtitle */
    email = seahorse_pgp_uid_get_email (row->uid);
    if (email && *email)
        adw_expander_row_set_subtitle (ADW_EXPANDER_ROW (row), email);

    /* Actions */
    gtk_widget_set_visible (row->actions_button, is_editable);

    if (is_editable) {
        update_actions (row);
    }

    /* Signatures we do at another point, since we try to lazy-load them */
}

static void
seahorse_pgp_uid_list_box_row_constructed (GObject *object)
{
    SeahorsePgpUidListBoxRow *row = SEAHORSE_PGP_UID_LIST_BOX_ROW (object);

    G_OBJECT_CLASS (seahorse_pgp_uid_list_box_row_parent_class)->constructed (object);

    update_row (row);
}

static void
seahorse_pgp_uid_list_box_row_init (SeahorsePgpUidListBoxRow *row)
{
    gtk_widget_init_template (GTK_WIDGET (row));

    row->actions = g_simple_action_group_new ();
    g_action_map_add_action_entries (G_ACTION_MAP (row->actions),
                                     UID_ACTION_ENTRIES,
                                     G_N_ELEMENTS (UID_ACTION_ENTRIES),
                                     row);

    gtk_widget_insert_action_group (GTK_WIDGET (row),
                                    "uid",
                                    G_ACTION_GROUP (row->actions));

    g_signal_connect (row, "notify::expanded", G_CALLBACK (on_row_expanded), NULL);
}

static void
seahorse_pgp_uid_list_box_row_finalize (GObject *object)
{
    SeahorsePgpUidListBoxRow *row = SEAHORSE_PGP_UID_LIST_BOX_ROW (object);

    g_clear_object (&row->uid);
    g_clear_pointer (&row->discovered_keys, g_list_free);

    G_OBJECT_CLASS (seahorse_pgp_uid_list_box_row_parent_class)->finalize (object);
}

static void
seahorse_pgp_uid_list_box_row_get_property (GObject      *object,
                                            unsigned int  prop_id,
                                            GValue       *value,
                                            GParamSpec   *pspec)
{
    SeahorsePgpUidListBoxRow *row = SEAHORSE_PGP_UID_LIST_BOX_ROW (object);

    switch (prop_id) {
    case ROW_PROP_UID:
        g_value_set_object (value, row->uid);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
seahorse_pgp_uid_list_box_row_set_property (GObject      *object,
                                            unsigned int  prop_id,
                                            const GValue *value,
                                            GParamSpec   *pspec)
{
    SeahorsePgpUidListBoxRow *row = SEAHORSE_PGP_UID_LIST_BOX_ROW (object);

    switch (prop_id) {
    case ROW_PROP_UID:
        row->uid = g_value_dup_object (value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
seahorse_pgp_uid_list_box_row_class_init (SeahorsePgpUidListBoxRowClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    gobject_class->constructed  = seahorse_pgp_uid_list_box_row_constructed;
    gobject_class->finalize     = seahorse_pgp_uid_list_box_row_finalize;
    gobject_class->set_property = seahorse_pgp_uid_list_box_row_set_property;
    gobject_class->get_property = seahorse_pgp_uid_list_box_row_get_property;

    g_object_class_install_property (gobject_class, ROW_PROP_UID,
        g_param_spec_object ("uid", "PGP uid", "The UID this row is showing",
                             SEAHORSE_PGP_TYPE_UID,
                             G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT));

    gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Seahorse/seahorse-pgp-uid-list-box-row.ui");

    gtk_widget_class_bind_template_child (widget_class, SeahorsePgpUidListBoxRow, actions_button);
    gtk_widget_class_bind_template_child (widget_class, SeahorsePgpUidListBoxRow, signatures_list);
}
