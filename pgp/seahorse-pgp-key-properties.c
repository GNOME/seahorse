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
#include "seahorse-gpgme-add-uid.h"
#include "seahorse-gpgme-dialogs.h"
#include "seahorse-gpgme-exporter.h"
#include "seahorse-gpgme-key.h"
#include "seahorse-gpgme-key-op.h"
#include "seahorse-pgp-backend.h"
#include "seahorse-gpg-op.h"
#include "seahorse-pgp-dialogs.h"
#include "seahorse-pgp-key.h"
#include "seahorse-pgp-uid.h"
#include "seahorse-pgp-signature.h"
#include "seahorse-pgp-subkey.h"

#include "seahorse-common.h"

#include "libseahorse/seahorse-object-model.h"
#include "libseahorse/seahorse-util.h"

#include <glib.h>
#include <glib/gi18n.h>

#include <string.h>
#include <time.h>

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
    GtkLabel *expired_message;

    GtkImage *photoid;
    GtkEventBox *photo_event_box;

    GtkLabel *owner_name_label;
    GtkLabel *owner_email_label;
    GtkLabel *owner_comment_label;
    GtkLabel *owner_keyid_label;
    GtkWidget *owner_photo_previous_button;
    GtkWidget *owner_photo_next_button;
    GtkLabel *details_id_label;
    GtkLabel *details_fingerprint_label;
    GtkLabel *details_algo_label;
    GtkLabel *details_created_label;
    GtkLabel *details_strength_label;
    GtkLabel *details_expires_label;
    GtkComboBox *details_trust_combobox;
    GtkTreeView *details_subkey_tree;

    /* Private key widgets */
    GtkTreeView *names_tree;
    GtkWidget *owner_photo_frame;
    GtkWidget *owner_photo_add_button;
    GtkWidget *owner_photo_delete_button;
    GtkWidget *owner_photo_primary_button;

    /* Public key widgets */
    GtkTreeView *owner_userid_tree;
    GtkTreeView *signatures_tree;
    GtkWidget *signatures_area;
    GtkWidget *uids_area;
    GtkWidget *trust_page;
    GtkLabel *trust_sign_label;
    GtkLabel *trust_revoke_label;
    GtkWidget *manual_trust_area;
    GtkWidget *sign_area;
    GtkWidget *revoke_area;
    GtkLabel *trust_marginal_label;
    GtkSwitch *trust_marginal_switch;
    GtkToggleButton *signatures_toggle;
};

G_DEFINE_TYPE (SeahorsePgpKeyProperties, seahorse_pgp_key_properties, GTK_TYPE_DIALOG)

static void
set_action_enabled (SeahorsePgpKeyProperties *self,
                    const gchar *action_str,
                    gboolean enabled)
{
    GAction *action;

    action = g_action_map_lookup_action (G_ACTION_MAP (self->action_group),
                                         action_str);
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action), enabled);
}

static gpointer
get_selected_object (GtkTreeView *widget, guint column)
{
    GtkTreeSelection *selection;
    GtkTreeIter iter;
    GtkTreeModel *model;
    GList *rows;
    gpointer object = NULL;

    model = gtk_tree_view_get_model (widget);

    selection = gtk_tree_view_get_selection (widget);
    g_assert (gtk_tree_selection_get_mode (selection) == GTK_SELECTION_SINGLE);

    rows = gtk_tree_selection_get_selected_rows (selection, NULL);

    if (g_list_length (rows) > 0) {
        gtk_tree_model_get_iter (model, &iter, rows->data);
        gtk_tree_model_get (model, &iter, column, &object, -1);
        if (object)
            g_object_unref (object);
    }

    g_list_foreach (rows, (GFunc)gtk_tree_path_free, NULL);
    g_list_free (rows);

    return object;
}

static void
on_pgp_signature_row_activated (GtkTreeView *treeview,
                                GtkTreePath *path,
                                GtkTreeViewColumn *arg2,
                                gpointer user_data)
{
    SeahorsePgpKeyProperties *self = SEAHORSE_PGP_KEY_PROPERTIES (user_data);
    GObject *object = NULL;
    GtkTreeModel *model;
    GtkTreeIter iter;

    model = gtk_tree_view_get_model (treeview);

    if (GTK_IS_TREE_MODEL_FILTER (model))
        model = gtk_tree_model_filter_get_model (GTK_TREE_MODEL_FILTER (model));

    g_return_if_fail (gtk_tree_model_get_iter (model, &iter, path));

    object = seahorse_object_model_get_row_key (SEAHORSE_OBJECT_MODEL (model), &iter);
    if (object != NULL && SEAHORSE_PGP_IS_KEY (object)) {
        GtkWindow *parent;
        g_autoptr(GtkWindow) dialog = NULL;

        parent = GTK_WINDOW (gtk_widget_get_parent (GTK_WIDGET (self)));
        dialog = seahorse_pgp_key_properties_new (SEAHORSE_PGP_KEY (object),
                                                  parent);
        gtk_dialog_run (GTK_DIALOG (dialog));
        gtk_widget_destroy (GTK_WIDGET (dialog));
    }
}

static void
unique_strings (GPtrArray *keyids)
{
    guint i;

    g_ptr_array_sort (keyids, (GCompareFunc)g_ascii_strcasecmp);
    for (i = 0; i + 1 < keyids->len; ) {
        if (g_ascii_strcasecmp (keyids->pdata[i], keyids->pdata[i + 1]) == 0)
            g_ptr_array_remove_index (keyids, i);
        else
            i++;
    }
}

/* -----------------------------------------------------------------------------
 * NAMES PAGE (PRIVATE KEYS)
 */

enum {
    UIDSIG_OBJECT,
    UIDSIG_ICON,
    UIDSIG_NAME,
    UIDSIG_KEYID,
    UIDSIG_N_COLUMNS
};

static GType uidsig_columns[] = {
    G_TYPE_OBJECT,  /* index */
    0 /* later */,  /* icon */
    G_TYPE_STRING,  /* name */
    G_TYPE_STRING   /* keyid */
};

static SeahorsePgpUid*
names_get_selected_uid (SeahorsePgpKeyProperties *self)
{
    return get_selected_object (self->names_tree, UIDSIG_OBJECT);
}

static void
on_uids_add (GSimpleAction *action, GVariant *param, gpointer user_data)
{
    SeahorsePgpKeyProperties *self = SEAHORSE_PGP_KEY_PROPERTIES (user_data);
    g_return_if_fail (SEAHORSE_GPGME_IS_KEY (self->key));
    seahorse_gpgme_add_uid_run_dialog (SEAHORSE_GPGME_KEY (self->key),
                                       GTK_WINDOW (self));
}

static void
on_uids_make_primary_cb (GObject *source, GAsyncResult *res, gpointer user_data)
{
    SeahorsePgpKeyProperties *self = SEAHORSE_PGP_KEY_PROPERTIES (user_data);
    SeahorseGpgmeUid *uid = SEAHORSE_GPGME_UID (source);
    g_autoptr(GError) error = NULL;

    if (!seahorse_gpgme_key_op_make_primary_finish (uid, res, &error)) {
        GtkWindow *window;
        window = gtk_window_get_transient_for (GTK_WINDOW (self));
        seahorse_util_show_error (GTK_WIDGET (window),
                                  _("Couldn’t change primary user ID"),
                                  error->message);
    }
}

static void
on_uids_make_primary (GSimpleAction *action, GVariant *param, gpointer user_data)
{
    SeahorsePgpKeyProperties *self = SEAHORSE_PGP_KEY_PROPERTIES (user_data);
    SeahorsePgpUid *uid;

    uid = names_get_selected_uid (self);
    if (!uid)
        return;

    g_return_if_fail (SEAHORSE_GPGME_IS_UID (uid));
    seahorse_gpgme_key_op_make_primary_async (SEAHORSE_GPGME_UID (uid),
                                              NULL,
                                              on_uids_make_primary_cb, self);
}

static void
on_uids_delete (GSimpleAction *action, GVariant *param, gpointer user_data)
{
    SeahorsePgpKeyProperties *self = SEAHORSE_PGP_KEY_PROPERTIES (user_data);
    SeahorsePgpUid *uid;
    gboolean ret;
    g_autofree gchar *message = NULL;
    gpgme_error_t gerr;

    uid = names_get_selected_uid (self);
    if (uid == NULL)
        return;

    g_return_if_fail (SEAHORSE_GPGME_IS_UID (uid));
    message = g_strdup_printf (_("Are you sure you want to permanently delete the “%s” user ID?"),
                               seahorse_object_get_label (SEAHORSE_OBJECT (uid)));
    ret = seahorse_delete_dialog_prompt (GTK_WINDOW (self), message);

    if (ret == FALSE)
        return;

    gerr = seahorse_gpgme_key_op_del_uid (SEAHORSE_GPGME_UID (uid));
    if (!GPG_IS_OK (gerr))
        seahorse_gpgme_handle_error (gerr, _("Couldn’t delete user ID"));
}

static void
on_uids_sign (GSimpleAction *action, GVariant *param, gpointer user_data)
{
    SeahorsePgpKeyProperties *self = SEAHORSE_PGP_KEY_PROPERTIES (user_data);
    SeahorsePgpUid *uid;

    uid = names_get_selected_uid (self);
    if (uid != NULL) {
        g_return_if_fail (SEAHORSE_GPGME_IS_UID (uid));
        seahorse_gpgme_sign_prompt_uid (SEAHORSE_GPGME_UID (uid),
                                        GTK_WINDOW (self));
    }
}

static void
on_uids_revoke (GSimpleAction *action, GVariant *param, gpointer user_data)
{
    /* TODO: */
/*    SeahorseObject *skey;
    gint index;
    Glist *keys = NULL;

    skey = SEAHORSE_OBJECT_WIDGET (swidget)->skey;
    index = names_get_selected_uid (swidget);

    if (index >= 1) {
        seahorse_revoke_show (SEAHORSE_PGP_KEY (skey), index - 1);

#ifdef WITH_KEYSERVER
        if (g_settings_get_boolean(AUTOSYNC_KEY) == TRUE) {
            keys = g_list_append (keys, skey);
            seahorse_keyserver_sync (keys);
            g_list_free(keys);
        }
#endif
    }*/
}

static void
update_names (GtkTreeSelection *selection, SeahorsePgpKeyProperties *self)
{
    SeahorsePgpUid *uid = names_get_selected_uid (self);
    gint index = -1;

    if (uid && SEAHORSE_GPGME_IS_UID (uid))
        index = seahorse_gpgme_uid_get_gpgme_index (SEAHORSE_GPGME_UID (uid));

    set_action_enabled (self, "uids.make-primary", index > 0);
    set_action_enabled (self, "uids.sign", index >= 0);
    set_action_enabled (self, "uids.delete", index >= 0);
}

/* Is called whenever a signature key changes, to update row */
static void
names_update_row (SeahorseObjectModel *skmodel, SeahorseObject *object,
                  GtkTreeIter *iter, SeahorsePgpKeyProperties *self)
{
    g_autoptr(GIcon) icon = NULL;
    const gchar *name, *id;

    icon = g_themed_icon_new (SEAHORSE_PGP_IS_KEY (object) ?
                              SEAHORSE_ICON_SIGN : "dialog-question");
    name = seahorse_object_get_markup (object);
    id = seahorse_object_get_identifier (object);

    gtk_tree_store_set (GTK_TREE_STORE (skmodel), iter,
                        UIDSIG_OBJECT, NULL,
                        UIDSIG_ICON, icon,
                        /* TRANSLATORS: [Unknown] signature name */
                        UIDSIG_NAME, name ? name : _("[Unknown]"),
                        UIDSIG_KEYID, id, -1);
}

static void
names_populate (SeahorsePgpKeyProperties *self, GtkTreeStore *store, SeahorsePgpKey *pkey)
{
    GObject *object;
    GtkTreeIter uiditer, sigiter;
    GList *keys, *l;
    GList *uids, *u;
    GList *sigs, *s;

    /* Insert all the fun-ness */
    uids = seahorse_pgp_key_get_uids (pkey);

    for (u = uids; u; u = g_list_next (u)) {
        SeahorsePgpUid *uid;
        g_autoptr(GIcon) icon = NULL;
        g_autoptr(GPtrArray) keyids = NULL;
        g_autoptr(GCancellable) cancellable = NULL;

        uid = SEAHORSE_PGP_UID (u->data);
        icon = g_themed_icon_new (SEAHORSE_ICON_PERSON);
        gtk_tree_store_append (store, &uiditer, NULL);
        gtk_tree_store_set (store, &uiditer,
                            UIDSIG_OBJECT, uid,
                            UIDSIG_ICON, icon,
                            UIDSIG_NAME, seahorse_object_get_markup (SEAHORSE_OBJECT (uid)),
                            -1);

        keyids = g_ptr_array_new ();

        /* Build a list of all the keyids */
        sigs = seahorse_pgp_uid_get_signatures (uid);
        for (s = sigs; s; s = g_list_next (s)) {
            /* Never show self signatures, they're implied */
            if (seahorse_pgp_key_has_keyid (pkey, seahorse_pgp_signature_get_keyid (s->data)))
                continue;
            g_ptr_array_add (keyids, (gpointer)seahorse_pgp_signature_get_keyid (s->data));
        }

        g_ptr_array_add (keyids, NULL);

        /*
         * Pass it to 'DiscoverKeys' for resolution/download, cancellable
         * ties search scope together
         */
        cancellable = g_cancellable_new ();
        keys = seahorse_pgp_backend_discover_keys (NULL, (const gchar **)keyids->pdata, cancellable);

        /* Add the keys to the store */
        for (l = keys; l; l = g_list_next (l)) {
            object = G_OBJECT (l->data);
            gtk_tree_store_append (store, &sigiter, &uiditer);

            /* This calls the 'update-row' callback, to set the values for the key */
            seahorse_object_model_set_row_object (SEAHORSE_OBJECT_MODEL (store), &sigiter, object);
        }
    }
}

static void
do_names (SeahorsePgpKeyProperties *self)
{
    GtkTreeStore *store;
    GtkCellRenderer *renderer;

    if (seahorse_object_get_usage (SEAHORSE_OBJECT (self->key)) != SEAHORSE_USAGE_PRIVATE_KEY)
        return;

    /* Clear/create table store */
    g_return_if_fail (self->names_tree != NULL);

    store = GTK_TREE_STORE (gtk_tree_view_get_model (self->names_tree));
    if (store) {
        gtk_tree_store_clear (store);

    } else {
        g_assert (UIDSIG_N_COLUMNS == G_N_ELEMENTS (uidsig_columns));
        uidsig_columns[UIDSIG_ICON] = G_TYPE_ICON;

        /* This is our first time so create a store */
        store = GTK_TREE_STORE (seahorse_object_model_new (UIDSIG_N_COLUMNS, uidsig_columns));
        g_signal_connect (store, "update-row", G_CALLBACK (names_update_row), self);

        /* Icon column */
        renderer = gtk_cell_renderer_pixbuf_new ();
        g_object_set (renderer, "stock-size", GTK_ICON_SIZE_LARGE_TOOLBAR, NULL);
        gtk_tree_view_insert_column_with_attributes (self->names_tree,
                                                     -1, "", renderer,
                                                     "gicon", UIDSIG_ICON, NULL);

        renderer = gtk_cell_renderer_text_new ();
        g_object_set (renderer, "yalign", 0.0, "xalign", 0.0, NULL);
        gtk_tree_view_insert_column_with_attributes (self->names_tree,
                                                     /* TRANSLATORS: The name and email set on the PGP key */
                                                     -1, _("Name/Email"), renderer,
                                                     "markup", UIDSIG_NAME, NULL);

        /* The signature ID column */
        renderer = gtk_cell_renderer_text_new ();
        g_object_set (renderer, "yalign", 0.0, "xalign", 0.0, NULL);
        gtk_tree_view_insert_column_with_attributes (self->names_tree,
                                                     -1, _("Signature ID"), renderer,
                                                     "text", UIDSIG_KEYID, NULL);
    }

    names_populate (self, store, self->key);

    gtk_tree_view_set_model (self->names_tree, GTK_TREE_MODEL(store));
    gtk_tree_view_expand_all (self->names_tree);

    update_names (NULL, self);
}

/* -----------------------------------------------------------------------------
 * PHOTO ID AREA
 */

/* drag-n-drop uri data */
enum {
    TARGET_URI,
};

static GtkTargetEntry target_list[] = {
    { "text/uri-list", 0, TARGET_URI } };

static void
on_pgp_owner_photo_drag_received (GtkWidget *widget,
                                  GdkDragContext *context,
                                  gint x,
                                  gint y,
                                  GtkSelectionData *sel_data,
                                  guint target_type,
                                  guint time,
                                  gpointer user_data)
{
    SeahorsePgpKeyProperties *self = SEAHORSE_PGP_KEY_PROPERTIES (user_data);
    gboolean dnd_success = FALSE;
    gint len = 0;

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
            g_autofree gchar *uri;

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
on_photos_add (GSimpleAction *action, GVariant *param, gpointer user_data)
{
    SeahorsePgpKeyProperties *self = SEAHORSE_PGP_KEY_PROPERTIES (user_data);

    g_return_if_fail (SEAHORSE_GPGME_IS_KEY (self->key));

    if (seahorse_gpgme_photo_add (SEAHORSE_GPGME_KEY (self->key),
                                  GTK_WINDOW (self), NULL))
        g_object_set_data (G_OBJECT (self), "current-photoid", NULL);
}

static void
on_photos_delete (GSimpleAction *action, GVariant *param, gpointer user_data)
{
    SeahorsePgpKeyProperties *self = SEAHORSE_PGP_KEY_PROPERTIES (user_data);
    SeahorseGpgmePhoto *photo;

    photo = g_object_get_data (G_OBJECT (self), "current-photoid");
    g_return_if_fail (SEAHORSE_IS_GPGME_PHOTO (photo));

    if (seahorse_gpgme_key_op_photo_delete (photo))
        g_object_set_data (G_OBJECT (self), "current-photoid", NULL);
}

static void
on_photos_make_primary (GSimpleAction *action, GVariant *param, gpointer user_data)
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
    GList *photos;

    etype = seahorse_object_get_usage (SEAHORSE_OBJECT (self->key));
    photos = seahorse_pgp_key_get_photos (self->key);

    photo = g_object_get_data (G_OBJECT (self), "current-photoid");
    g_return_if_fail (!photo || SEAHORSE_PGP_IS_PHOTO (photo));
    is_gpgme = SEAHORSE_GPGME_IS_KEY (self->key);

    if (etype == SEAHORSE_USAGE_PRIVATE_KEY) {
        gtk_widget_set_visible (self->owner_photo_add_button, is_gpgme);
        gtk_widget_set_visible (self->owner_photo_primary_button,
                                is_gpgme && photos && photos->next);
        gtk_widget_set_visible (self->owner_photo_delete_button,
                                is_gpgme && photo);
    }

    /* Display both of these when there is more than one photo id */
    gtk_widget_set_visible (self->owner_photo_previous_button,
                            photos && photos->next);
    gtk_widget_set_visible (self->owner_photo_next_button,
                            photos && photos->next);

    /* Change sensitivity if first/last photo id */
    set_action_enabled (self, "photos.previous",
                        photo && photos && photo != g_list_first (photos)->data);
    set_action_enabled (self, "photos.next",
                        photo && photos && photo != g_list_last (photos)->data);

    pixbuf = photo ? seahorse_pgp_photo_get_pixbuf (photo) : NULL;
    if (pixbuf)
        gtk_image_set_from_pixbuf (self->photoid, pixbuf);
}

static void
do_photo_id (SeahorsePgpKeyProperties *self)
{
    GList *photos;

    photos = seahorse_pgp_key_get_photos (self->key);
    if (!photos)
        g_object_set_data (G_OBJECT (self), "current-photoid", NULL);
    else
        g_object_set_data_full (G_OBJECT (self), "current-photoid",
                                g_object_ref (photos->data), g_object_unref);

    set_photoid_state (self);
}

static void
on_photos_next (GSimpleAction *action, GVariant *param, gpointer user_data)
{
    SeahorsePgpKeyProperties *self = SEAHORSE_PGP_KEY_PROPERTIES (user_data);
    SeahorsePgpPhoto *photo;
    GList *photos;

    photos = seahorse_pgp_key_get_photos (self->key);
    photo = g_object_get_data (G_OBJECT (self), "current-photoid");
    if (photo) {
        g_return_if_fail (SEAHORSE_PGP_IS_PHOTO (photo));
        photos = g_list_find (photos, photo);
        if (photos && photos->next)
            g_object_set_data_full (G_OBJECT (self), "current-photoid",
                                    g_object_ref (photos->next->data),
                                    g_object_unref);
    }

    set_photoid_state (self);
}

static void
on_photos_previous (GSimpleAction *action, GVariant *param, gpointer user_data)
{
    SeahorsePgpKeyProperties *self = SEAHORSE_PGP_KEY_PROPERTIES (user_data);
    SeahorsePgpPhoto *photo;
    GList *photos;

    photos = seahorse_pgp_key_get_photos (self->key);
    photo = g_object_get_data (G_OBJECT (self), "current-photoid");
    if (photo) {
        g_return_if_fail (SEAHORSE_PGP_IS_PHOTO (photo));
        photos = g_list_find (photos, photo);
        if (photos && photos->prev)
            g_object_set_data_full (G_OBJECT (self), "current-photoid",
                                    g_object_ref (photos->prev->data),
                                    g_object_unref);
    }

    set_photoid_state (self);
}

static void
on_pgp_owner_photoid_button (GtkWidget *widget,
                             GdkEvent *event,
                             gpointer user_data)
{
    SeahorsePgpKeyProperties *self = SEAHORSE_PGP_KEY_PROPERTIES (user_data);
    const gchar *action_str;

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

/* -----------------------------------------------------------------------------
 * OWNER PAGE
 */

/* owner uid list */
enum {
    UID_OBJECT,
    UID_ICON,
    UID_MARKUP,
    UID_N_COLUMNS
};

static GType uid_columns[] = {
    G_TYPE_OBJECT,  /* object */
    0 /* later */,  /* icon */
    G_TYPE_STRING,  /* name */
    G_TYPE_STRING,  /* email */
    G_TYPE_STRING   /* comment */
};

static void
on_gpgme_key_change_pass_done (GObject *source,
                               GAsyncResult *res,
                               gpointer user_data)
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
on_change_password (GSimpleAction *action, GVariant *param, gpointer user_data)
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
do_owner (SeahorsePgpKeyProperties *self)
{
    GtkCellRenderer *renderer;
    GtkListStore *store;
    GtkTreeIter iter;
    guint flags;
    const gchar *label;
    GList *uids, *l;

    flags = seahorse_object_get_flags (SEAHORSE_OBJECT (self->key));

    /* Display appropriate warnings */
    gtk_widget_set_visible (self->expired_area, flags & SEAHORSE_FLAG_EXPIRED);
    gtk_widget_set_visible (self->revoked_area, flags & SEAHORSE_FLAG_REVOKED);

    /* Update the expired message */
    if (flags & SEAHORSE_FLAG_EXPIRED) {
        gulong expires_date;
        g_autofree gchar *message = NULL;
        g_autofree gchar *date_str = NULL;

        expires_date = seahorse_pgp_key_get_expires (self->key);
        if (expires_date == 0) {
            /* TRANSLATORS: (unknown) expiry date */
            date_str = g_strdup (_("(unknown)"));
        } else {
            date_str = seahorse_util_get_display_date_string (expires_date);
        }

        message = g_strdup_printf (_("This key expired on: %s"), date_str);
        gtk_label_set_text (self->expired_message, message);
    }

    /* Hide trust page when above */
    if (self->trust_page != NULL) {
        gtk_widget_set_visible (self->trust_page, !((flags & SEAHORSE_FLAG_EXPIRED) ||
                                                    (flags & SEAHORSE_FLAG_REVOKED) ||
                                                    (flags & SEAHORSE_FLAG_DISABLED)));
    }

    /* Hide or show the uids area */
    uids = seahorse_pgp_key_get_uids (self->key);
    if (self->uids_area != NULL)
        gtk_widget_set_visible (self->uids_area, uids != NULL);
    if (uids != NULL) {
        g_autofree gchar *title = NULL;
        g_autofree gchar *email_escaped = NULL;
        g_autofree gchar *email_label = NULL;
        SeahorsePgpUid *uid;

        uid = SEAHORSE_PGP_UID (uids->data);

        label = seahorse_pgp_uid_get_name (uid);
        gtk_label_set_text (self->owner_name_label, label);

        if (seahorse_object_get_usage (SEAHORSE_OBJECT (self->key)) != SEAHORSE_USAGE_PRIVATE_KEY) {
            /* Translators: the 1st part of the title is the owner's name */
            title = g_strdup_printf (_("%s — Public key"), label);
        } else {
            /* Translators: the 1st part of the title is the owner's name */
            title = g_strdup_printf (_("%s — Private key"), label);
        }
        gtk_window_set_title (GTK_WINDOW (self), title);

        label = seahorse_pgp_uid_get_email (uid);
        if (label && *label) {
            email_escaped = g_markup_escape_text (label, -1);
            email_label = g_strdup_printf ("<a href=\"mailto:%s\">%s</a>", label, email_escaped);
            gtk_label_set_markup (self->owner_email_label, email_label);
        }

        label = seahorse_pgp_uid_get_comment (uid);
        gtk_label_set_text (self->owner_comment_label, label);

        label = seahorse_object_get_identifier (SEAHORSE_OBJECT (self->key));
        gtk_label_set_text (self->owner_keyid_label, label);
    }

    /* Clear/create table store */
    if (self->owner_userid_tree) {
        store = GTK_LIST_STORE (gtk_tree_view_get_model (self->owner_userid_tree));

        if (store) {
            gtk_list_store_clear (GTK_LIST_STORE (store));

        } else {
            g_assert (UID_N_COLUMNS != G_N_ELEMENTS (uid_columns));
            uid_columns[1] = G_TYPE_ICON;

            /* This is our first time so create a store */
            store = gtk_list_store_newv (UID_N_COLUMNS, (GType*)uid_columns);

            /* Make the columns for the view */
            renderer = gtk_cell_renderer_pixbuf_new ();
            g_object_set (renderer, "stock-size", GTK_ICON_SIZE_LARGE_TOOLBAR, NULL);
            gtk_tree_view_insert_column_with_attributes (self->owner_userid_tree,
                                                         -1, "", renderer,
                                                         "gicon", UID_ICON, NULL);

            gtk_tree_view_insert_column_with_attributes (self->owner_userid_tree,
                                                         -1, _("Name"), gtk_cell_renderer_text_new (),
                                                         "markup", UID_MARKUP, NULL);
        }

        for (l = uids; l; l = g_list_next (l)) {
            const gchar *markup;
            g_autoptr(GIcon) icon = NULL;

            markup = seahorse_object_get_markup (l->data);
            icon = g_themed_icon_new (SEAHORSE_ICON_PERSON);
            gtk_list_store_append (store, &iter);
            gtk_list_store_set (store, &iter,
                                UID_OBJECT, l->data,
                                UID_ICON, icon,
                                UID_MARKUP, markup, -1);
        }

        gtk_tree_view_set_model (self->owner_userid_tree, GTK_TREE_MODEL (store));
    }

    do_photo_id (self);
}

/* -----------------------------------------------------------------------------
 * DETAILS PAGE
 */

/* details subkey list */
enum {
    SUBKEY_OBJECT,
    SUBKEY_ID,
    SUBKEY_TYPE,
    SUBKEY_USAGE,
    SUBKEY_CREATED,
    SUBKEY_EXPIRES,
    SUBKEY_STATUS,
    SUBKEY_LENGTH,
    SUBKEY_N_COLUMNS
};

const GType subkey_columns[] = {
    G_TYPE_OBJECT,  /* SUBKEY_OBJECT */
    G_TYPE_STRING,  /* SUBKEY_ID */
    G_TYPE_STRING,  /* SUBKEY_TYPE */
    G_TYPE_STRING,  /* SUBKEY_USAGE */
    G_TYPE_STRING,  /* SUBKEY_CREATED */
    G_TYPE_STRING,  /* SUBKEY_EXPIRES  */
    G_TYPE_STRING,  /* SUBKEY_STATUS */
    G_TYPE_UINT     /* SUBKEY_LENGTH */
};

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

static SeahorsePgpSubkey*
get_selected_subkey (SeahorsePgpKeyProperties *self)
{
    return get_selected_object (self->details_subkey_tree, SUBKEY_OBJECT);
}

static void
details_subkey_selected (GtkTreeSelection *selection, SeahorsePgpKeyProperties *self)
{
    SeahorsePgpSubkey* subkey;
    guint flags = 0;

    subkey = get_selected_object (self->details_subkey_tree, SUBKEY_OBJECT);
    if (subkey)
        flags = seahorse_pgp_subkey_get_flags (subkey);

    set_action_enabled (self, "subkeys.change-expires", subkey != NULL);
    set_action_enabled (self, "subkeys.revoke",
                        subkey != NULL && !(flags & SEAHORSE_FLAG_REVOKED));
    set_action_enabled (self, "subkeys.delete", subkey != NULL);
}

static void
on_subkeys_add (GSimpleAction *action, GVariant *param, gpointer user_data)
{
    SeahorsePgpKeyProperties *self = SEAHORSE_PGP_KEY_PROPERTIES (user_data);

    g_return_if_fail (SEAHORSE_GPGME_IS_KEY (self->key));
    seahorse_gpgme_add_subkey_new (SEAHORSE_GPGME_KEY (self->key),
                                   GTK_WINDOW (self));
}

static void
on_subkeys_delete (GSimpleAction *action, GVariant *param, gpointer user_data)
{
    SeahorsePgpKeyProperties *self = SEAHORSE_PGP_KEY_PROPERTIES (user_data);
    SeahorsePgpSubkey *subkey;
    guint index;
    gboolean ret;
    const gchar *label;
    g_autofree gchar *message = NULL;
    gpgme_error_t err;

    subkey = get_selected_subkey (self);
    if (!subkey)
        return;

    g_return_if_fail (SEAHORSE_GPGME_IS_SUBKEY (subkey));

    index = seahorse_pgp_subkey_get_index (subkey);
    label = seahorse_object_get_label (SEAHORSE_OBJECT (self->key));
    message = g_strdup_printf (_("Are you sure you want to permanently delete subkey %d of %s?"), index, label);
    ret = seahorse_delete_dialog_prompt (GTK_WINDOW (self), message);

    if (ret == FALSE)
        return;

    err = seahorse_gpgme_key_op_del_subkey (SEAHORSE_GPGME_SUBKEY (subkey));
    if (!GPG_IS_OK (err))
        seahorse_gpgme_handle_error (err, _("Couldn’t delete subkey"));
}

static void
on_subkeys_revoke (GSimpleAction *action, GVariant *param, gpointer user_data)
{
    SeahorsePgpKeyProperties *self = SEAHORSE_PGP_KEY_PROPERTIES (user_data);
    SeahorsePgpSubkey *subkey = get_selected_subkey (self);
    if (subkey != NULL)
        return;

    g_return_if_fail (SEAHORSE_GPGME_IS_SUBKEY (subkey));
    seahorse_gpgme_revoke_new (SEAHORSE_GPGME_SUBKEY (subkey),
                               GTK_WINDOW (self));
}

static void
on_subkeys_change_expires (GSimpleAction *action, GVariant *param, gpointer user_data)
{
    SeahorsePgpKeyProperties *self = SEAHORSE_PGP_KEY_PROPERTIES (user_data);
    SeahorsePgpSubkey *subkey;
    GList *subkeys;

    subkey = get_selected_subkey (self);
    if (subkey == NULL) {
        subkeys = seahorse_pgp_key_get_subkeys (self->key);
        if (subkeys)
            subkey = subkeys->data;
    }

    g_return_if_fail (SEAHORSE_GPGME_IS_SUBKEY (subkey));

    if (subkey != NULL)
        seahorse_gpgme_expires_new (SEAHORSE_GPGME_SUBKEY (subkey),
                                    GTK_WINDOW (self));
}

static void
on_pgp_details_trust_changed (GtkComboBox *selection, gpointer user_data)
{
    SeahorsePgpKeyProperties *self = SEAHORSE_PGP_KEY_PROPERTIES (user_data);
    gint trust;
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
on_export_complete (GObject *source, GAsyncResult *result, gpointer user_data)
{
    g_autoptr(GtkWindow) parent = GTK_WINDOW (user_data);
    GError *error = NULL;

    seahorse_exporter_export_to_file_finish (SEAHORSE_EXPORTER (source), result, &error);
    if (error != NULL)
        seahorse_util_handle_error (&error, parent, _("Couldn’t export key"));
}

static void
on_export_secret (GSimpleAction *action, GVariant *param, gpointer user_data)
{
    SeahorsePgpKeyProperties *self = SEAHORSE_PGP_KEY_PROPERTIES (user_data);
    GList *exporters = NULL;
    GtkWindow *window;
    g_autofree gchar *directory = NULL;
    g_autoptr(GFile) file = NULL;
    SeahorseExporter *exporter = NULL;

    exporters = g_list_append (exporters,
                               seahorse_gpgme_exporter_new (G_OBJECT (self->key), TRUE, TRUE));

    window = GTK_WINDOW (self);
    if (seahorse_exportable_prompt (exporters, window, &directory, &file, &exporter)) {
        seahorse_exporter_export_to_file (exporter, file, TRUE, NULL,
                                          on_export_complete, g_object_ref (window));
    }

    g_clear_object (&exporter);
    g_list_free_full (exporters, g_object_unref);
}

static void
on_change_expires (GSimpleAction *action, GVariant *param, gpointer user_data)
{
    SeahorsePgpKeyProperties *self = SEAHORSE_PGP_KEY_PROPERTIES (user_data);
    GList *subkeys;

    subkeys = seahorse_pgp_key_get_subkeys (self->key);
    g_return_if_fail (subkeys);

    seahorse_gpgme_expires_new (SEAHORSE_GPGME_SUBKEY (subkeys->data),
                                GTK_WINDOW (self));
}

static void
setup_trust_combobox (SeahorsePgpKeyProperties *self)
{
    SeahorseUsage etype;
    GtkListStore *model;
    GtkCellRenderer *text_cell = gtk_cell_renderer_text_new ();

    etype = seahorse_object_get_usage (SEAHORSE_OBJECT (self->key));

    gtk_cell_layout_clear (GTK_CELL_LAYOUT (self->details_trust_combobox));
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (self->details_trust_combobox),
                                text_cell, FALSE);
    gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (self->details_trust_combobox),
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

    gtk_combo_box_set_model (self->details_trust_combobox, GTK_TREE_MODEL (model));
}

static void
do_details (SeahorsePgpKeyProperties *self)
{
    SeahorsePgpSubkey *subkey;
    GtkListStore *store;
    GtkTreeModel *model;
    GtkTreeIter iter;
    gchar dbuffer[G_ASCII_DTOSTR_BUF_SIZE];
    g_autofree gchar *fp_label = NULL;
    g_autofree gchar *created_str = NULL;
    g_autofree gchar *expires_str = NULL;
    const gchar *label;
    gint trust;
    GList *subkeys, *l;
    gboolean valid;

    subkeys = seahorse_pgp_key_get_subkeys (self->key);
    g_return_if_fail (subkeys && subkeys->data);
    subkey = SEAHORSE_PGP_SUBKEY (subkeys->data);

    label = seahorse_object_get_identifier (SEAHORSE_OBJECT (self->key));
    gtk_label_set_text (self->details_id_label, label);

    fp_label = g_strdup (seahorse_pgp_key_get_fingerprint (self->key));
    if (strlen (fp_label) > 24 && g_ascii_isspace (fp_label[24]))
        fp_label[24] = '\n';
    gtk_label_set_text (self->details_fingerprint_label, fp_label);

    label = seahorse_pgp_key_get_algo (self->key);
    gtk_label_set_text (self->details_algo_label, label);

    created_str = seahorse_util_get_display_date_string (seahorse_pgp_subkey_get_created (subkey));
    gtk_label_set_text (self->details_created_label, created_str);

    g_ascii_dtostr (dbuffer, G_ASCII_DTOSTR_BUF_SIZE, seahorse_pgp_subkey_get_length (subkey));
    gtk_label_set_text (self->details_strength_label, dbuffer);

    gulong expires = seahorse_pgp_subkey_get_expires (subkey);
    if (!SEAHORSE_GPGME_IS_KEY (self->key))
        expires_str = NULL;
    else if (expires == 0)
        expires_str = g_strdup (C_("Expires", "Never"));
    else
        expires_str = seahorse_util_get_display_date_string (expires);
    gtk_label_set_text (self->details_expires_label, expires_str);

    gtk_widget_set_visible (GTK_WIDGET (self->details_trust_combobox),
                            SEAHORSE_GPGME_IS_KEY (self->key));
    gtk_widget_set_sensitive (GTK_WIDGET (self->details_trust_combobox),
                              !(seahorse_object_get_flags (SEAHORSE_OBJECT (self->key)) & SEAHORSE_FLAG_DISABLED));

    model = gtk_combo_box_get_model (self->details_trust_combobox);
    valid = gtk_tree_model_get_iter_first (model, &iter);
    while (valid) {
        gtk_tree_model_get (model, &iter,
                            TRUST_VALIDITY, &trust,
                            -1);

        if (trust == seahorse_pgp_key_get_trust (self->key)) {
            g_signal_handlers_block_by_func (self->details_trust_combobox, on_pgp_details_trust_changed, self);
            gtk_combo_box_set_active_iter (self->details_trust_combobox,  &iter);
            g_signal_handlers_unblock_by_func (self->details_trust_combobox, on_pgp_details_trust_changed, self);
            break;
        }

        valid = gtk_tree_model_iter_next (model, &iter);
    }

    /* Clear/create table store */
    store = GTK_LIST_STORE (gtk_tree_view_get_model (self->details_subkey_tree));
    if (store) {
        gtk_list_store_clear (store);
    } else {

        /* This is our first time so create a store */
        store = gtk_list_store_newv (SUBKEY_N_COLUMNS, (GType*)subkey_columns);
        gtk_tree_view_set_model (self->details_subkey_tree, GTK_TREE_MODEL (store));

        /* Make the columns for the view */
        gtk_tree_view_insert_column_with_attributes (self->details_subkey_tree,
                                                     -1, _("ID"), gtk_cell_renderer_text_new (),
                                                     "text", SUBKEY_ID, NULL);
        gtk_tree_view_insert_column_with_attributes (self->details_subkey_tree,
                                                     -1, _("Type"), gtk_cell_renderer_text_new (),
                                                     "text", SUBKEY_TYPE, NULL);
        gtk_tree_view_insert_column_with_attributes (self->details_subkey_tree,
                                                     -1, _("Usage"), gtk_cell_renderer_text_new (),
                                                     "text", SUBKEY_USAGE, NULL);
        gtk_tree_view_insert_column_with_attributes (self->details_subkey_tree,
                                                     -1, _("Created"), gtk_cell_renderer_text_new (),
                                                     "text", SUBKEY_CREATED, NULL);
        gtk_tree_view_insert_column_with_attributes (self->details_subkey_tree,
                                                     -1, _("Expires"), gtk_cell_renderer_text_new (),
                                                     "text", SUBKEY_EXPIRES, NULL);
        gtk_tree_view_insert_column_with_attributes (self->details_subkey_tree,
                                                     -1, _("Status"), gtk_cell_renderer_text_new (),
                                                     "text", SUBKEY_STATUS, NULL);
        gtk_tree_view_insert_column_with_attributes (self->details_subkey_tree,
                                                     -1, _("Strength"), gtk_cell_renderer_text_new (),
                                                     "text", SUBKEY_LENGTH, NULL);
    }

    for (l = subkeys; l; l = g_list_next (l)) {
        const gchar *status = NULL;
        g_autofree gchar *expiration_date = NULL;
        g_autofree gchar *created_date = NULL;
        g_autofree gchar *usage = NULL;
        gulong expires;
        guint flags;

        subkey = SEAHORSE_PGP_SUBKEY (l->data);
        expires = seahorse_pgp_subkey_get_expires (subkey);
        flags = seahorse_pgp_subkey_get_flags (subkey);
        status = "";

        if (flags & SEAHORSE_FLAG_REVOKED)
            status = _("Revoked");
        else if (flags & SEAHORSE_FLAG_EXPIRED)
            status = _("Expired");
        else if (flags & SEAHORSE_FLAG_DISABLED)
            status = _("Disabled");
        else if (flags & SEAHORSE_FLAG_IS_VALID)
            status = _("Good");

        if (expires == 0)
            expiration_date = g_strdup (C_("Expires", "Never"));
        else
            expiration_date = seahorse_util_get_display_date_string (expires);

        created_date = seahorse_util_get_display_date_string (seahorse_pgp_subkey_get_created (subkey));

        usage = seahorse_pgp_subkey_get_usage (subkey);

        gtk_list_store_append (store, &iter);
        gtk_list_store_set (store, &iter,
                            SUBKEY_OBJECT, subkey,
                            SUBKEY_ID, seahorse_pgp_subkey_get_keyid (subkey),
                            SUBKEY_TYPE, seahorse_pgp_subkey_get_algorithm (subkey),
                            SUBKEY_USAGE, usage,
                            SUBKEY_CREATED, created_date,
                            SUBKEY_EXPIRES, expiration_date,
                            SUBKEY_STATUS, status,
                            SUBKEY_LENGTH, seahorse_pgp_subkey_get_length (subkey),
                            -1);
    }
}

/* -----------------------------------------------------------------------------
 * TRUST PAGE (PUBLIC KEYS)
 */

enum {
    SIGN_ICON,
    SIGN_NAME,
    SIGN_KEYID,
    SIGN_TRUSTED,
    SIGN_N_COLUMNS
};

static GType sign_columns[] = {
    0 /* later */,
    G_TYPE_STRING,
    G_TYPE_STRING,
    G_TYPE_BOOLEAN
};

static void
on_toggle_action (GSimpleAction *action, GVariant *param, gpointer user_data) {
    GVariant *old_state, *new_state;

    old_state = g_action_get_state (G_ACTION (action));
    new_state = g_variant_new_boolean (!g_variant_get_boolean (old_state));
    g_action_change_state (G_ACTION (action), new_state);
}

static void
on_trust_marginal_changed (GSimpleAction *action, GVariant *new_state, gpointer user_data)
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

/* Is called whenever a signature key changes */
static void
trust_update_row (SeahorseObjectModel *skmodel, SeahorseObject *object,
                  GtkTreeIter *iter, gpointer user_data)
{
    gboolean trusted = FALSE;
    g_autoptr(GIcon) icon = NULL;
    const gchar *name, *id;

    if (seahorse_object_get_usage (object) == SEAHORSE_USAGE_PRIVATE_KEY)
        trusted = TRUE;
    else if (seahorse_object_get_flags (object) & SEAHORSE_FLAG_TRUSTED)
        trusted = TRUE;

    icon = g_themed_icon_new (SEAHORSE_PGP_IS_KEY (object) ?
                              SEAHORSE_ICON_SIGN : "dialog-question");
    name = seahorse_object_get_label (object);
    id = seahorse_object_get_identifier (object);

    gtk_tree_store_set (GTK_TREE_STORE (skmodel), iter,
                        SIGN_ICON, icon,
                        /* TRANSLATORS: [Unknown] signature name */
                        SIGN_NAME, name ? name : _("[Unknown]"),
                        SIGN_KEYID, id,
                        SIGN_TRUSTED, trusted,
                        -1);
}

static void
signatures_populate_model (SeahorsePgpKeyProperties *self, SeahorseObjectModel *skmodel)
{
    GtkTreeIter iter;
    gboolean have_sigs = FALSE;
    g_autoptr(GPtrArray) rawids = NULL;
    GList *keys, *l, *uids;
    GList *sigs, *s;

    if (self->signatures_tree == NULL)
        return;

    rawids = g_ptr_array_new ();
    uids = seahorse_pgp_key_get_uids (self->key);

    /* Build a list of all the keyids */
    for (l = uids; l; l = g_list_next (l)) {
        sigs = seahorse_pgp_uid_get_signatures (l->data);
        for (s = sigs; s; s = g_list_next (s)) {
            /* Never show self signatures, they're implied */
            if (seahorse_pgp_key_has_keyid (self->key,
                                            seahorse_pgp_signature_get_keyid (s->data)))
                continue;
            have_sigs = TRUE;
            g_ptr_array_add (rawids, (gchar *)seahorse_pgp_signature_get_keyid (s->data));
        }
    }

    /* Strip out duplicates */
    unique_strings (rawids);
    g_ptr_array_add (rawids, NULL);

    /* Only show signatures area when there are signatures */
    gtk_widget_set_visible (self->signatures_area, have_sigs);

    if (skmodel) {
        g_autoptr(GCancellable) cancellable = NULL;

        /* Pass it to 'DiscoverKeys' for resolution/download. cancellable ties
         * search scope together */
        cancellable = g_cancellable_new ();
        keys = seahorse_pgp_backend_discover_keys (NULL, (const gchar **)rawids->pdata, cancellable);

        /* Add the keys to the store */
        for (l = keys; l; l = g_list_next (l)) {
            GObject *object = G_OBJECT (l->data);

            gtk_tree_store_append (GTK_TREE_STORE (skmodel), &iter, NULL);
            /* This calls the 'update-row' callback, to set the values for the key */
            seahorse_object_model_set_row_object (SEAHORSE_OBJECT_MODEL (skmodel), &iter, object);
        }
    }
}

/* Refilter when the user toggles the 'only show trusted' checkbox */
static void
on_pgp_trusted_toggled (GtkToggleButton *toggle, GtkTreeModelFilter *filter)
{
    /* Set flag on the store */
    GtkTreeModel *model = gtk_tree_model_filter_get_model (filter);
    g_object_set_data (G_OBJECT (model), "only-trusted",
                GINT_TO_POINTER (gtk_toggle_button_get_active (toggle)));
    gtk_tree_model_filter_refilter (filter);
}

/* Add a signature */
static void
on_sign_key (GSimpleAction *action, GVariant *param, gpointer user_data)
{
    SeahorsePgpKeyProperties *self = SEAHORSE_PGP_KEY_PROPERTIES (user_data);

    g_return_if_fail (SEAHORSE_GPGME_IS_KEY (self->key));
    seahorse_gpgme_sign_prompt (SEAHORSE_GPGME_KEY (self->key), GTK_WINDOW (self));
}

/* When the 'only display trusted' check is checked, hide untrusted rows */
static gboolean
trust_filter (GtkTreeModel *model, GtkTreeIter *iter, gpointer userdata)
{
    /* Read flag on the store */
    gboolean trusted = FALSE;
    gtk_tree_model_get (model, iter, SIGN_TRUSTED, &trusted, -1);
    return !g_object_get_data (G_OBJECT (model), "only-trusted") || trusted;
}

static gboolean
key_have_signatures (SeahorsePgpKey *pkey, guint types)
{
    GList *uids, *u;
    GList *sigs, *s;

    uids = seahorse_pgp_key_get_uids (pkey);
    for (u = uids; u; u = g_list_next (u)) {
        sigs = seahorse_pgp_uid_get_signatures (u->data);
        for (s = sigs; s; s = g_list_next (s)) {
            if (seahorse_pgp_signature_get_sigtype (s->data) & types)
                return TRUE;
        }
    }

    return FALSE;
}

static void
do_trust (SeahorsePgpKeyProperties *self)
{
    GtkTreeStore *store;
    GtkTreeModelFilter *filter;
    gboolean sigpersonal;
    GtkCellRenderer *renderer;
    GAction *trust_action;

    if (seahorse_object_get_usage (SEAHORSE_OBJECT (self->key)) != SEAHORSE_USAGE_PUBLIC_KEY)
        return;

    /* Remote keys */
    if (!SEAHORSE_GPGME_IS_KEY (self->key)) {
        gtk_widget_set_visible (self->manual_trust_area, FALSE);
        gtk_widget_set_visible (GTK_WIDGET (self->trust_marginal_switch), TRUE);
        gtk_widget_set_sensitive (GTK_WIDGET (self->trust_marginal_switch), FALSE);
        gtk_widget_set_visible (self->sign_area, FALSE);
        gtk_widget_set_visible (self->revoke_area, FALSE);

    /* Local keys */
    } else {
        guint trust;
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
        gtk_widget_set_visible (self->manual_trust_area, !managed);
        gtk_widget_set_visible (GTK_WIDGET (self->trust_marginal_switch), managed);
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
        gtk_widget_set_visible (self->sign_area, !sigpersonal);
        gtk_widget_set_visible (self->revoke_area, sigpersonal);
    }

    /* The actual signatures listing */
    if (self->signatures_tree != NULL) {
        filter = GTK_TREE_MODEL_FILTER (gtk_tree_view_get_model (self->signatures_tree));

        if (filter) {
            /* First time create the store */
            store = GTK_TREE_STORE (gtk_tree_model_filter_get_model (filter));
            gtk_tree_store_clear (store);
        } else {
            /* Create a new SeahorseObjectModel store.... */
            sign_columns[SIGN_ICON] = G_TYPE_ICON;
            store = GTK_TREE_STORE (seahorse_object_model_new (SIGN_N_COLUMNS, (GType*)sign_columns));
            g_signal_connect (store, "update-row",
                              G_CALLBACK (trust_update_row), self);

            /* .... and a filter to go ontop of it */
            filter = GTK_TREE_MODEL_FILTER (gtk_tree_model_filter_new (GTK_TREE_MODEL (store), NULL));
            gtk_tree_model_filter_set_visible_func (filter,
                                                    (GtkTreeModelFilterVisibleFunc)trust_filter, NULL, NULL);

            /* Make the colunms for the view */
            renderer = gtk_cell_renderer_pixbuf_new ();
            g_object_set (renderer, "stock-size", GTK_ICON_SIZE_LARGE_TOOLBAR, NULL);
            gtk_tree_view_insert_column_with_attributes (self->signatures_tree,
                                                         -1, "", renderer,
                                                         "gicon", SIGN_ICON, NULL);
            gtk_tree_view_insert_column_with_attributes (self->signatures_tree,
                                                         /* TRANSLATORS: The name and email set on the PGP key */
                                                         -1, _("Name/Email"), gtk_cell_renderer_text_new (),
                                                         "text", SIGN_NAME, NULL);
            gtk_tree_view_insert_column_with_attributes (self->signatures_tree,
                                                         -1, _("Key ID"), gtk_cell_renderer_text_new (),
                                                         "text", SIGN_KEYID, NULL);

            gtk_tree_view_set_model (self->signatures_tree, GTK_TREE_MODEL (filter));

            g_signal_connect (self->signatures_toggle, "toggled",
                              G_CALLBACK (on_pgp_trusted_toggled), filter);
            gtk_toggle_button_set_active (self->signatures_toggle, TRUE);
        }

        signatures_populate_model (self, SEAHORSE_OBJECT_MODEL (store));
    }
}

/* -----------------------------------------------------------------------------
 * GENERAL
 */

static const GActionEntry PRIVATE_KEY_ACTIONS[] = {
    { "change-password",  on_change_password  },
    { "change-expires",   on_change_expires   },
    { "export-secret",    on_export_secret    },
    { "uids.add",           on_uids_add           },
    { "uids.delete",        on_uids_delete        },
    { "uids.make-primary",  on_uids_make_primary  },
    { "uids.revoke",        on_uids_revoke        },
    { "uids.sign",          on_uids_sign          },
    { "photos.add",           on_photos_add           },
    { "photos.delete",        on_photos_delete        },
    { "photos.previous",      on_photos_previous      },
    { "photos.next",          on_photos_next          },
    { "photos.make-primary",  on_photos_make_primary  },
    { "subkeys.add",             on_subkeys_add             },
    { "subkeys.delete",          on_subkeys_delete          },
    { "subkeys.revoke",          on_subkeys_revoke          },
    { "subkeys.change-expires",  on_subkeys_change_expires  },
};

static const GActionEntry PUBLIC_KEY_ACTIONS[] = {
    { "sign-key",        on_sign_key  },
    { "trust-marginal",  on_toggle_action,  NULL,  "false",  on_trust_marginal_changed },
    { "photos.previous",      on_photos_previous      },
    { "photos.next",          on_photos_next          },
};

static void
key_notify (GObject *object, GParamSpec *pspec, gpointer user_data)
{
    SeahorsePgpKeyProperties *self = SEAHORSE_PGP_KEY_PROPERTIES (user_data);

    do_owner (self);
    do_names (self);
    do_trust (self);
    do_details (self);
}

static void
get_common_widgets (SeahorsePgpKeyProperties *self, GtkBuilder *builder)
{
    self->owner_name_label = GTK_LABEL (gtk_builder_get_object (builder, "owner-name-label"));
    self->owner_email_label = GTK_LABEL (gtk_builder_get_object (builder, "owner-email-label"));
    self->owner_comment_label = GTK_LABEL (gtk_builder_get_object (builder, "owner-comment-label"));
    self->owner_keyid_label = GTK_LABEL (gtk_builder_get_object (builder, "owner-keyid-label"));
    self->owner_photo_previous_button = GTK_WIDGET (gtk_builder_get_object (builder, "owner-photo-previous-button"));
    self->owner_photo_next_button = GTK_WIDGET (gtk_builder_get_object (builder, "owner-photo-next-button"));
    self->revoked_area = GTK_WIDGET (gtk_builder_get_object (builder, "revoked-area"));
    self->expired_area = GTK_WIDGET (gtk_builder_get_object (builder, "expired-area"));
    self->expired_message = GTK_LABEL (gtk_builder_get_object (builder, "expired-message"));
    self->photoid = GTK_IMAGE (gtk_builder_get_object (builder, "photoid"));
    self->photo_event_box = GTK_EVENT_BOX (gtk_builder_get_object (builder, "photo-event-box"));
    self->details_id_label = GTK_LABEL (gtk_builder_get_object (builder, "details-id-label"));
    self->details_fingerprint_label = GTK_LABEL (gtk_builder_get_object (builder, "details-fingerprint-label"));
    self->details_algo_label = GTK_LABEL (gtk_builder_get_object (builder, "details-algo-label"));
    self->details_created_label = GTK_LABEL (gtk_builder_get_object (builder, "details-created-label"));
    self->details_strength_label = GTK_LABEL (gtk_builder_get_object (builder, "details-strength-label"));
    self->details_expires_label = GTK_LABEL (gtk_builder_get_object (builder, "details-expires-label"));
    self->details_trust_combobox = GTK_COMBO_BOX (gtk_builder_get_object (builder, "details-trust-combobox"));
    self->details_subkey_tree = GTK_TREE_VIEW (gtk_builder_get_object (builder, "details-subkey-tree"));

    g_signal_connect_object (self->photo_event_box, "scroll-event",
                             G_CALLBACK (on_pgp_owner_photoid_button),
                             self, 0);
    g_signal_connect_object (self->details_trust_combobox, "changed",
                             G_CALLBACK (on_pgp_details_trust_changed),
                             self, 0);
}

static void
create_public_key_dialog (SeahorsePgpKeyProperties *self)
{
    g_autoptr(GtkBuilder) builder = NULL;
    GtkWidget *content_area, *content;
    const gchar *user;
    g_autofree gchar *user_escaped = NULL;

    builder = gtk_builder_new_from_resource (PUBLIC_KEY_PROPERTIES_UI);
    gtk_builder_connect_signals (builder, self);

    content_area = gtk_dialog_get_content_area (GTK_DIALOG (self));
    content = GTK_WIDGET (gtk_builder_get_object (builder, "window-content"));
    gtk_container_add (GTK_CONTAINER (content_area), content);

    g_action_map_add_action_entries (G_ACTION_MAP (self->action_group),
                                     PUBLIC_KEY_ACTIONS,
                                     G_N_ELEMENTS (PUBLIC_KEY_ACTIONS),
                                     self);
    gtk_widget_insert_action_group (GTK_WIDGET (self),
                                    "props",
                                    G_ACTION_GROUP (self->action_group));

    get_common_widgets (self, builder);

    self->signatures_tree = GTK_TREE_VIEW (gtk_builder_get_object (builder, "signatures-tree"));
    self->signatures_area = GTK_WIDGET (gtk_builder_get_object (builder, "signatures-area"));
    self->uids_area = GTK_WIDGET (gtk_builder_get_object (builder, "uids-area"));
    self->trust_page = GTK_WIDGET (gtk_builder_get_object (builder, "trust-page"));
    self->trust_sign_label = GTK_LABEL (gtk_builder_get_object (builder, "trust-sign-label"));
    self->trust_revoke_label = GTK_LABEL (gtk_builder_get_object (builder, "trust-revoke-label"));
    self->manual_trust_area = GTK_WIDGET (gtk_builder_get_object (builder, "manual-trust-area"));
    self->sign_area = GTK_WIDGET (gtk_builder_get_object (builder, "sign-area"));
    self->revoke_area = GTK_WIDGET (gtk_builder_get_object (builder, "revoke-area"));
    self->trust_marginal_switch = GTK_SWITCH (gtk_builder_get_object (builder, "trust-marginal-switch"));
    self->trust_marginal_label = GTK_LABEL (gtk_builder_get_object (builder, "trust-marginal-label"));
    self->signatures_toggle = GTK_TOGGLE_BUTTON (gtk_builder_get_object (builder, "signatures-toggle"));
    self->owner_userid_tree = GTK_TREE_VIEW (gtk_builder_get_object (builder, "owner-userid-tree"));

    setup_trust_combobox (self);
    do_owner (self);
    do_details (self);
    do_trust (self);

    g_signal_connect_object (self->signatures_tree, "row-activated",
                             G_CALLBACK (on_pgp_signature_row_activated),
                             self, 0);

    /* Fill in trust labels with name. */
    user = seahorse_object_get_label (SEAHORSE_OBJECT (self->key));
    user_escaped = g_markup_escape_text (user, -1);

    {
        g_autofree gchar *text = NULL;
        text = g_strdup_printf(_("I trust signatures from “%s” on other keys"),
                               user);
        gtk_label_set_label (self->trust_marginal_label, text);
    }

    {
        g_autofree gchar *text = NULL;
        text = g_strdup_printf(_("If you believe that the person that owns this key is “%s”, <i>sign</i> this key:"),
                               user_escaped);
        gtk_label_set_markup (self->trust_sign_label, text);
    }

    {
        g_autofree gchar *text = NULL;
        text = g_strdup_printf(_("If you no longer trust that “%s” owns this key, <i>revoke</i> your signature:"),
                               user_escaped);
        gtk_label_set_markup (self->trust_revoke_label, text);
    }
}

static void
create_private_key_dialog (SeahorsePgpKeyProperties *self)
{
    g_autoptr(GtkBuilder) builder = NULL;
    GtkWidget *content_area, *content;
    GtkTreeSelection *selection;

    builder = gtk_builder_new_from_resource (PRIVATE_KEY_PROPERTIES_UI);

    content_area = gtk_dialog_get_content_area (GTK_DIALOG (self));
    content = GTK_WIDGET (gtk_builder_get_object (builder, "window-content"));
    gtk_container_add (GTK_CONTAINER (content_area), content);

    g_action_map_add_action_entries (G_ACTION_MAP (self->action_group),
                                     PRIVATE_KEY_ACTIONS,
                                     G_N_ELEMENTS (PRIVATE_KEY_ACTIONS),
                                     self);
    gtk_widget_insert_action_group (GTK_WIDGET (self),
                                    "props",
                                    G_ACTION_GROUP (self->action_group));

    get_common_widgets (self, builder);

    self->names_tree = GTK_TREE_VIEW (gtk_builder_get_object (builder, "names-tree"));
    self->owner_photo_frame = GTK_WIDGET (gtk_builder_get_object (builder, "owner-photo-frame"));
    self->owner_photo_add_button = GTK_WIDGET (gtk_builder_get_object (builder, "owner-photo-add-button"));
    self->owner_photo_delete_button = GTK_WIDGET (gtk_builder_get_object (builder, "owner-photo-delete-button"));
    self->owner_photo_primary_button = GTK_WIDGET (gtk_builder_get_object (builder, "owner-photo-primary-button"));

    setup_trust_combobox (self);
    do_owner (self);
    do_names (self);
    do_details (self);

    /* Allow DnD on the photo frame */
    g_signal_connect_object (self->owner_photo_frame, "drag-data-received",
                             G_CALLBACK (on_pgp_owner_photo_drag_received),
                             self, 0);
    gtk_drag_dest_set (self->owner_photo_frame,
                       GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_HIGHLIGHT | GTK_DEST_DEFAULT_DROP,
                       target_list, G_N_ELEMENTS (target_list),
                       GDK_ACTION_COPY);

    /* Enable and disable buttons as subkeys are selected */
    g_signal_connect (gtk_tree_view_get_selection (self->details_subkey_tree),
        "changed", G_CALLBACK (details_subkey_selected), self);
    details_subkey_selected (NULL, self);

    /* Enable and disable buttons as UIDs are selected */
    selection = gtk_tree_view_get_selection (self->names_tree);
    g_signal_connect (selection, "changed", G_CALLBACK (update_names), self);
}

static void
seahorse_pgp_key_properties_get_property (GObject *object,
                                          guint prop_id,
                                          GValue *value,
                                          GParamSpec *pspec)
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
seahorse_pgp_key_properties_set_property (GObject *object,
                                          guint prop_id,
                                          const GValue *value,
                                          GParamSpec *pspec)
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
