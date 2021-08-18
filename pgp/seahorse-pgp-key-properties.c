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

#include "seahorse-pgp-dialogs.h"
#include "seahorse-gpgme-add-subkey.h"
#include "seahorse-gpgme-add-uid.h"
#include "seahorse-gpgme-dialogs.h"
#include "seahorse-gpgme-expires-dialog.h"
#include "seahorse-gpgme-exporter.h"
#include "seahorse-gpgme-key.h"
#include "seahorse-gpgme-key-op.h"
#include "seahorse-gpgme-revoke-dialog.h"
#include "seahorse-gpgme-sign-dialog.h"
#include "seahorse-pgp-backend.h"
#include "seahorse-gpg-op.h"
#include "seahorse-pgp-dialogs.h"
#include "seahorse-pgp-key.h"
#include "seahorse-pgp-uid.h"
#include "seahorse-pgp-uid-list-box.h"
#include "seahorse-pgp-signature.h"
#include "seahorse-pgp-subkey.h"
#include "seahorse-pgp-subkey-list-box.h"

#include "seahorse-common.h"

#include "libseahorse/seahorse-util.h"

#include <glib.h>
#include <glib/gi18n.h>

#include <string.h>

#define PUBLIC_KEY_PROPERTIES_UI "/org/gnome/Seahorse/seahorse-pgp-public-key-properties.ui"
#define PRIVATE_KEY_PROPERTIES_UI  "/org/gnome/Seahorse/seahorse-pgp-private-key-properties.ui"

enum {
    PROP_0,
    PROP_KEY,
};

struct _SeahorsePgpKeyProperties {
    GtkDialog parent_instance;

    SeahorsePgpKey *key;

    GSimpleActionGroup *action_group;

    /* Common widgets */
    GtkWidget *revoked_area;
    GtkWidget *expired_area;
    GtkWidget *expired_message;

    GtkImage *photoid;
    GtkEventBox *photo_event_box;

    GtkWidget *name_label;
    GtkWidget *email_label;
    GtkWidget *comment_label;
    GtkWidget *keyid_label;
    GtkWidget *fingerprint_label;
    GtkWidget *expires_label;
    GtkWidget *photo_previous_button;
    GtkWidget *photo_next_button;
    GtkWidget *ownertrust_combobox;
    GtkWidget *uids_container;
    GtkWidget *subkeys_container;

    /* Private key widgets */
    GMenuModel *actions_menu;
    GtkWidget *owner_photo_frame;
    GtkWidget *owner_photo_add_button;
    GtkWidget *owner_photo_delete_button;
    GtkWidget *owner_photo_primary_button;

    /* Public key widgets */
    GtkWidget *indicate_trust_row;
    GtkWidget *trust_page;
    GtkWidget *trust_sign_row;
    GtkWidget *trust_revoke_row;
    GtkWidget *trust_marginal_switch;
};

G_DEFINE_TYPE (SeahorsePgpKeyProperties, seahorse_pgp_key_properties, GTK_TYPE_DIALOG)

static void
set_action_enabled (SeahorsePgpKeyProperties *self,
                    const char               *action_str,
                    gboolean                  enabled)
{
    GAction *action;

    action = g_action_map_lookup_action (G_ACTION_MAP (self->action_group),
                                         action_str);
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action), enabled);
}

static void
on_uids_add (GSimpleAction *action, GVariant *param, void *user_data)
{
    SeahorsePgpKeyProperties *self = SEAHORSE_PGP_KEY_PROPERTIES (user_data);
    g_return_if_fail (SEAHORSE_GPGME_IS_KEY (self->key));
    seahorse_gpgme_add_uid_run_dialog (SEAHORSE_GPGME_KEY (self->key),
                                       GTK_WINDOW (self));
}

/* drag-n-drop uri data */
enum {
    TARGET_URI,
};

static GtkTargetEntry target_list[] = {
    { "text/uri-list", 0, TARGET_URI } };

static void
on_pgp_owner_photo_drag_received (GtkWidget        *widget,
                                  GdkDragContext   *context,
                                  int               x,
                                  int               y,
                                  GtkSelectionData *sel_data,
                                  unsigned int      target_type,
                                  unsigned int      time,
                                  void             *user_data)
{
    SeahorsePgpKeyProperties *self = SEAHORSE_PGP_KEY_PROPERTIES (user_data);
    gboolean dnd_success = FALSE;
    int len = 0;

    g_return_if_fail (SEAHORSE_GPGME_IS_KEY (self->key));

    /*
     * This needs to be improved, support should be added for remote images
     * and there has to be a better way to get rid of the trailing \r\n appended
     * to the end of the path after the call to g_filename_from_uri
     */
    if ((sel_data != NULL) && (gtk_selection_data_get_length (sel_data) >= 0)) {
        g_auto(GStrv) uri_list = NULL;

        g_return_if_fail (target_type == TARGET_URI);

        uri_list = gtk_selection_data_get_uris (sel_data);
        while (uri_list && uri_list[len]) {
            g_autofree char *uri = NULL;

            uri = g_filename_from_uri (uri_list[len], NULL, NULL);
            if (!uri)
                continue;

            dnd_success = seahorse_gpgme_photo_add (SEAHORSE_GPGME_KEY (self->key),
                                                    GTK_WINDOW (self), uri);
            if (!dnd_success)
                break;
            len++;
        }
    }

    gtk_drag_finish (context, dnd_success, FALSE, time);
}

static void
on_photos_add (GSimpleAction *action, GVariant *param, void *user_data)
{
    SeahorsePgpKeyProperties *self = SEAHORSE_PGP_KEY_PROPERTIES (user_data);

    g_return_if_fail (SEAHORSE_GPGME_IS_KEY (self->key));

    if (seahorse_gpgme_photo_add (SEAHORSE_GPGME_KEY (self->key),
                                  GTK_WINDOW (self), NULL))
        g_object_set_data (G_OBJECT (self), "current-photoid", NULL);
}

static void
on_photos_delete (GSimpleAction *action, GVariant *param, void *user_data)
{
    SeahorsePgpKeyProperties *self = SEAHORSE_PGP_KEY_PROPERTIES (user_data);
    SeahorseGpgmePhoto *photo;

    photo = g_object_get_data (G_OBJECT (self), "current-photoid");
    g_return_if_fail (SEAHORSE_IS_GPGME_PHOTO (photo));

    if (seahorse_gpgme_key_op_photo_delete (photo))
        g_object_set_data (G_OBJECT (self), "current-photoid", NULL);
}

static void
on_photos_make_primary (GSimpleAction *action, GVariant *param, void *user_data)
{
    SeahorsePgpKeyProperties *self = SEAHORSE_PGP_KEY_PROPERTIES (user_data);
    gpgme_error_t gerr;
    SeahorseGpgmePhoto *photo;

    photo = g_object_get_data (G_OBJECT (self), "current-photoid");
    g_return_if_fail (SEAHORSE_IS_GPGME_PHOTO (photo));

    gerr = seahorse_gpgme_key_op_photo_primary (photo);
    if (!GPG_IS_OK (gerr))
        seahorse_gpgme_handle_error (gerr, _("Couldn’t change primary photo"));
}

static void
set_photoid_state (SeahorsePgpKeyProperties *self)
{
    SeahorseUsage etype;
    SeahorsePgpPhoto *photo;
    gboolean is_gpgme;
    GdkPixbuf *pixbuf;
    GListModel *photos;
    unsigned int n_photos;
    g_autoptr(SeahorsePgpPhoto) first_photo = NULL;
    g_autoptr(SeahorsePgpPhoto) last_photo = NULL;

    etype = seahorse_object_get_usage (SEAHORSE_OBJECT (self->key));
    photos = seahorse_pgp_key_get_photos (self->key);
    n_photos = g_list_model_get_n_items (photos);

    photo = g_object_get_data (G_OBJECT (self), "current-photoid");
    g_return_if_fail (!photo || SEAHORSE_PGP_IS_PHOTO (photo));
    is_gpgme = SEAHORSE_GPGME_IS_KEY (self->key);

    if (etype == SEAHORSE_USAGE_PRIVATE_KEY) {
        gtk_widget_set_visible (self->owner_photo_add_button, is_gpgme);
        gtk_widget_set_visible (self->owner_photo_primary_button,
                                is_gpgme && n_photos> 1);
        gtk_widget_set_visible (self->owner_photo_delete_button,
                                is_gpgme && photo);
    }

    /* Display both of these when there is more than one photo id */
    gtk_widget_set_visible (self->photo_previous_button, n_photos > 1);
    gtk_widget_set_visible (self->photo_next_button, n_photos > 1);

    /* Change sensitivity if first/last photo id */
    first_photo = g_list_model_get_item (photos, 0);
    last_photo = g_list_model_get_item (photos, MAX (n_photos - 1, 0));
    set_action_enabled (self, "photos.previous",
                        photo && photo != first_photo);
    set_action_enabled (self, "photos.next",
                        photo && photo != last_photo);

    pixbuf = photo ? seahorse_pgp_photo_get_pixbuf (photo) : NULL;
    if (pixbuf)
        gtk_image_set_from_pixbuf (self->photoid, pixbuf);
}

static void
do_photo_id (SeahorsePgpKeyProperties *self)
{
    GListModel *photos;
    g_autoptr(SeahorsePgpPhoto) first_photo = NULL;

    photos = seahorse_pgp_key_get_photos (self->key);
    first_photo = g_list_model_get_item (photos, 0);
    if (first_photo)
        g_object_set_data_full (G_OBJECT (self), "current-photoid",
                                g_steal_pointer (&first_photo), g_object_unref);
    else
        g_object_set_data (G_OBJECT (self), "current-photoid", NULL);

    set_photoid_state (self);
}

static unsigned int
find_photo_index (GListModel *photos, SeahorsePgpPhoto *photo)
{
    for (unsigned int i = 0; i < g_list_model_get_n_items (photos); i++) {
        g_autoptr(SeahorsePgpPhoto) foto = g_list_model_get_item (photos, i);

        if (photo == foto)
            return i;
    }

    g_return_val_if_reached (0);
}

static void
on_photos_next (GSimpleAction *action, GVariant *param, void *user_data)
{
    SeahorsePgpKeyProperties *self = SEAHORSE_PGP_KEY_PROPERTIES (user_data);
    SeahorsePgpPhoto *photo;
    GListModel *photos;

    photos = seahorse_pgp_key_get_photos (self->key);
    photo = g_object_get_data (G_OBJECT (self), "current-photoid");
    if (photo) {
        unsigned int index;
        g_autoptr(SeahorsePgpPhoto) next = NULL;

        index = find_photo_index (photos, photo);
        next = g_list_model_get_item (photos, index + 1);
        if (next)
            g_object_set_data_full (G_OBJECT (self), "current-photoid",
                                    g_steal_pointer (&next),
                                    g_object_unref);
    }

    set_photoid_state (self);
}

static void
on_photos_previous (GSimpleAction *action, GVariant *param, void *user_data)
{
    SeahorsePgpKeyProperties *self = SEAHORSE_PGP_KEY_PROPERTIES (user_data);
    SeahorsePgpPhoto *photo;
    GListModel *photos;

    photos = seahorse_pgp_key_get_photos (self->key);
    photo = g_object_get_data (G_OBJECT (self), "current-photoid");
    if (photo) {
        unsigned int index;

        index = find_photo_index (photos, photo);
        if (index > 0) {
            g_autoptr(SeahorsePgpPhoto) prev = NULL;

            prev = g_list_model_get_item (photos, index - 1);
            g_object_set_data_full (G_OBJECT (self), "current-photoid",
                                    g_steal_pointer (&prev),
                                    g_object_unref);
        }
    }

    set_photoid_state (self);
}

static void
on_pgp_owner_photoid_button (GtkWidget *widget,
                             GdkEvent  *event,
                             void      *user_data)
{
    SeahorsePgpKeyProperties *self = SEAHORSE_PGP_KEY_PROPERTIES (user_data);
    const char *action_str;

    if (event->type != GDK_SCROLL)
        return;

    switch (((GdkEventScroll *) event)->direction) {
    case GDK_SCROLL_UP:
        action_str = "photos.previous";
        break;
    case GDK_SCROLL_DOWN:
        action_str = "photos.next";
        break;
    default:
        return;
    }

    g_action_group_activate_action (G_ACTION_GROUP (self->action_group),
                                    action_str, NULL);
}

static void
on_gpgme_key_change_pass_done (GObject      *source,
                               GAsyncResult *res,
                               void         *user_data)
{
    g_autoptr(SeahorsePgpKeyProperties) self = SEAHORSE_PGP_KEY_PROPERTIES (user_data);
    SeahorseGpgmeKey *pkey = SEAHORSE_GPGME_KEY (source);
    g_autoptr(GError) error = NULL;

    if (!seahorse_gpgme_key_op_change_pass_finish (pkey, res, &error)) {
        GtkWindow *window;
        window = gtk_window_get_transient_for (GTK_WINDOW (self));
        seahorse_util_show_error (GTK_WIDGET (window),
                                  _("Error changing password"),
                                  error->message);
    }
}

static void
on_change_password (GSimpleAction *action, GVariant *param, void *user_data)
{
    SeahorsePgpKeyProperties *self = SEAHORSE_PGP_KEY_PROPERTIES (user_data);
    SeahorseUsage usage;

    usage = seahorse_object_get_usage (SEAHORSE_OBJECT (self->key));
    g_return_if_fail (usage == SEAHORSE_USAGE_PRIVATE_KEY);
    g_return_if_fail (SEAHORSE_GPGME_IS_KEY (self->key));

    seahorse_gpgme_key_op_change_pass_async (SEAHORSE_GPGME_KEY (self->key),
                                             NULL,
                                             on_gpgme_key_change_pass_done,
                                             g_object_ref (self));
}

static void
setup_titlebar (SeahorsePgpKeyProperties *self)
{
    const char *name;
    GtkWidget *headerbar;
    GtkWidget *menu_icon;
    GtkWidget *menu_button;
    g_autofree char *title = NULL;

    name = seahorse_pgp_key_get_primary_name (self->key);

    headerbar = gtk_header_bar_new ();
    gtk_header_bar_set_show_close_button (GTK_HEADER_BAR (headerbar), TRUE);
    gtk_widget_show (headerbar);

    menu_button = gtk_menu_button_new ();
    menu_icon = gtk_image_new_from_icon_name ("open-menu-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_button_set_image (GTK_BUTTON (menu_button), menu_icon);
    gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (menu_button), self->actions_menu);

    gtk_header_bar_pack_start (GTK_HEADER_BAR (headerbar), menu_button);

    if (seahorse_pgp_key_is_private_key (self->key)) {
        /* Translators: the 1st part of the title is the owner's name */
        title = g_strdup_printf (_("%s — Private key"), name);
        gtk_widget_show (menu_button);
    } else {
        /* Translators: the 1st part of the title is the owner's name */
        title = g_strdup_printf (_("%s — Public key"), name);
        gtk_widget_hide (menu_button);
    }
    gtk_header_bar_set_title (GTK_HEADER_BAR (headerbar), title);

    gtk_window_set_titlebar (GTK_WINDOW (self), headerbar);
}

static void
do_owner (SeahorsePgpKeyProperties *self)
{
    unsigned int flags;
    const char *label;
    GListModel *uids;
    g_autoptr(SeahorsePgpUid) primary_uid = NULL;
    GDateTime *expires;
    g_autofree char *expires_str = NULL;

    flags = seahorse_object_get_flags (SEAHORSE_OBJECT (self->key));

    /* Display appropriate warnings */
    gtk_widget_set_visible (self->expired_area, flags & SEAHORSE_FLAG_EXPIRED);
    gtk_widget_set_visible (self->revoked_area, flags & SEAHORSE_FLAG_REVOKED);

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
        gtk_label_set_text (GTK_LABEL (self->expired_message), message);
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
            gtk_widget_show (self->email_label);
        } else {
            gtk_widget_hide (self->email_label);
        }

        label = seahorse_pgp_uid_get_comment (primary_uid);
        if (label && *label) {
            gtk_label_set_markup (GTK_LABEL (self->comment_label), label);
            gtk_widget_show (self->comment_label);
        } else {
            gtk_widget_hide (self->comment_label);
        }

        label = seahorse_object_get_identifier (SEAHORSE_OBJECT (self->key));
        gtk_label_set_text (GTK_LABEL (self->keyid_label), label);
    }

    gtk_label_set_text (GTK_LABEL (self->fingerprint_label),
                        seahorse_pgp_key_get_fingerprint (self->key));

    expires = seahorse_pgp_key_get_expires (self->key);
    if (expires)
        expires_str = g_date_time_format (expires, "%x");
    else
        expires_str = g_strdup (C_("Expires", "Never"));
    gtk_label_set_text (GTK_LABEL (self->expires_label), expires_str);

    setup_titlebar (self);
    do_photo_id (self);
}

/* trust combo box list */
enum {
    TRUST_LABEL,
    TRUST_VALIDITY,
    TRUST_N_COLUMNS
};

const GType trust_columns[] = {
    G_TYPE_STRING,  /* label */
    G_TYPE_INT      /* validity */
};

static void
on_add_subkey_completed (GObject *object, GAsyncResult *res, void *user_data)
{
    SeahorsePgpKeyProperties *self = SEAHORSE_PGP_KEY_PROPERTIES (user_data);
    g_autoptr(GError) error = NULL;

    if (!seahorse_gpgme_key_op_add_subkey_finish (SEAHORSE_GPGME_KEY (self->key),
                                                  res, &error)) {
        seahorse_util_handle_error (&error, self, NULL);
    }
}

static void
on_subkeys_add (GSimpleAction *action, GVariant *param, void *user_data)
{
    SeahorsePgpKeyProperties *self = SEAHORSE_PGP_KEY_PROPERTIES (user_data);
    SeahorseGpgmeAddSubkey *dialog;
    int response;
    SeahorseKeyEncType type;
    unsigned int length;
    GDateTime *expires;

    g_return_if_fail (SEAHORSE_GPGME_IS_KEY (self->key));

    dialog = seahorse_gpgme_add_subkey_new (SEAHORSE_GPGME_KEY (self->key),
                                            GTK_WINDOW (self));

    response = gtk_dialog_run (GTK_DIALOG (dialog));
    if (response != GTK_RESPONSE_OK) {
        gtk_widget_destroy (GTK_WIDGET (dialog));
        return;
    }

    length = seahorse_gpgme_add_subkey_get_keysize (dialog);
    type = seahorse_gpgme_add_subkey_get_active_type (dialog);
    expires = seahorse_gpgme_add_subkey_get_expires (dialog);
    seahorse_gpgme_key_op_add_subkey_async (SEAHORSE_GPGME_KEY (self->key),
                                            type, length, expires, NULL,
                                            on_add_subkey_completed, self);

    gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
on_pgp_details_trust_changed (GtkComboBox *selection, void *user_data)
{
    SeahorsePgpKeyProperties *self = SEAHORSE_PGP_KEY_PROPERTIES (user_data);
    int trust;
    GtkTreeModel *model;
    GtkTreeIter iter;

    if (!gtk_combo_box_get_active_iter (selection, &iter))
        return;

    g_return_if_fail (SEAHORSE_GPGME_IS_KEY (self->key));

    model = gtk_combo_box_get_model (selection);
    gtk_tree_model_get (model, &iter, TRUST_VALIDITY, &trust, -1);

    if (seahorse_pgp_key_get_trust (self->key) != trust) {
        gpgme_error_t err;

        err = seahorse_gpgme_key_op_set_trust (SEAHORSE_GPGME_KEY (self->key),
                                               trust);
        if (err)
            seahorse_gpgme_handle_error (err, _("Unable to change trust"));
    }
}

static void
on_export_complete (GObject *source, GAsyncResult *result, void *user_data)
{
    g_autoptr(GtkWindow) parent = GTK_WINDOW (user_data);
    GError *error = NULL;

    seahorse_exporter_export_to_file_finish (SEAHORSE_EXPORTER (source), result, &error);
    if (error != NULL)
        seahorse_util_handle_error (&error, parent, _("Couldn’t export key"));
}

static void
export_key_to_file (SeahorsePgpKeyProperties *self, gboolean secret)
{
    GList *exporters = NULL;
    GtkWindow *window;
    g_autofree char *directory = NULL;
    g_autoptr(GFile) file = NULL;
    SeahorseExporter *exporter = NULL;

    exporters = g_list_append (exporters,
                               seahorse_gpgme_exporter_new (G_OBJECT (self->key), TRUE, secret));

    window = GTK_WINDOW (self);
    if (seahorse_exportable_prompt (exporters, window, &directory, &file, &exporter)) {
        seahorse_exporter_export_to_file (exporter, file, TRUE, NULL,
                                          on_export_complete, g_object_ref (window));
    }

    g_clear_object (&exporter);
    g_list_free_full (exporters, g_object_unref);
}

static void
on_export_secret (GSimpleAction *action, GVariant *param, void *user_data)
{
    SeahorsePgpKeyProperties *self = SEAHORSE_PGP_KEY_PROPERTIES (user_data);

    export_key_to_file (self, TRUE);
}

static void
on_export_public (GSimpleAction *action, GVariant *param, void *user_data)
{
    SeahorsePgpKeyProperties *self = SEAHORSE_PGP_KEY_PROPERTIES (user_data);

    export_key_to_file (self, FALSE);
}

static void
on_change_expires (GSimpleAction *action, GVariant *param, void *user_data)
{
    SeahorsePgpKeyProperties *self = SEAHORSE_PGP_KEY_PROPERTIES (user_data);
    GListModel *subkeys;
    g_autoptr(SeahorseGpgmeSubkey) subkey = NULL;
    GtkDialog *dialog;

    subkeys = seahorse_pgp_key_get_subkeys (self->key);
    g_return_if_fail (g_list_model_get_n_items (subkeys) > 0);

    subkey = SEAHORSE_GPGME_SUBKEY (g_list_model_get_item (subkeys, 0));
    g_return_if_fail (subkey);

    dialog = seahorse_gpgme_expires_dialog_new (subkey, GTK_WINDOW (self));
    gtk_dialog_run (dialog);
    gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
setup_trust_combobox (SeahorsePgpKeyProperties *self)
{
    SeahorseUsage etype;
    GtkListStore *model;
    GtkCellRenderer *text_cell = gtk_cell_renderer_text_new ();

    etype = seahorse_object_get_usage (SEAHORSE_OBJECT (self->key));

    gtk_cell_layout_clear (GTK_CELL_LAYOUT (self->ownertrust_combobox));
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (self->ownertrust_combobox),
                                text_cell, FALSE);
    gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (self->ownertrust_combobox),
                                   text_cell,
                                   "text", TRUST_LABEL,
                                   NULL);

    /* Initialize the store */
    model = gtk_list_store_newv (TRUST_N_COLUMNS, (GType*)trust_columns);

    if (etype != SEAHORSE_USAGE_PRIVATE_KEY) {
        gtk_list_store_insert_with_values (model, NULL, -1,
            TRUST_LABEL, seahorse_validity_get_string (SEAHORSE_VALIDITY_UNKNOWN),
            TRUST_VALIDITY, SEAHORSE_VALIDITY_UNKNOWN,
            -1);

        gtk_list_store_insert_with_values (model, NULL, -1,
            TRUST_LABEL, seahorse_validity_get_string (SEAHORSE_VALIDITY_NEVER),
            TRUST_VALIDITY,  SEAHORSE_VALIDITY_NEVER,
            -1);
    }

    gtk_list_store_insert_with_values (model, NULL, -1,
                        TRUST_LABEL, seahorse_validity_get_string (SEAHORSE_VALIDITY_MARGINAL),
                        TRUST_VALIDITY,  SEAHORSE_VALIDITY_MARGINAL,
                        -1);

    gtk_list_store_insert_with_values (model, NULL, -1,
                        TRUST_LABEL, seahorse_validity_get_string (SEAHORSE_VALIDITY_FULL),
                        TRUST_VALIDITY,  SEAHORSE_VALIDITY_FULL,
                        -1);
    if (etype == SEAHORSE_USAGE_PRIVATE_KEY) {
        gtk_list_store_insert_with_values (model, NULL, -1,
                            TRUST_LABEL, seahorse_validity_get_string (SEAHORSE_VALIDITY_ULTIMATE),
                            TRUST_VALIDITY,  SEAHORSE_VALIDITY_ULTIMATE,
                            -1);
    }

    gtk_combo_box_set_model (GTK_COMBO_BOX (self->ownertrust_combobox),
                             GTK_TREE_MODEL (model));
}

static void
do_details (SeahorsePgpKeyProperties *self)
{
    int trust;
    gboolean valid;
    GtkTreeModel *model;
    GtkTreeIter iter;

    if (!seahorse_pgp_key_is_private_key (self->key)) {
        gtk_widget_set_visible (self->indicate_trust_row,
                                SEAHORSE_GPGME_IS_KEY (self->key));
    }
    gtk_widget_set_sensitive (GTK_WIDGET (self->ownertrust_combobox),
                              !(seahorse_object_get_flags (SEAHORSE_OBJECT (self->key)) & SEAHORSE_FLAG_DISABLED));

    model = gtk_combo_box_get_model (GTK_COMBO_BOX (self->ownertrust_combobox));
    valid = gtk_tree_model_get_iter_first (model, &iter);
    while (valid) {
        gtk_tree_model_get (model, &iter,
                            TRUST_VALIDITY, &trust,
                            -1);

        if (trust == seahorse_pgp_key_get_trust (self->key)) {
            g_signal_handlers_block_by_func (self->ownertrust_combobox, on_pgp_details_trust_changed, self);
            gtk_combo_box_set_active_iter (GTK_COMBO_BOX (self->ownertrust_combobox), &iter);
            g_signal_handlers_unblock_by_func (self->ownertrust_combobox, on_pgp_details_trust_changed, self);
            break;
        }

        valid = gtk_tree_model_iter_next (model, &iter);
    }
}

static void
on_trust_marginal_changed (GSimpleAction *action,
                           GVariant      *new_state,
                           void          *user_data)
{
    SeahorsePgpKeyProperties *self = SEAHORSE_PGP_KEY_PROPERTIES (user_data);
    SeahorseValidity trust;
    gpgme_error_t err;

    g_return_if_fail (SEAHORSE_GPGME_IS_KEY (self->key));

    trust = g_variant_get_boolean (new_state) ?
            SEAHORSE_VALIDITY_MARGINAL : SEAHORSE_VALIDITY_UNKNOWN;
    g_simple_action_set_state (action, new_state);

    if (seahorse_pgp_key_get_trust (self->key) != trust) {
        err = seahorse_gpgme_key_op_set_trust (SEAHORSE_GPGME_KEY (self->key), trust);
        if (err)
            seahorse_gpgme_handle_error (err, _("Unable to change trust"));
    }
}

/* Add a signature */
static void
on_sign_key (GSimpleAction *action, GVariant *param, void *user_data)
{
    SeahorsePgpKeyProperties *self = SEAHORSE_PGP_KEY_PROPERTIES (user_data);
    SeahorseGpgmeSignDialog *dialog;

    g_return_if_fail (SEAHORSE_GPGME_IS_KEY (self->key));

    dialog = seahorse_gpgme_sign_dialog_new (SEAHORSE_OBJECT (self->key));

    gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_destroy (GTK_WIDGET (dialog));
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
do_trust (SeahorsePgpKeyProperties *self)
{
    gboolean sigpersonal;
    GAction *trust_action;

    if (seahorse_object_get_usage (SEAHORSE_OBJECT (self->key)) != SEAHORSE_USAGE_PUBLIC_KEY)
        return;

    /* Remote keys */
    if (!SEAHORSE_GPGME_IS_KEY (self->key)) {
        gtk_widget_set_visible (self->trust_marginal_switch, TRUE);
        gtk_widget_set_sensitive (self->trust_marginal_switch, FALSE);
        gtk_widget_set_visible (self->trust_sign_row, FALSE);
        gtk_widget_set_visible (self->trust_revoke_row, FALSE);

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
        trust_action = g_action_map_lookup_action (G_ACTION_MAP (self->action_group),
                                                   "trust-marginal");

        /* Managed check boxes */
        if (managed) {
            GVariant *state;
            state = g_variant_new_boolean (trust != SEAHORSE_VALIDITY_UNKNOWN);
            g_simple_action_set_state (G_SIMPLE_ACTION (trust_action), state);
        }

        /* Signing and revoking */
        sigpersonal = key_have_signatures (self->key, SKEY_PGPSIG_PERSONAL);
        gtk_widget_set_visible (self->trust_sign_row, !sigpersonal);
        gtk_widget_set_visible (self->trust_revoke_row, sigpersonal);
    }
}

static const GActionEntry PRIVATE_KEY_ACTIONS[] = {
    { "change-password",  on_change_password  },
    { "change-expires",   on_change_expires   },
    { "export-secret",    on_export_secret    },
    { "export-public",    on_export_public    },
    { "uids.add",         on_uids_add         },
    { "subkeys.add",      on_subkeys_add      },
    { "photos.add",           on_photos_add           },
    { "photos.delete",        on_photos_delete        },
    { "photos.previous",      on_photos_previous      },
    { "photos.next",          on_photos_next          },
    { "photos.make-primary",  on_photos_make_primary  },
};

static const GActionEntry PUBLIC_KEY_ACTIONS[] = {
    { "sign-key",         on_sign_key  },
    /* { "revoke-key",    on_revoke_key  },  TODO */
    { "trust-marginal",   seahorse_util_toggle_action, NULL, "false", on_trust_marginal_changed },
    { "photos.previous",  on_photos_previous      },
    { "photos.next",      on_photos_next          },
};

static void
key_notify (GObject *object, GParamSpec *pspec, void *user_data)
{
    SeahorsePgpKeyProperties *self = SEAHORSE_PGP_KEY_PROPERTIES (user_data);

    do_owner (self);
    do_trust (self);
    do_details (self);
}

static void
get_common_widgets (SeahorsePgpKeyProperties *self, GtkBuilder *builder)
{
    GtkWidget *uids_listbox, *subkeys_listbox;

    self->name_label = GTK_WIDGET (gtk_builder_get_object (builder, "name_label"));
    self->email_label = GTK_WIDGET (gtk_builder_get_object (builder, "email_label"));
    self->comment_label = GTK_WIDGET (gtk_builder_get_object (builder, "comment_label"));
    self->keyid_label = GTK_WIDGET (gtk_builder_get_object (builder, "keyid_label"));
    self->fingerprint_label = GTK_WIDGET (gtk_builder_get_object (builder, "fingerprint_label"));
    self->expires_label = GTK_WIDGET (gtk_builder_get_object (builder, "expires_label"));
    self->photo_previous_button = GTK_WIDGET (gtk_builder_get_object (builder, "photo_previous_button"));
    self->photo_next_button = GTK_WIDGET (gtk_builder_get_object (builder, "photo_next_button"));
    self->revoked_area = GTK_WIDGET (gtk_builder_get_object (builder, "revoked_area"));
    self->expired_area = GTK_WIDGET (gtk_builder_get_object (builder, "expired_area"));
    self->expired_message = GTK_WIDGET (gtk_builder_get_object (builder, "expired_message"));
    self->photoid = GTK_IMAGE (gtk_builder_get_object (builder, "photoid"));
    self->photo_event_box = GTK_EVENT_BOX (gtk_builder_get_object (builder, "photo-event-box"));
    self->ownertrust_combobox = GTK_WIDGET (gtk_builder_get_object (builder, "ownertrust_combobox"));
    self->uids_container = GTK_WIDGET (gtk_builder_get_object (builder, "uids_container"));
    self->subkeys_container = GTK_WIDGET (gtk_builder_get_object (builder, "subkeys_container"));

    g_signal_connect_object (self->photo_event_box, "scroll-event",
                             G_CALLBACK (on_pgp_owner_photoid_button),
                             self, 0);
    g_signal_connect_object (self->ownertrust_combobox, "changed",
                             G_CALLBACK (on_pgp_details_trust_changed),
                             self, 0);

    uids_listbox = seahorse_pgp_uid_list_box_new (self->key);
    gtk_widget_show (uids_listbox);
    gtk_container_add (GTK_CONTAINER (self->uids_container),
                       uids_listbox);

    subkeys_listbox = seahorse_pgp_subkey_list_box_new (self->key);
    gtk_widget_show (subkeys_listbox);
    gtk_container_add (GTK_CONTAINER (self->subkeys_container),
                       subkeys_listbox);
}

static void
create_public_key_dialog (SeahorsePgpKeyProperties *self)
{
    g_autoptr(GtkBuilder) builder = NULL;
    GtkWidget *content_area, *content;
    const char *user;
    g_autofree char *sign_text = NULL;
    g_autofree char *revoke_text = NULL;

    builder = gtk_builder_new_from_resource (PUBLIC_KEY_PROPERTIES_UI);
    gtk_builder_connect_signals (builder, self);

    content_area = gtk_dialog_get_content_area (GTK_DIALOG (self));
    content = GTK_WIDGET (gtk_builder_get_object (builder, "window_content"));
    gtk_container_add (GTK_CONTAINER (content_area), content);

    g_action_map_add_action_entries (G_ACTION_MAP (self->action_group),
                                     PUBLIC_KEY_ACTIONS,
                                     G_N_ELEMENTS (PUBLIC_KEY_ACTIONS),
                                     self);
    gtk_widget_insert_action_group (GTK_WIDGET (self),
                                    "props",
                                    G_ACTION_GROUP (self->action_group));

    get_common_widgets (self, builder);

    self->trust_page = GTK_WIDGET (gtk_builder_get_object (builder, "trust-page"));
    self->indicate_trust_row = GTK_WIDGET (gtk_builder_get_object (builder, "indicate_trust_row"));
    self->trust_sign_row = GTK_WIDGET (gtk_builder_get_object (builder, "trust_sign_row"));
    self->trust_revoke_row = GTK_WIDGET (gtk_builder_get_object (builder, "trust_revoke_row"));
    self->trust_marginal_switch = GTK_WIDGET (gtk_builder_get_object (builder, "trust-marginal-switch"));

    setup_trust_combobox (self);
    do_owner (self);
    do_details (self);
    do_trust (self);

    /* Fill in trust labels with name. */
    user = seahorse_object_get_label (SEAHORSE_OBJECT (self->key));

    sign_text = g_strdup_printf(_("I believe “%s” is the owner of this key"),
                               user);
    hdy_action_row_set_subtitle (HDY_ACTION_ROW (self->trust_sign_row), sign_text);
    hdy_action_row_set_subtitle_lines (HDY_ACTION_ROW (self->trust_sign_row), 0);

    revoke_text = g_strdup_printf(_("I no longer trust that “%s” owns this key"),
                                  user);
    hdy_action_row_set_subtitle (HDY_ACTION_ROW (self->trust_revoke_row), revoke_text);
    hdy_action_row_set_subtitle_lines (HDY_ACTION_ROW (self->trust_revoke_row), 0);
}

static void
create_private_key_dialog (SeahorsePgpKeyProperties *self)
{
    g_autoptr(GtkBuilder) builder = NULL;
    GtkWidget *content_area, *content;

    builder = gtk_builder_new_from_resource (PRIVATE_KEY_PROPERTIES_UI);

    content_area = gtk_dialog_get_content_area (GTK_DIALOG (self));
    content = GTK_WIDGET (gtk_builder_get_object (builder, "window_content"));
    self->actions_menu = G_MENU_MODEL (gtk_builder_get_object (builder, "actions_menu"));
    gtk_container_add (GTK_CONTAINER (content_area), content);

    g_action_map_add_action_entries (G_ACTION_MAP (self->action_group),
                                     PRIVATE_KEY_ACTIONS,
                                     G_N_ELEMENTS (PRIVATE_KEY_ACTIONS),
                                     self);
    gtk_widget_insert_action_group (GTK_WIDGET (self),
                                    "props",
                                    G_ACTION_GROUP (self->action_group));

    get_common_widgets (self, builder);

    self->owner_photo_frame = GTK_WIDGET (gtk_builder_get_object (builder, "owner-photo-frame"));
    self->owner_photo_add_button = GTK_WIDGET (gtk_builder_get_object (builder, "owner-photo-add-button"));
    self->owner_photo_delete_button = GTK_WIDGET (gtk_builder_get_object (builder, "owner-photo-delete-button"));
    self->owner_photo_primary_button = GTK_WIDGET (gtk_builder_get_object (builder, "owner-photo-primary-button"));

    setup_trust_combobox (self);
    do_owner (self);
    do_details (self);

    /* Allow DnD on the photo frame */
    g_signal_connect_object (self->owner_photo_frame, "drag-data-received",
                             G_CALLBACK (on_pgp_owner_photo_drag_received),
                             self, 0);
    gtk_drag_dest_set (self->owner_photo_frame,
                       GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_HIGHLIGHT | GTK_DEST_DEFAULT_DROP,
                       target_list, G_N_ELEMENTS (target_list),
                       GDK_ACTION_COPY);
}

static void
seahorse_pgp_key_properties_get_property (GObject      *object,
                                          unsigned int  prop_id,
                                          GValue       *value,
                                          GParamSpec   *pspec)
{
    SeahorsePgpKeyProperties *self = SEAHORSE_PGP_KEY_PROPERTIES (object);

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
seahorse_pgp_key_properties_set_property (GObject      *object,
                                          unsigned int  prop_id,
                                          const GValue *value,
                                          GParamSpec   *pspec)
{
    SeahorsePgpKeyProperties *self = SEAHORSE_PGP_KEY_PROPERTIES (object);

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
seahorse_pgp_key_properties_finalize (GObject *obj)
{
    SeahorsePgpKeyProperties *self = SEAHORSE_PGP_KEY_PROPERTIES (obj);

    g_clear_object (&self->key);
    g_clear_object (&self->action_group);

    G_OBJECT_CLASS (seahorse_pgp_key_properties_parent_class)->finalize (obj);
}

static void
seahorse_pgp_key_properties_constructed (GObject *obj)
{
    SeahorsePgpKeyProperties *self = SEAHORSE_PGP_KEY_PROPERTIES (obj);
    SeahorseUsage usage;

    G_OBJECT_CLASS (seahorse_pgp_key_properties_parent_class)->constructed (obj);

    usage = seahorse_object_get_usage (SEAHORSE_OBJECT (self->key));
    if (usage == SEAHORSE_USAGE_PUBLIC_KEY)
        create_public_key_dialog (self);
    else
        create_private_key_dialog (self);

    g_signal_connect_object (self->key, "notify",
                             G_CALLBACK (key_notify), self, 0);
}

static void
seahorse_pgp_key_properties_init (SeahorsePgpKeyProperties *self)
{
    self->action_group = g_simple_action_group_new ();

    gtk_window_set_default_size (GTK_WINDOW (self), 300, 600);
    gtk_container_set_border_width (GTK_CONTAINER (self), 0);
}

static void
seahorse_pgp_key_properties_class_init (SeahorsePgpKeyPropertiesClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->constructed = seahorse_pgp_key_properties_constructed;
    gobject_class->get_property = seahorse_pgp_key_properties_get_property;
    gobject_class->set_property = seahorse_pgp_key_properties_set_property;
    gobject_class->finalize = seahorse_pgp_key_properties_finalize;

    g_object_class_install_property (gobject_class, PROP_KEY,
        g_param_spec_object ("key", "PGP key",
                             "The PGP key of which we're showing the details",
                             SEAHORSE_PGP_TYPE_KEY,
                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
}

GtkWindow *
seahorse_pgp_key_properties_new (SeahorsePgpKey *pkey, GtkWindow *parent)
{
    g_autoptr(SeahorsePgpKeyProperties) dialog = NULL;

    /* This causes the key source to get any specific info about the key */
    if (SEAHORSE_GPGME_IS_KEY (pkey)) {
        seahorse_gpgme_key_refresh (SEAHORSE_GPGME_KEY (pkey));
        seahorse_gpgme_key_ensure_signatures (SEAHORSE_GPGME_KEY (pkey));
    }

    dialog = g_object_new (SEAHORSE_PGP_TYPE_KEY_PROPERTIES,
                           "key", pkey,
                           "transient-for", parent,
                           NULL);

    return GTK_WINDOW (g_steal_pointer (&dialog));
}
