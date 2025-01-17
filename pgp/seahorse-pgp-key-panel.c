/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
 * Copyright (C) 2005 Jim Pharis
 * Copyright (C) 2005-2006 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
 * Copyright (C) 2019 Niels De Graef
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

#include "seahorse-pgp-key-panel.h"
#include "seahorse-gpgme-add-uid.h"
#include "seahorse-gpgme-dialogs.h"
#include "seahorse-gpgme-expires-dialog.h"
#include "seahorse-gpgme-key.h"
#include "seahorse-gpgme-key-export-operation.h"
#include "seahorse-gpgme-key-op.h"
#include "seahorse-gpgme-revoke-dialog.h"
#include "seahorse-gpgme-sign-dialog.h"
#include "seahorse-pgp-backend.h"
#include "seahorse-gpg-op.h"
#include "seahorse-pgp-dialogs.h"
#include "seahorse-pgp-key.h"
#include "seahorse-pgp-photos-widget.h"
#include "seahorse-pgp-signature.h"
#include "seahorse-pgp-subkey.h"
#include "seahorse-pgp-subkey-list-box.h"
#include "seahorse-pgp-uid.h"
#include "seahorse-pgp-uid-list-box.h"

#include "seahorse-common.h"

#include "libseahorse/seahorse-util.h"

#include <glib.h>
#include <glib/gi18n.h>

#include <string.h>

enum {
    PROP_0,
    PROP_KEY,
    N_PROPS
};
static GParamSpec *properties[N_PROPS] = { NULL, };

struct _SeahorsePgpKeyPanel {
    SeahorsePanel parent_instance;

    SeahorsePgpKey *key;

    /* Common widgets */
    GtkWidget *revoked_banner;
    GtkWidget *expired_banner;

    GtkWidget *photos_container;
    GtkWidget *name_label;
    GtkWidget *email_label;
    GtkWidget *comment_label;
    GtkWidget *keyid_label;
    GtkWidget *fingerprint_label;
    GtkWidget *expires_label;
    GtkWidget *owner_trust_row;
    GtkCustomFilter *owner_trust_filter;
    GtkWidget *uids_container;
    GtkWidget *subkeys_container;

    /* Public key widgets */
    GtkWidget *trust_page;
    GtkWidget *trust_sign_row;
    GtkWidget *trust_marginal_switch;
};

G_DEFINE_TYPE (SeahorsePgpKeyPanel, seahorse_pgp_key_panel, SEAHORSE_TYPE_PANEL)

static GtkWindow *
get_toplevel (SeahorsePgpKeyPanel *self)
{
    GtkRoot *root;

    root = gtk_widget_get_root (GTK_WIDGET (self));
    if (GTK_IS_WINDOW (root)) {
        return GTK_WINDOW (root);
    }

    return NULL;
}

static void
on_gpgme_key_change_pass_done (GObject      *source,
                               GAsyncResult *res,
                               void         *user_data)
{
    g_autoptr(SeahorsePgpKeyPanel) self = SEAHORSE_PGP_KEY_PANEL (user_data);
    SeahorseGpgmeKey *pkey = SEAHORSE_GPGME_KEY (source);
    g_autoptr(GError) error = NULL;

    if (!seahorse_gpgme_key_op_change_pass_finish (pkey, res, &error)) {
        GtkWindow *window;
        window = gtk_window_get_transient_for (get_toplevel (self));
        seahorse_util_show_error (GTK_WIDGET (window),
                                  _("Error changing password"),
                                  error->message);
    }
}

static void
action_change_password (GSimpleAction *action, GVariant *param, void *user_data)
{
    SeahorsePgpKeyPanel *self = SEAHORSE_PGP_KEY_PANEL (user_data);

    g_return_if_fail (SEAHORSE_GPGME_IS_KEY (self->key));
    g_return_if_fail (seahorse_pgp_key_is_private_key (self->key));

    seahorse_gpgme_key_op_change_pass_async (SEAHORSE_GPGME_KEY (self->key),
                                             NULL,
                                             on_gpgme_key_change_pass_done,
                                             g_object_ref (self));
}

static void
do_owner (SeahorsePgpKeyPanel *self)
{
    unsigned int flags;
    const char *label;
    GListModel *uids;
    g_autoptr(SeahorsePgpUid) primary_uid = NULL;
    GtkWidget *photos_widget;
    GDateTime *expires;
    g_autofree char *expires_str = NULL;

    flags = seahorse_item_get_item_flags (SEAHORSE_ITEM (self->key));

    /* Display appropriate warnings */
    gtk_widget_set_visible (self->expired_banner,
                            flags & SEAHORSE_FLAG_EXPIRED);
    gtk_widget_set_visible (self->revoked_banner,
                            flags & SEAHORSE_FLAG_REVOKED);

    /* Update the expired message */
    if (flags & SEAHORSE_FLAG_EXPIRED) {
        GDateTime *expires_date;
        g_autofree char *date_str = NULL;
        g_autofree char *message = NULL;

        expires_date = seahorse_pgp_key_get_expires (self->key);
        if (!expires_date) {
            /* TRANSLATORS: (unknown) expiry date */
            date_str = g_strdup (_("(unknown)"));
        } else {
            date_str = g_date_time_format (expires_date, "%x");
        }

        message = g_strdup_printf (_("This key expired on %s"), date_str);
        adw_banner_set_title (ADW_BANNER (self->expired_banner), message);
    }

    /* Hide trust page when above */
    if (self->trust_page != NULL) {
        gtk_widget_set_visible (self->trust_page, !((flags & SEAHORSE_FLAG_EXPIRED) ||
                                                    (flags & SEAHORSE_FLAG_REVOKED) ||
                                                    (flags & SEAHORSE_FLAG_DISABLED)));
    }

    uids = seahorse_pgp_key_get_uids (self->key);
    primary_uid = g_list_model_get_item (uids, 0);
    if (primary_uid != NULL) {
        g_autofree char *title = NULL;
        g_autofree char *email_escaped = NULL;
        g_autofree char *email_label = NULL;

        label = seahorse_pgp_uid_get_name (primary_uid);
        gtk_label_set_text (GTK_LABEL (self->name_label), label);

        label = seahorse_pgp_uid_get_email (primary_uid);
        if (label && *label) {
            email_escaped = g_markup_escape_text (label, -1);
            email_label = g_strdup_printf ("<a href=\"mailto:%s\">%s</a>", label, email_escaped);
            gtk_label_set_markup (GTK_LABEL (self->email_label), email_label);
            gtk_widget_set_visible (self->email_label, TRUE);
        } else {
            gtk_widget_set_visible (self->email_label, FALSE);
        }

        label = seahorse_pgp_uid_get_comment (primary_uid);
        if (label && *label) {
            gtk_label_set_markup (GTK_LABEL (self->comment_label), label);
            gtk_widget_set_visible (self->comment_label, TRUE);
        } else {
            gtk_widget_set_visible (self->comment_label, FALSE);
        }

        label = seahorse_pgp_key_get_keyid (self->key);
        gtk_label_set_text (GTK_LABEL (self->keyid_label), label);
    }

    photos_widget = seahorse_pgp_photos_widget_new (self->key);
    adw_bin_set_child (ADW_BIN (self->photos_container), photos_widget);

    gtk_label_set_text (GTK_LABEL (self->fingerprint_label),
                        seahorse_pgp_key_get_fingerprint (self->key));

    expires = seahorse_pgp_key_get_expires (self->key);
    if (expires)
        expires_str = g_date_time_format (expires, "%x");
    else
        expires_str = g_strdup (C_("Expires", "Never"));
    gtk_label_set_text (GTK_LABEL (self->expires_label), expires_str);
}

static void
on_owner_trust_selected_changed (GObject    *object,
                                 GParamSpec *pspec,
                                 void       *user_data)
{
    SeahorsePgpKeyPanel *self = SEAHORSE_PGP_KEY_PANEL (user_data);
    GObject *selected;
    int trust;

    g_return_if_fail (SEAHORSE_GPGME_IS_KEY (self->key));

    selected = adw_combo_row_get_selected_item (ADW_COMBO_ROW (self->owner_trust_row));
    if (selected == NULL)
        return;

    trust = adw_enum_list_item_get_value ((AdwEnumListItem *) selected);
    if (seahorse_pgp_key_get_trust (self->key) != trust) {
        gpgme_error_t err;

        err = seahorse_gpgme_key_op_set_trust (SEAHORSE_GPGME_KEY (self->key),
                                               trust);
        if (err)
            seahorse_gpgme_handle_error (err, _("Unable to change trust"));
    }
}

static void
on_export_op_execute_done (GObject *src_object,
                           GAsyncResult *result,
                           void *user_data)
{
    SeahorseExportOperation *export_op = SEAHORSE_EXPORT_OPERATION (src_object);
    SeahorsePgpKeyPanel *self = SEAHORSE_PGP_KEY_PANEL (user_data);
    g_autoptr(GError) error = NULL;
    gboolean success;

    success = seahorse_export_operation_execute_finish (export_op, result, &error);
    if (!success) {
        if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
            g_debug ("User cancelled export of key");
            return;
        }

        seahorse_util_show_error (GTK_WIDGET (self), _("Couldn't export key"), error->message);
        return;
    }
}

static void
on_export_op_prompt_for_file_done (GObject *src_object,
                                   GAsyncResult *result,
                                   void *user_data)
{
    SeahorseExportOperation *export_op = SEAHORSE_EXPORT_OPERATION (src_object);
    SeahorsePgpKeyPanel *self = SEAHORSE_PGP_KEY_PANEL (user_data);
    g_autoptr(GError) error = NULL;
    gboolean prompted;

    prompted = seahorse_export_operation_prompt_for_file_finish (export_op,
                                                                 result,
                                                                 &error);
    if (!prompted) {
        if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
            g_debug ("User cancelled export of key");
            return;
        }

        seahorse_util_show_error (GTK_WIDGET (self), _("Couldn't export key"), error->message);
        return;
    }

    seahorse_export_operation_execute (export_op, NULL, on_export_op_execute_done, self);
}

static void
export_key_to_file (SeahorsePgpKeyPanel *self, gboolean secret)
{
    g_autoptr(SeahorseExportOperation) export_op = NULL;

    g_return_if_fail (SEAHORSE_GPGME_IS_KEY (self->key));

    export_op = seahorse_gpgme_key_export_operation_new (SEAHORSE_GPGME_KEY (self->key), TRUE, secret);
    seahorse_export_operation_prompt_for_file (export_op,
                                               get_toplevel (self),
                                               NULL,
                                               on_export_op_prompt_for_file_done,
                                               self);
}

static void
action_export_secret (GSimpleAction *action, GVariant *param, void *user_data)
{
    SeahorsePgpKeyPanel *self = SEAHORSE_PGP_KEY_PANEL (user_data);

    export_key_to_file (self, TRUE);
}

static void
action_export_public (GSimpleAction *action, GVariant *param, void *user_data)
{
    SeahorsePgpKeyPanel *self = SEAHORSE_PGP_KEY_PANEL (user_data);

    export_key_to_file (self, FALSE);
}

static void
action_change_expires (GSimpleAction *action, GVariant *param, void *user_data)
{
    SeahorsePgpKeyPanel *self = SEAHORSE_PGP_KEY_PANEL (user_data);
    GListModel *subkeys;
    g_autoptr(SeahorseGpgmeSubkey) subkey = NULL;
    GtkWidget *dialog;
    GtkWindow *parent;

    subkeys = seahorse_pgp_key_get_subkeys (self->key);
    g_return_if_fail (g_list_model_get_n_items (subkeys) > 0);

    subkey = SEAHORSE_GPGME_SUBKEY (g_list_model_get_item (subkeys, 0));
    g_return_if_fail (subkey);

    dialog = seahorse_gpgme_expires_dialog_new (subkey);
    parent = get_toplevel (self);
    adw_dialog_present (ADW_DIALOG (dialog), parent? GTK_WIDGET (parent) : NULL);
}

static gboolean
key_trust_filter_func (void *object, void *user_data)
{
    AdwEnumListItem *item = ADW_ENUM_LIST_ITEM (object);
    SeahorsePgpKeyPanel *self = SEAHORSE_PGP_KEY_PANEL (user_data);
    int trust;

    trust = adw_enum_list_item_get_value (item);
    switch (trust) {
    /* Never shown as an option */
    case SEAHORSE_VALIDITY_REVOKED:
    case SEAHORSE_VALIDITY_DISABLED:
        return FALSE;
    /* Only for public keys */
    case SEAHORSE_VALIDITY_NEVER:
        return ! seahorse_pgp_key_is_private_key (self->key);
    /* Shown for both public/private */
    case SEAHORSE_VALIDITY_UNKNOWN:
    case SEAHORSE_VALIDITY_MARGINAL:
    case SEAHORSE_VALIDITY_FULL:
        return TRUE;
    /* Only for private keys */
    case SEAHORSE_VALIDITY_ULTIMATE:
        return seahorse_pgp_key_is_private_key (self->key);
    }

    g_return_val_if_reached (FALSE);
}

static char *
pgp_trust_to_string (void *user_data,
                     SeahorseValidity validity)
{
  return g_strdup (seahorse_validity_get_string (validity));
}

static void
setup_trust_dropdown (SeahorsePgpKeyPanel *self)
{
    gtk_custom_filter_set_filter_func (self->owner_trust_filter,
                                       key_trust_filter_func,
                                       self,
                                       NULL);
}

static void
do_details (SeahorsePgpKeyPanel *self)
{
    AdwComboRow *owner_trust_row = ADW_COMBO_ROW (self->owner_trust_row);
    SeahorseFlags flags;
    GListModel *model;
    int trust;

    if (!seahorse_pgp_key_is_private_key (self->key)) {
        gtk_widget_set_visible (self->owner_trust_row,
                                SEAHORSE_GPGME_IS_KEY (self->key));
    }

    flags = seahorse_item_get_item_flags (SEAHORSE_ITEM (self->key));
    gtk_widget_set_sensitive (self->owner_trust_row,
                              !(flags & SEAHORSE_FLAG_DISABLED));

    trust = seahorse_pgp_key_get_trust (self->key);
    model = adw_combo_row_get_model (owner_trust_row);
    for (unsigned int i = 0; i < g_list_model_get_n_items (model); i++) {
        g_autoptr(AdwEnumListItem) item = g_list_model_get_item (model, i);

        if (trust == adw_enum_list_item_get_value (item)) {
            adw_combo_row_set_selected (owner_trust_row, i);
            break;
        }
    }
}

static void
on_trust_marginal_switch_notify_active (GObject    *object,
                                        GParamSpec *new_state,
                                        void       *user_data)
{
    SeahorsePgpKeyPanel *self = SEAHORSE_PGP_KEY_PANEL (user_data);
    SeahorseValidity trust;
    gpgme_error_t err;

    g_return_if_fail (SEAHORSE_GPGME_IS_KEY (self->key));

    trust = adw_switch_row_get_active (ADW_SWITCH_ROW (object)) ?
            SEAHORSE_VALIDITY_MARGINAL : SEAHORSE_VALIDITY_UNKNOWN;

    if (seahorse_pgp_key_get_trust (self->key) != trust) {
        err = seahorse_gpgme_key_op_set_trust (SEAHORSE_GPGME_KEY (self->key), trust);
        if (err)
            seahorse_gpgme_handle_error (err, _("Unable to change trust"));
    }
}

/* Add a signature */
static void
action_sign_key (GSimpleAction *action, GVariant *param, void *user_data)
{
    SeahorsePgpKeyPanel *self = SEAHORSE_PGP_KEY_PANEL (user_data);
    SeahorseGpgmeSignDialog *dialog;

    g_return_if_fail (SEAHORSE_GPGME_IS_KEY (self->key));

    dialog = seahorse_gpgme_sign_dialog_new (SEAHORSE_ITEM (self->key));
    gtk_window_present (GTK_WINDOW (dialog));
}

static gboolean
key_have_signatures (SeahorsePgpKey *pkey, unsigned int types)
{
    GListModel *uids;

    uids = seahorse_pgp_key_get_uids (pkey);
    for (unsigned int i = 0; i < g_list_model_get_n_items (uids); i++) {
        g_autoptr(SeahorsePgpUid) uid = g_list_model_get_item (uids, i);
        GListModel *sigs;

        sigs = seahorse_pgp_uid_get_signatures (uid);
        for (unsigned int j = 0; j < g_list_model_get_n_items (sigs); j++) {
            g_autoptr(SeahorsePgpSignature) sig = g_list_model_get_item (sigs, j);
            if (seahorse_pgp_signature_get_sigtype (sig) & types)
                return TRUE;
        }
    }

    return FALSE;
}

static void
do_trust (SeahorsePgpKeyPanel *self)
{
    gboolean sigpersonal;

    if (seahorse_pgp_key_is_private_key (self->key))
        return;

    /* Remote keys */
    if (!SEAHORSE_GPGME_IS_KEY (self->key)) {
        gtk_widget_set_visible (self->trust_marginal_switch, TRUE);
        gtk_widget_set_sensitive (self->trust_marginal_switch, FALSE);
        gtk_widget_set_visible (self->trust_sign_row, FALSE);

    /* Local keys */
    } else {
        unsigned int trust;
        gboolean managed = FALSE;

        trust = seahorse_pgp_key_get_trust (self->key);

        switch (trust) {
        /* We shouldn't be seeing this page with these trusts */
        case SEAHORSE_VALIDITY_REVOKED:
        case SEAHORSE_VALIDITY_DISABLED:
            return;
        /* Trust is specified manually */
        case SEAHORSE_VALIDITY_ULTIMATE:
        case SEAHORSE_VALIDITY_NEVER:
            managed = FALSE;
            break;
        /* We manage the trust through this page */
        case SEAHORSE_VALIDITY_FULL:
        case SEAHORSE_VALIDITY_MARGINAL:
        case SEAHORSE_VALIDITY_UNKNOWN:
            managed = TRUE;
            break;
        default:
            g_warning ("unknown trust value: %d", trust);
            g_assert_not_reached ();
            return;
        }

        /* Managed and unmanaged areas */
        gtk_widget_set_visible (self->trust_marginal_switch, managed);

        /* Managed check boxes */
        if (managed) {
            adw_switch_row_set_active (ADW_SWITCH_ROW (self->trust_marginal_switch),
                                       (trust != SEAHORSE_VALIDITY_UNKNOWN));
        }

        /* Signing and revoking */
        sigpersonal = key_have_signatures (self->key, SKEY_PGPSIG_PERSONAL);
        gtk_widget_set_visible (self->trust_sign_row, !sigpersonal);
    }
}

static const GActionEntry PRIVATE_KEY_ACTIONS[] = {
    { "change-password", action_change_password },
    { "change-expires", action_change_expires },
    { "export-secret", action_export_secret },
    { "export-public", action_export_public },
};

static const GActionEntry PUBLIC_KEY_ACTIONS[] = {
    { "sign-key", action_sign_key },
    { "export-public", action_export_public },
};

static void
key_notify (GObject *object, GParamSpec *pspec, void *user_data)
{
    SeahorsePgpKeyPanel *self = SEAHORSE_PGP_KEY_PANEL (user_data);

    do_owner (self);
    do_trust (self);
    do_details (self);
}

static void
create_public_key_dialog (SeahorsePgpKeyPanel *self)
{
    GSimpleActionGroup *action_group;
    const char *user;
    g_autofree char *sign_text = NULL;
    g_autofree char *sign_text_esc = NULL;

    action_group = seahorse_panel_get_actions (SEAHORSE_PANEL (self));
    g_action_map_add_action_entries (G_ACTION_MAP (action_group),
                                     PUBLIC_KEY_ACTIONS,
                                     G_N_ELEMENTS (PUBLIC_KEY_ACTIONS),
                                     self);
    gtk_widget_insert_action_group (GTK_WIDGET (self),
                                    "panel",
                                    G_ACTION_GROUP (action_group));

    setup_trust_dropdown (self);
    do_owner (self);
    do_details (self);
    do_trust (self);

    /* Fill in trust labels with name. */
    user = seahorse_item_get_title (SEAHORSE_ITEM (self->key));

    sign_text = g_strdup_printf(_("I believe “%s” is the owner of this key"),
                                user);
    sign_text_esc = g_markup_escape_text (sign_text, -1);
    adw_action_row_set_subtitle (ADW_ACTION_ROW (self->trust_sign_row), sign_text_esc);
    adw_action_row_set_subtitle_lines (ADW_ACTION_ROW (self->trust_sign_row), 0);
}

static void
create_private_key_dialog (SeahorsePgpKeyPanel *self)
{
    GSimpleActionGroup *action_group;

    action_group = seahorse_panel_get_actions (SEAHORSE_PANEL (self));
    g_action_map_add_action_entries (G_ACTION_MAP (action_group),
                                     PRIVATE_KEY_ACTIONS,
                                     G_N_ELEMENTS (PRIVATE_KEY_ACTIONS),
                                     self);
    gtk_widget_insert_action_group (GTK_WIDGET (self),
                                    "panel",
                                    G_ACTION_GROUP (action_group));

    setup_trust_dropdown (self);
    do_owner (self);
    do_details (self);
}

static void
seahorse_pgp_key_panel_get_property (GObject      *object,
                                     unsigned int  prop_id,
                                     GValue       *value,
                                     GParamSpec   *pspec)
{
    SeahorsePgpKeyPanel *self = SEAHORSE_PGP_KEY_PANEL (object);

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
seahorse_pgp_key_panel_set_property (GObject      *object,
                                     unsigned int  prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
    SeahorsePgpKeyPanel *self = SEAHORSE_PGP_KEY_PANEL (object);

    switch (prop_id) {
    case PROP_KEY:
        g_set_object (&self->key, g_value_get_object (value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
seahorse_pgp_key_panel_finalize (GObject *obj)
{
    SeahorsePgpKeyPanel *self = SEAHORSE_PGP_KEY_PANEL (obj);

    g_clear_object (&self->key);

    G_OBJECT_CLASS (seahorse_pgp_key_panel_parent_class)->finalize (obj);
}

static void
seahorse_pgp_key_panel_dispose (GObject *obj)
{
    SeahorsePgpKeyPanel *self = SEAHORSE_PGP_KEY_PANEL (obj);
    GtkWidget *child = NULL;

    while ((child = gtk_widget_get_first_child (GTK_WIDGET (self))) != NULL)
        gtk_widget_unparent (child);

    G_OBJECT_CLASS (seahorse_pgp_key_panel_parent_class)->dispose (obj);
}

static void
seahorse_pgp_key_panel_constructed (GObject *obj)
{
    SeahorsePgpKeyPanel *self = SEAHORSE_PGP_KEY_PANEL (obj);
    GtkWidget *uids_listbox, *subkeys_listbox;
    const char *name;
    g_autofree char *title = NULL;
    gboolean is_public_key;

    G_OBJECT_CLASS (seahorse_pgp_key_panel_parent_class)->constructed (obj);

    is_public_key = ! seahorse_pgp_key_is_private_key (self->key);
    if (is_public_key)
        create_public_key_dialog (self);
    else
        create_private_key_dialog (self);

    uids_listbox = seahorse_pgp_uid_list_box_new (self->key);
    gtk_box_append (GTK_BOX (self->uids_container), uids_listbox);

    subkeys_listbox = seahorse_pgp_subkey_list_box_new (self->key);
    gtk_box_append (GTK_BOX (self->subkeys_container), subkeys_listbox);

    /* Some trust rows are only make sense for public keys */
    gtk_widget_set_visible (self->trust_page, is_public_key);
    gtk_widget_set_visible (self->owner_trust_row, is_public_key);
    gtk_widget_set_visible (self->trust_sign_row, is_public_key);
    gtk_widget_set_visible (self->trust_marginal_switch, is_public_key);

    g_signal_connect_object (self->key, "notify",
                             G_CALLBACK (key_notify), self, 0);

    /* Titlebar */
    name = seahorse_pgp_key_get_primary_name (self->key);

    /* Translators: the 1st part of the title is the owner's name */
    title = (is_public_key)? g_strdup_printf (_("%s — Public key"), name)
                           : g_strdup_printf (_("%s — Private key"), name);
    seahorse_panel_set_title (SEAHORSE_PANEL (self), title);
}

static void
seahorse_pgp_key_panel_init (SeahorsePgpKeyPanel *self)
{
    gtk_widget_init_template (GTK_WIDGET (self));
}

static void
seahorse_pgp_key_panel_class_init (SeahorsePgpKeyPanelClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    gobject_class->constructed = seahorse_pgp_key_panel_constructed;
    gobject_class->get_property = seahorse_pgp_key_panel_get_property;
    gobject_class->set_property = seahorse_pgp_key_panel_set_property;
    gobject_class->dispose = seahorse_pgp_key_panel_dispose;
    gobject_class->finalize = seahorse_pgp_key_panel_finalize;

    properties[PROP_KEY] =
        g_param_spec_object ("key", NULL, NULL,
                             SEAHORSE_PGP_TYPE_KEY,
                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
    g_object_class_install_properties (gobject_class, N_PROPS, properties);

    gtk_widget_class_set_template_from_resource (widget_class,
                                                 "/org/gnome/Seahorse/seahorse-pgp-key-panel.ui");
    gtk_widget_class_bind_template_child (widget_class,
                                          SeahorsePgpKeyPanel,
                                          photos_container);
    gtk_widget_class_bind_template_child (widget_class,
                                          SeahorsePgpKeyPanel,
                                          name_label);
    gtk_widget_class_bind_template_child (widget_class,
                                          SeahorsePgpKeyPanel,
                                          email_label);
    gtk_widget_class_bind_template_child (widget_class,
                                          SeahorsePgpKeyPanel,
                                          comment_label);
    gtk_widget_class_bind_template_child (widget_class,
                                          SeahorsePgpKeyPanel,
                                          keyid_label);
    gtk_widget_class_bind_template_child (widget_class,
                                          SeahorsePgpKeyPanel,
                                          fingerprint_label);
    gtk_widget_class_bind_template_child (widget_class,
                                          SeahorsePgpKeyPanel,
                                          expires_label);
    gtk_widget_class_bind_template_child (widget_class,
                                          SeahorsePgpKeyPanel,
                                          revoked_banner);
    gtk_widget_class_bind_template_child (widget_class,
                                          SeahorsePgpKeyPanel,
                                          expired_banner);
    gtk_widget_class_bind_template_child (widget_class,
                                          SeahorsePgpKeyPanel,
                                          owner_trust_row);
    gtk_widget_class_bind_template_child (widget_class,
                                          SeahorsePgpKeyPanel,
                                          owner_trust_filter);
    gtk_widget_class_bind_template_child (widget_class,
                                          SeahorsePgpKeyPanel,
                                          uids_container);
    gtk_widget_class_bind_template_child (widget_class,
                                          SeahorsePgpKeyPanel,
                                          subkeys_container);

    /* public keys only */
    gtk_widget_class_bind_template_child (widget_class,
                                          SeahorsePgpKeyPanel,
                                          trust_page);
    gtk_widget_class_bind_template_child (widget_class,
                                          SeahorsePgpKeyPanel,
                                          trust_sign_row);
    gtk_widget_class_bind_template_child (widget_class,
                                          SeahorsePgpKeyPanel,
                                          trust_marginal_switch);

    gtk_widget_class_bind_template_callback (widget_class,
                                             on_owner_trust_selected_changed);
    gtk_widget_class_bind_template_callback (widget_class,
                                             pgp_trust_to_string);
    gtk_widget_class_bind_template_callback (widget_class,
                                             on_trust_marginal_switch_notify_active);
}

GtkWidget *
seahorse_pgp_key_panel_new (SeahorsePgpKey *pkey)
{
    g_autoptr(SeahorsePgpKeyPanel) self = NULL;

    /* This causes the key source to get any specific info about the key */
    if (SEAHORSE_GPGME_IS_KEY (pkey)) {
        seahorse_gpgme_key_refresh (SEAHORSE_GPGME_KEY (pkey));
        seahorse_gpgme_key_ensure_signatures (SEAHORSE_GPGME_KEY (pkey));
    }

    self = g_object_new (SEAHORSE_PGP_TYPE_KEY_PANEL,
                         "key", pkey,
                         NULL);

    return GTK_WIDGET (g_steal_pointer (&self));
}
