/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
 * Copyright (C) 2005 Jim Pharis
 * Copyright (C) 2005-2006 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
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
 * along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/* TODO: Make sure to free when getting text from seahorse_pgp_key_* */

#include "config.h"

#include <string.h>
  
#include <glib/gi18n.h>

#include "seahorse-bind.h"
#include "seahorse-gtkstock.h"
#include "seahorse-object.h"
#include "seahorse-object-model.h"
#include "seahorse-object-widget.h"
#include "seahorse-util.h"

#include "seahorse-gpgme-dialogs.h"
#include "seahorse-gpgme-key.h"
#include "seahorse-gpgme-key-op.h"
#include "seahorse-gpg-op.h"
#include "seahorse-pgp-dialogs.h"
#include "seahorse-pgp-key.h"
#include "seahorse-pgp-uid.h"
#include "seahorse-pgp-signature.h"
#include "seahorse-pgp-subkey.h"

#define DEBUG_FLAG SEAHORSE_DEBUG_KEYS
#include "seahorse-debug.h"

#include <time.h>

#define NOTEBOOK "notebook"

/* Forward declarations */
static void     properties_response                           (GtkDialog *dialog,
                                                               int response,
                                                               SeahorseWidget *swidget);

void            on_pgp_trust_sign                             (GtkWidget *widget,
                                                               gpointer user_data);

void            seahorse_pgp_details_signatures_delete_button (GtkWidget *widget,
                                                               SeahorseWidget *swidget);

void            on_pgp_details_signatures_revoke_button       (GtkWidget *widget,
                                                               SeahorseWidget *swidget);

void            on_pgp_trust_marginal_toggled                 (GtkToggleButton *toggle,
                                                               gpointer user_data);

void            on_pgp_details_export_button                  (GtkWidget *widget,
                                                               gpointer user_data);

void            on_pgp_details_expires_button                 (GtkWidget *widget,
                                                               gpointer user_data);

void            on_pgp_details_expires_subkey                 (GtkWidget *widget,
                                                               gpointer user_data);

void            on_pgp_details_add_subkey_button              (GtkButton *button,
                                                               gpointer user_data);

void            on_pgp_details_del_subkey_button              (GtkButton *button,
                                                               gpointer user_data);

void            on_pgp_details_revoke_subkey_button           (GtkButton *button,
                                                               gpointer user_data);

void            on_pgp_details_trust_changed                  (GtkComboBox *selection,
                                                               gpointer user_data);

void            on_pgp_owner_passphrase_button_clicked        (GtkWidget *widget,
                                                               gpointer user_data);

void            on_pgp_owner_photoid_next                     (GtkWidget *widget,
                                                               gpointer user_data);

void            on_pgp_owner_photoid_prev                     (GtkWidget *widget,
                                                               gpointer user_data);

void            on_pgp_owner_photoid_button                   (GtkWidget *widget,
                                                               GdkEvent *event,
                                                               gpointer user_data);

void            on_pgp_owner_photo_drag_received              (GtkWidget *widget,
                                                               GdkDragContext *context,
                                                               gint x,
                                                               gint y,
                                                               GtkSelectionData *sel_data,
                                                               guint target_type,
                                                               guint time,
                                                               gpointer user_data);

void            on_pgp_owner_photo_add_button                 (GtkWidget *widget,
                                                               gpointer user_data);

void            on_pgp_owner_photo_delete_button              (GtkWidget *widget,
                                                               gpointer user_data);

void            on_pgp_owner_photo_primary_button             (GtkWidget *widget,
                                                               gpointer user_data);

void            on_pgp_names_add_clicked                      (GtkWidget *widget,
                                                               gpointer user_data);

void            on_pgp_names_primary_clicked                  (GtkWidget *widget,
                                                               gpointer user_data);

void            on_pgp_names_delete_clicked                   (GtkWidget *widget,
                                                               gpointer user_data);

void            on_pgp_names_sign_clicked                     (GtkWidget *widget,
                                                               gpointer user_data);

void            on_pgp_names_revoke_clicked                   (GtkWidget *widget,
                                                               gpointer user_data);

void            on_pgp_signature_row_activated                (GtkTreeView *treeview,
                                                               GtkTreePath *path,
                                                               GtkTreeViewColumn *arg2,
                                                               gpointer user_data);

static void 
show_gtkbuilder_widget (SeahorseWidget *swidget, const gchar *name, gboolean show)
{
    GtkWidget *widget = GTK_WIDGET (gtk_builder_get_object (swidget->gtkbuilder, name));
    if (widget != NULL)
        seahorse_widget_set_visible (swidget, name, show);
}

static void
set_gtkbuilder_image (SeahorseWidget *swidget, const gchar *name, const gchar *stock)
{
    GtkWidget *widget = GTK_WIDGET (gtk_builder_get_object (swidget->gtkbuilder, name));
    
    if (!widget)
        return;
    
    gtk_image_set_from_stock (GTK_IMAGE (widget), stock, GTK_ICON_SIZE_DIALOG);
}

static void
sensitive_gtkbuilder_widget (SeahorseWidget *swidget, const gchar *name, gboolean sens)
{
    GtkWidget *widget = GTK_WIDGET (gtk_builder_get_object (swidget->gtkbuilder, name));
    if (widget)
        gtk_widget_set_sensitive (widget, sens);
}

static void
printf_gtkbuilder_widget (SeahorseWidget *swidget, const gchar *name, const gchar *str)
{
    GtkWidget *widget;
    const gchar *label;
    gchar *text;
    
    widget = GTK_WIDGET (gtk_builder_get_object (swidget->gtkbuilder, name));
    if (!widget)
        return; 

    if (!GTK_IS_LABEL (widget))    
        label = gtk_button_get_label (GTK_BUTTON (widget));
    else
        label = gtk_label_get_text (GTK_LABEL (widget));
        
    text = g_strdup_printf (label, str);
    
    if (!GTK_IS_LABEL (widget))
        gtk_button_set_label (GTK_BUTTON (widget), text);
    else
        gtk_label_set_text (GTK_LABEL (widget), text);
        
    g_free (text);
}

static gpointer
get_selected_object (SeahorseWidget *swidget, const gchar *objectid, guint column)
{
	GtkTreeSelection *selection;
	GtkWidget *widget;
	GtkTreeIter iter;
	GtkTreeModel *model;
	GList *rows;
	gpointer object = NULL;

	widget = GTK_WIDGET (seahorse_widget_get_widget (swidget, objectid));
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (widget));

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));
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

G_MODULE_EXPORT void
on_pgp_signature_row_activated (GtkTreeView *treeview,
                                GtkTreePath *path,
                                GtkTreeViewColumn *arg2,
                                gpointer user_data)
{
	SeahorseWidget *swidget = SEAHORSE_WIDGET (user_data);
	SeahorseObject *object = NULL;
	GtkTreeModel *model;
	GtkTreeIter iter;
    
	model = gtk_tree_view_get_model (treeview);
    
	if (GTK_IS_TREE_MODEL_FILTER (model)) 
		model = gtk_tree_model_filter_get_model (GTK_TREE_MODEL_FILTER (model));
        
	g_return_if_fail (gtk_tree_model_get_iter (model, &iter, path));
    
	object = seahorse_object_model_get_row_key (SEAHORSE_OBJECT_MODEL (model), &iter);
	if (object != NULL && SEAHORSE_IS_PGP_KEY (object)) {
		seahorse_pgp_key_properties_show (SEAHORSE_PGP_KEY (object), 
		                                  GTK_WINDOW (gtk_widget_get_parent (seahorse_widget_get_toplevel (swidget))));
	}
}

static GList*
unique_slist_strings (GList *keyids)
{
    GList *l;
    
    keyids = g_list_sort (keyids, (GCompareFunc)g_ascii_strcasecmp);
    for (l = keyids; l; l = g_list_next (l)) {
        while (l->next && l->data && l->next->data && 
               g_ascii_strcasecmp (l->data, l->next->data) == 0)
            keyids = g_list_delete_link (keyids, l->next);
    }    
    
    return keyids;
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

const GType uidsig_columns[] = {
    G_TYPE_OBJECT,  /* index */
    G_TYPE_STRING,  /* icon */
    G_TYPE_STRING,  /* name */
    G_TYPE_STRING   /* keyid */
};

static SeahorsePgpUid*
names_get_selected_uid (SeahorseWidget *swidget)
{
	return get_selected_object (swidget, "names-tree", UIDSIG_OBJECT);
}

G_MODULE_EXPORT void
on_pgp_names_add_clicked (GtkWidget *widget,
                          gpointer user_data)
{
	SeahorseWidget *swidget = SEAHORSE_WIDGET (user_data);
	SeahorseObject *obj = SEAHORSE_OBJECT_WIDGET (swidget)->object;
	g_return_if_fail (SEAHORSE_IS_GPGME_KEY (obj));
	seahorse_gpgme_add_uid_new (SEAHORSE_GPGME_KEY (obj), 
	                            GTK_WINDOW (seahorse_widget_get_widget (swidget, swidget->name)));
}

G_MODULE_EXPORT void
on_pgp_names_primary_clicked (GtkWidget *widget,
                              gpointer user_data)
{
	SeahorseWidget *swidget = SEAHORSE_WIDGET (user_data);
	SeahorsePgpUid *uid;
	gpgme_error_t err;
    
	uid = names_get_selected_uid (swidget);
	if (uid) {
		g_return_if_fail (SEAHORSE_IS_GPGME_UID (uid));
		err = seahorse_gpgme_key_op_primary_uid (SEAHORSE_GPGME_UID (uid));
		if (!GPG_IS_OK (err)) 
			seahorse_gpgme_handle_error (err, _("Couldn't change primary user ID"));
	}
}

G_MODULE_EXPORT void
on_pgp_names_delete_clicked (GtkWidget *widget,
                             gpointer user_data)
{
	SeahorseWidget *swidget = SEAHORSE_WIDGET (user_data);
	SeahorsePgpUid *uid;
	gboolean ret;
	gchar *message; 
	gpgme_error_t gerr;
    
	uid = names_get_selected_uid (swidget);
	if (uid == NULL)
		return;
	
	g_return_if_fail (SEAHORSE_IS_GPGME_UID (uid));
	message = g_strdup_printf (_("Are you sure you want to permanently delete the '%s' user ID?"), 
	                           seahorse_object_get_label (SEAHORSE_OBJECT (uid)));
	ret = seahorse_util_prompt_delete (message, seahorse_widget_get_toplevel (swidget));
	g_free (message);
	
	if (ret == FALSE)
		return;
	
	gerr = seahorse_gpgme_key_op_del_uid (SEAHORSE_GPGME_UID (uid));
	if (!GPG_IS_OK (gerr))
		seahorse_gpgme_handle_error (gerr, _("Couldn't delete user ID"));
}

G_MODULE_EXPORT void
on_pgp_names_sign_clicked (GtkWidget *widget,
                           gpointer user_data)
{
	SeahorseWidget *swidget = SEAHORSE_WIDGET (user_data);
	SeahorsePgpUid *uid;

	uid = names_get_selected_uid (swidget);
	if (uid != NULL) {
		g_return_if_fail (SEAHORSE_IS_GPGME_UID (uid));
		seahorse_gpgme_sign_prompt_uid (SEAHORSE_GPGME_UID (uid), 
		                                GTK_WINDOW (seahorse_widget_get_widget (swidget, swidget->name)));
	}
}

G_MODULE_EXPORT void
on_pgp_names_revoke_clicked (GtkWidget *widget,
                             gpointer user_data)
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
update_names (GtkTreeSelection *selection, SeahorseWidget *swidget)
{
	SeahorsePgpUid *uid = names_get_selected_uid (swidget);
	gint index = -1;
	
	if (uid && SEAHORSE_IS_GPGME_UID (uid))
		index = seahorse_gpgme_uid_get_gpgme_index (SEAHORSE_GPGME_UID (uid));

	sensitive_gtkbuilder_widget (swidget, "names-primary-button", index > 0);
	sensitive_gtkbuilder_widget (swidget, "names-delete-button", index >= 0);
	sensitive_gtkbuilder_widget (swidget, "names-sign-button", index >= 0);
	show_gtkbuilder_widget (swidget, "names-revoke-button", FALSE);
}

/* Is called whenever a signature key changes, to update row */
static void
names_update_row (SeahorseObjectModel *skmodel, SeahorseObject *object, 
                  GtkTreeIter *iter, SeahorseWidget *swidget)
{
	SeahorseObject *preferred;
	const gchar *icon;
	const gchar *name, *id;
    
	/* Always use the most preferred key for this keyid */
	preferred = seahorse_object_get_preferred (object);
	if (preferred) {
		while (SEAHORSE_IS_OBJECT (preferred)) {
			object = SEAHORSE_OBJECT (preferred);
			preferred = seahorse_object_get_preferred (preferred);
		}
		seahorse_object_model_set_row_object (skmodel, iter, object);
	}

	icon = seahorse_object_get_location (object) < SEAHORSE_LOCATION_LOCAL ? 
	                 GTK_STOCK_DIALOG_QUESTION : SEAHORSE_STOCK_SIGN;
	name = seahorse_object_get_markup (object);
	id = seahorse_object_get_identifier (object);
	
	gtk_tree_store_set (GTK_TREE_STORE (skmodel), iter,
	                    UIDSIG_OBJECT, NULL,
	                    UIDSIG_ICON, icon,
	                    UIDSIG_NAME, name ? name : _("[Unknown]"),
	                    UIDSIG_KEYID, id, -1);
}

static void
names_populate (SeahorseWidget *swidget, GtkTreeStore *store, SeahorsePgpKey *pkey)
{
	SeahorseObject *object;
	GtkTreeIter uiditer, sigiter;
	GList *rawids = NULL;
	SeahorsePgpUid *uid;
	GList *keys, *l;
	GList *uids, *u;
	GList *sigs, *s;
	GCancellable *cancellable;

	/* Insert all the fun-ness */
	uids = seahorse_pgp_key_get_uids (pkey);
	
	for (u = uids; u; u = g_list_next (u)) {

		uid = SEAHORSE_PGP_UID (u->data);

		gtk_tree_store_append (store, &uiditer, NULL);
		gtk_tree_store_set (store, &uiditer,  
		                    UIDSIG_OBJECT, uid,
		                    UIDSIG_ICON, SEAHORSE_STOCK_PERSON,
		                    UIDSIG_NAME, seahorse_object_get_markup (SEAHORSE_OBJECT (uid)),
		                    -1);
        
        
		/* Build a list of all the keyids */
		sigs = seahorse_pgp_uid_get_signatures (uid);
		for (s = sigs; s; s = g_list_next (s)) {
			/* Never show self signatures, they're implied */
			if (seahorse_pgp_key_has_keyid (pkey, seahorse_pgp_signature_get_keyid (s->data)))
				continue;
			rawids = g_list_prepend (rawids, (gpointer)seahorse_pgp_signature_get_keyid (s->data));
		}

		/*
		 * Pass it to 'DiscoverKeys' for resolution/download, cancellable
		 * ties search scope together
		 */
		cancellable = g_cancellable_new ();
		keys = seahorse_context_discover_objects (seahorse_context_instance (),
		                                          SEAHORSE_PGP, rawids, cancellable);
		g_object_unref (cancellable);
		g_list_free (rawids);
		rawids = NULL;
        
		/* Add the keys to the store */
		for (l = keys; l; l = g_list_next (l)) {
			object = SEAHORSE_OBJECT (l->data);
			gtk_tree_store_append (store, &sigiter, &uiditer);
            
			/* This calls the 'update-row' callback, to set the values for the key */
			seahorse_object_model_set_row_object (SEAHORSE_OBJECT_MODEL (store), &sigiter, object);
		}
	} 
}

static void
do_names (SeahorseWidget *swidget)
{
    SeahorseObject *object;
    SeahorsePgpKey *pkey;
    GtkWidget *widget;
    GtkTreeStore *store;
    GtkCellRenderer *renderer;
    
    object = SEAHORSE_OBJECT_WIDGET (swidget)->object;
    pkey = SEAHORSE_PGP_KEY (object);
    
    if (seahorse_object_get_usage (object) != SEAHORSE_USAGE_PRIVATE_KEY)
        return;

    /* Clear/create table store */
    widget = seahorse_widget_get_widget (swidget, "names-tree");
    g_return_if_fail (widget != NULL);
    
    store = GTK_TREE_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (widget)));
    if (store) {
        gtk_tree_store_clear (store);
        
    } else {
        
        /* This is our first time so create a store */
        store = GTK_TREE_STORE (seahorse_object_model_new (UIDSIG_N_COLUMNS, (GType*)uidsig_columns));
        g_signal_connect (store, "update-row", G_CALLBACK (names_update_row), swidget);
        
        /* Icon column */
        renderer = gtk_cell_renderer_pixbuf_new ();
        g_object_set (renderer, "stock-size", GTK_ICON_SIZE_LARGE_TOOLBAR, NULL);
        gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (widget),
                                                     -1, "", renderer,
                                                     "stock-id", UIDSIG_ICON, NULL);

        /* The name column */
        gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (widget), 
                                                     -1, _("Name/Email"), gtk_cell_renderer_text_new (), 
                                                     "markup", UIDSIG_NAME, NULL);

        /* The signature ID column */
        gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (widget), 
                                                     -1, _("Signature ID"), gtk_cell_renderer_text_new (), 
                                                     "text", UIDSIG_KEYID, NULL);
    }

    names_populate (swidget, store, pkey);
    
    gtk_tree_view_set_model (GTK_TREE_VIEW (widget), GTK_TREE_MODEL(store));
    gtk_tree_view_expand_all (GTK_TREE_VIEW (widget));
    
    update_names (NULL, swidget);
}

static void
do_names_signals (SeahorseWidget *swidget)
{ 
    SeahorseObject *object;
    GtkTreeSelection *selection;
    GtkWidget *widget;
    
    object = SEAHORSE_OBJECT_WIDGET (swidget)->object;
    
    if (seahorse_object_get_usage (object) != SEAHORSE_USAGE_PRIVATE_KEY)
        return;
    widget = seahorse_widget_get_widget (swidget, "names-tree");
    g_return_if_fail (widget != NULL);
    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));
    g_signal_connect (selection, "changed", G_CALLBACK (update_names), swidget);
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

static guint n_targets = G_N_ELEMENTS (target_list);

G_MODULE_EXPORT void
on_pgp_owner_photo_drag_received (GtkWidget *widget,
                                  GdkDragContext *context,
                                  gint x,
                                  gint y,
                                  GtkSelectionData *sel_data,
                                  guint target_type,
                                  guint time,
                                  gpointer user_data)
{
    SeahorseWidget *swidget = SEAHORSE_WIDGET (user_data);
    gboolean dnd_success = FALSE;
    SeahorseGpgmeKey *pkey;
    gchar **uri_list;
    gint len = 0;
    gchar *uri;
    
    pkey = SEAHORSE_GPGME_KEY (SEAHORSE_OBJECT_WIDGET (swidget)->object);
    g_return_if_fail (SEAHORSE_IS_GPGME_KEY (pkey));
    
    /* 
     * This needs to be improved, support should be added for remote images 
     * and there has to be a better way to get rid of the trailing \r\n appended
     * to the end of the path after the call to g_filename_from_uri
     */
    if((sel_data != NULL) && (gtk_selection_data_get_length (sel_data) >= 0)) {
        g_return_if_fail (target_type == TARGET_URI);
        
        uri_list = gtk_selection_data_get_uris (sel_data);
        while (uri_list && uri_list[len]) {
                
            uri = g_filename_from_uri (uri_list[len], NULL, NULL);
            if (!uri)
                continue;
                
            dnd_success = seahorse_gpgme_photo_add (pkey, GTK_WINDOW (seahorse_widget_get_toplevel (swidget)), uri);
            g_free (uri);
            
            if (!dnd_success)
                break;
            len++;
        }
        
        g_strfreev (uri_list);
    }
    
    gtk_drag_finish (context, dnd_success, FALSE, time);
}

G_MODULE_EXPORT void
on_pgp_owner_photo_add_button (GtkWidget *widget,
                               gpointer user_data)
{
	SeahorseWidget *swidget = SEAHORSE_WIDGET (user_data);
	SeahorsePgpKey *pkey;

	pkey = SEAHORSE_PGP_KEY (SEAHORSE_OBJECT_WIDGET (swidget)->object);
	g_return_if_fail (SEAHORSE_IS_GPGME_KEY (pkey)); 
    
	if (seahorse_gpgme_photo_add (SEAHORSE_GPGME_KEY (pkey), GTK_WINDOW (seahorse_widget_get_toplevel (swidget)), NULL))
		g_object_set_data (G_OBJECT (swidget), "current-photoid", NULL);
}
 
G_MODULE_EXPORT void
on_pgp_owner_photo_delete_button (GtkWidget *widget,
                                  gpointer user_data)
{
	SeahorseWidget *swidget = SEAHORSE_WIDGET (user_data);
	SeahorseGpgmePhoto *photo;

	photo = g_object_get_data (G_OBJECT (swidget), "current-photoid");
	g_return_if_fail (SEAHORSE_IS_GPGME_PHOTO (photo));

	if (seahorse_gpgme_key_op_photo_delete (photo))
		g_object_set_data (G_OBJECT (swidget), "current-photoid", NULL);
}

G_MODULE_EXPORT void
on_pgp_owner_photo_primary_button (GtkWidget *widget,
                                   gpointer user_data)
{
	SeahorseWidget *swidget = SEAHORSE_WIDGET (user_data);
	gpgme_error_t gerr;
	SeahorseGpgmePhoto *photo;

	photo = g_object_get_data (G_OBJECT (swidget), "current-photoid");
	g_return_if_fail (SEAHORSE_IS_GPGME_PHOTO (photo));
        
	gerr = seahorse_gpgme_key_op_photo_primary (photo);
	if (!GPG_IS_OK (gerr))
		seahorse_gpgme_handle_error (gerr, _("Couldn't change primary photo"));
}

static void
set_photoid_state (SeahorseWidget *swidget, SeahorsePgpKey *pkey)
{
	SeahorseUsage etype; 
	GtkWidget *photo_image;
	SeahorsePgpPhoto *photo;
	gboolean is_gpgme;
	GdkPixbuf *pixbuf;
	GList *photos;

	etype = seahorse_object_get_usage (SEAHORSE_OBJECT (pkey));
	photos = seahorse_pgp_key_get_photos (pkey);
	
	photo = g_object_get_data (G_OBJECT (swidget), "current-photoid");
	g_return_if_fail (!photo || SEAHORSE_IS_PGP_PHOTO (photo));
	is_gpgme = SEAHORSE_IS_GPGME_KEY (pkey);

	/* Show when adding a photo is possible */
	show_gtkbuilder_widget (swidget, "owner-photo-add-button",
	                   is_gpgme && etype == SEAHORSE_USAGE_PRIVATE_KEY);

	/* Show when we have a photo to set as primary */
	show_gtkbuilder_widget (swidget, "owner-photo-primary-button",
	                   is_gpgme && etype == SEAHORSE_USAGE_PRIVATE_KEY && photos && photos->next);

	/* Display this when there are any photo ids */
	show_gtkbuilder_widget (swidget, "owner-photo-delete-button",
	                   is_gpgme && etype == SEAHORSE_USAGE_PRIVATE_KEY && photo);

	/* Sensitive when not the first photo id */
	sensitive_gtkbuilder_widget (swidget, "owner-photo-previous-button",
	                        photo && photos && photo != g_list_first (photos)->data);
    
	/* Sensitive when not the last photo id */
	sensitive_gtkbuilder_widget (swidget, "owner-photo-next-button",
	                        photo && photos && photo != g_list_last (photos)->data);
    
	/* Display *both* of these when there are more than one photo id */
	show_gtkbuilder_widget (swidget, "owner-photo-previous-button",
	                   photos && g_list_next (photos));
                       
	show_gtkbuilder_widget (swidget, "owner-photo-next-button",
	                   photos && g_list_next (photos));
                       
	photo_image = GTK_WIDGET (seahorse_widget_get_widget (swidget, "photoid"));
	g_return_if_fail (photo_image);
	
	pixbuf = photo ? seahorse_pgp_photo_get_pixbuf (photo) : NULL;
	if (pixbuf)
		gtk_image_set_from_pixbuf (GTK_IMAGE (photo_image), pixbuf);
	else if (etype == SEAHORSE_USAGE_PRIVATE_KEY)
		gtk_image_set_from_stock (GTK_IMAGE (photo_image), SEAHORSE_STOCK_SECRET, (GtkIconSize)-1);
	else 
		gtk_image_set_from_stock (GTK_IMAGE (photo_image), SEAHORSE_STOCK_KEY, (GtkIconSize)-1);
}

static void
do_photo_id (SeahorseWidget *swidget)
{
	SeahorsePgpKey *pkey;
	GList *photos;

	pkey = SEAHORSE_PGP_KEY (SEAHORSE_OBJECT_WIDGET (swidget)->object);
	photos = seahorse_pgp_key_get_photos (pkey);

	if (!photos)
		g_object_set_data (G_OBJECT (swidget), "current-photoid", NULL);
	else
		g_object_set_data_full (G_OBJECT (swidget), "current-photoid", 
		                        g_object_ref (photos->data), g_object_unref);

	set_photoid_state (swidget, pkey);
}

G_MODULE_EXPORT void
on_pgp_owner_photoid_next (GtkWidget *widget,
                           gpointer user_data)
{
	SeahorseWidget *swidget = SEAHORSE_WIDGET (user_data);
	SeahorsePgpKey *pkey;
	SeahorsePgpPhoto *photo;
	GList *photos;

	pkey = SEAHORSE_PGP_KEY (SEAHORSE_OBJECT_WIDGET (swidget)->object);
	photos = seahorse_pgp_key_get_photos (pkey);
	
	photo = g_object_get_data (G_OBJECT (swidget), "current-photoid");
	if (photo) {
		g_return_if_fail (SEAHORSE_IS_PGP_PHOTO (photo));
		photos = g_list_find (photos, photo);
		if(photos && photos->next)
			g_object_set_data_full (G_OBJECT (swidget), "current-photoid", 
			                        g_object_ref (photos->next->data), 
			                        g_object_unref);
	}
        
	set_photoid_state (swidget, pkey);
}

G_MODULE_EXPORT void
on_pgp_owner_photoid_prev (GtkWidget *widget,
                           gpointer user_data)
{
	SeahorseWidget *swidget = SEAHORSE_WIDGET (user_data);
	SeahorsePgpKey *pkey;
	SeahorsePgpPhoto *photo;
	GList *photos;

	pkey = SEAHORSE_PGP_KEY (SEAHORSE_OBJECT_WIDGET (swidget)->object);
	photos = seahorse_pgp_key_get_photos (pkey);

	photo = g_object_get_data (G_OBJECT (swidget), "current-photoid");
	if (photo) {
		g_return_if_fail (SEAHORSE_IS_PGP_PHOTO (photo));
		photos = g_list_find (photos, photo);
		if(photos && photos->prev)
			g_object_set_data_full (G_OBJECT (swidget), "current-photoid", 
			                        g_object_ref (photos->prev->data), 
			                        g_object_unref);
	}
	
	set_photoid_state (swidget, pkey);
}

G_MODULE_EXPORT void
on_pgp_owner_photoid_button (GtkWidget *widget,
                             GdkEvent *event,
                             gpointer user_data)
{
    SeahorseWidget *swidget = SEAHORSE_WIDGET (user_data);
    GdkEventScroll *event_scroll;
    
    if(event->type == GDK_SCROLL) {
        event_scroll = (GdkEventScroll *) event;
        
        if (event_scroll->direction == GDK_SCROLL_UP)
            on_pgp_owner_photoid_prev (widget, swidget);
        else if (event_scroll->direction == GDK_SCROLL_DOWN)
            on_pgp_owner_photoid_next (widget, swidget);
    }
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

const GType uid_columns[] = {
    G_TYPE_OBJECT,  /* object */
    G_TYPE_STRING,  /* icon */
    G_TYPE_STRING,  /* name */
    G_TYPE_STRING,  /* email */
    G_TYPE_STRING   /* comment */
};

G_MODULE_EXPORT void
on_pgp_owner_passphrase_button_clicked (GtkWidget *widget,
                                        gpointer user_data)
{
	SeahorseWidget *swidget = SEAHORSE_WIDGET (user_data);
	SeahorseObject *object = SEAHORSE_OBJECT_WIDGET (swidget)->object;
	if (seahorse_object_get_usage (object) == SEAHORSE_USAGE_PRIVATE_KEY && 
	    SEAHORSE_IS_GPGME_KEY (object))
		seahorse_gpgme_key_op_change_pass (SEAHORSE_GPGME_KEY (object));
}

static void
do_owner_signals (SeahorseWidget *swidget)
{ 
    SeahorseObject *object;
    SeahorseUsage etype;
    GtkWidget *frame;
    
    object = SEAHORSE_OBJECT_WIDGET (swidget)->object;
    etype = seahorse_object_get_usage (object);

    if (etype == SEAHORSE_USAGE_PRIVATE_KEY ) {
        frame = GTK_WIDGET (seahorse_widget_get_widget (swidget, "owner-photo-frame"));
        gtk_drag_dest_set (frame, GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_HIGHLIGHT |
                           GTK_DEST_DEFAULT_DROP, target_list, n_targets, GDK_ACTION_COPY);
    } else {
        show_gtkbuilder_widget (swidget, "owner-photo-add-button", FALSE);
        show_gtkbuilder_widget (swidget, "owner-photo-delete-button", FALSE);
        show_gtkbuilder_widget (swidget, "owner-photo-primary-button", FALSE);
        show_gtkbuilder_widget (swidget, "passphrase-button", FALSE);
    }
}

static void
do_owner (SeahorseWidget *swidget)
{
	SeahorseObject *object;
	SeahorsePgpKey *pkey;
	SeahorsePgpUid *uid;
	GtkWidget *widget;
	GtkCellRenderer *renderer;
	GtkListStore *store;
	GtkTreeIter iter;
	gchar *text, *t;
	gulong expires_date;
	guint flags;
	const gchar *markup;
	const gchar *label;
	GList *uids, *l;

	object = SEAHORSE_OBJECT_WIDGET (swidget)->object;
	pkey = SEAHORSE_PGP_KEY (object);

	flags = seahorse_object_get_flags (object);
    
	/* Display appropriate warnings */    
	show_gtkbuilder_widget (swidget, "expired-area", flags & SEAHORSE_FLAG_EXPIRED);
	show_gtkbuilder_widget (swidget, "revoked-area", flags & SEAHORSE_FLAG_REVOKED);
	show_gtkbuilder_widget (swidget, "disabled-area", flags & SEAHORSE_FLAG_DISABLED);
    
	/* Update the expired message */
	if (flags & SEAHORSE_FLAG_EXPIRED) {
		widget = GTK_WIDGET (gtk_builder_get_object (swidget->gtkbuilder, "expired-message"));
		if (widget) {
            
			expires_date = seahorse_pgp_key_get_expires (pkey);
			if (expires_date == 0) 
				t = g_strdup (_("(unknown)"));
			else
				t = seahorse_util_get_display_date_string (expires_date);
			text = g_strdup_printf (_("This key expired on: %s"), t);
			
			gtk_label_set_text (GTK_LABEL (widget), text);
            
			g_free (t);
			g_free (text);
		}
	}
        
	/* Hide trust page when above */
	show_gtkbuilder_widget (swidget, "trust-page", !((flags & SEAHORSE_FLAG_EXPIRED) ||
			   (flags & SEAHORSE_FLAG_REVOKED) || (flags & SEAHORSE_FLAG_DISABLED)));

	/* Hide or show the uids area */
	uids = seahorse_pgp_key_get_uids (pkey);
	show_gtkbuilder_widget (swidget, "uids-area", uids != NULL);
	if (uids != NULL) {
		uid = SEAHORSE_PGP_UID (uids->data);
    
		label = seahorse_pgp_uid_get_name (uid);
		widget = GTK_WIDGET (seahorse_widget_get_widget (swidget, "owner-name-label"));
		gtk_label_set_text (GTK_LABEL (widget), label); 
		widget = GTK_WIDGET (seahorse_widget_get_widget (swidget, swidget->name));
		gtk_window_set_title (GTK_WINDOW (widget), label);

		label = seahorse_pgp_uid_get_email (uid);
		widget = GTK_WIDGET (seahorse_widget_get_widget (swidget, "owner-email-label"));
		gtk_label_set_text (GTK_LABEL (widget), label); 

		label = seahorse_pgp_uid_get_comment (uid);
		widget = GTK_WIDGET (seahorse_widget_get_widget (swidget, "owner-comment-label"));
		gtk_label_set_text (GTK_LABEL (widget), label); 
    
		widget = GTK_WIDGET (gtk_builder_get_object (swidget->gtkbuilder, "owner-keyid-label"));
		if (widget) {
			label = seahorse_object_get_identifier (object); 
			gtk_label_set_text (GTK_LABEL (widget), label);
		}
	}
    
	/* Clear/create table store */
	widget = GTK_WIDGET (gtk_builder_get_object (swidget->gtkbuilder, "owner-userid-tree"));
	if (widget) {
		store = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (widget)));
    
		if (store) {
			gtk_list_store_clear (GTK_LIST_STORE (store));
            
		} else {
    
			/* This is our first time so create a store */
			store = gtk_list_store_newv (UID_N_COLUMNS, (GType*)uid_columns);
    
			/* Make the columns for the view */
			renderer = gtk_cell_renderer_pixbuf_new ();
			g_object_set (renderer, "stock-size", GTK_ICON_SIZE_LARGE_TOOLBAR, NULL);
			gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (widget),
			                                             -1, "", renderer,
			                                             "stock-id", UID_ICON, NULL);

			gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (widget), 
			                                             -1, _("Name"), gtk_cell_renderer_text_new (), 
			                                             "markup", UID_MARKUP, NULL);
		}
    
		for (l = uids; l; l = g_list_next (l)) {
    
			markup = seahorse_object_get_markup (l->data);
			gtk_list_store_append (store, &iter);
			gtk_list_store_set (store, &iter,  
			                    UID_OBJECT, l->data,
			                    UID_ICON, SEAHORSE_STOCK_PERSON,
			                    UID_MARKUP, markup, -1);
		} 
        
		gtk_tree_view_set_model (GTK_TREE_VIEW (widget), GTK_TREE_MODEL(store));
	}
    
	do_photo_id (swidget);
}

/* -----------------------------------------------------------------------------
 * DETAILS PAGE 
 */

/* details subkey list */
enum {
	SUBKEY_OBJECT,
	SUBKEY_ID,
	SUBKEY_TYPE,
	SUBKEY_CREATED,
	SUBKEY_EXPIRES,
	SUBKEY_STATUS,
	SUBKEY_LENGTH,
	SUBKEY_N_COLUMNS
};

const GType subkey_columns[] = {
    G_TYPE_OBJECT,  /* index */
    G_TYPE_STRING,  /* id */
    G_TYPE_STRING,  /* created */
    G_TYPE_STRING,  /* expires */
    G_TYPE_STRING,  /* status  */
    G_TYPE_STRING,  /* type */
    G_TYPE_UINT     /* length*/
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
get_selected_subkey (SeahorseWidget *swidget)
{
	return get_selected_object (swidget, "details-subkey-tree", SUBKEY_OBJECT);
}

static void
details_subkey_selected (GtkTreeSelection *selection, SeahorseWidget *swidget)
{
	SeahorsePgpSubkey* subkey;
	guint flags = 0;
	
	subkey = get_selected_subkey (swidget);
	if (subkey)
		flags = seahorse_pgp_subkey_get_flags (subkey);
	
	sensitive_gtkbuilder_widget (swidget, "details-date-button", subkey != NULL);
	sensitive_gtkbuilder_widget (swidget, "details-revoke-button", subkey != NULL && !(flags & SEAHORSE_FLAG_REVOKED));
	sensitive_gtkbuilder_widget (swidget, "details-delete-button", subkey != NULL);
}

G_MODULE_EXPORT void
on_pgp_details_add_subkey_button (GtkButton *button,
                                  gpointer user_data)
{
	SeahorseWidget *swidget = SEAHORSE_WIDGET (user_data);
	SeahorseObject *object = SEAHORSE_OBJECT_WIDGET (swidget)->object;
	g_return_if_fail (SEAHORSE_IS_GPGME_KEY (object));
	seahorse_gpgme_add_subkey_new (SEAHORSE_GPGME_KEY (object), GTK_WINDOW (seahorse_widget_get_widget (swidget, swidget->name)));
}

G_MODULE_EXPORT void
on_pgp_details_del_subkey_button (GtkButton *button,
                                  gpointer user_data)
{
	SeahorseWidget *swidget = SEAHORSE_WIDGET (user_data);
	SeahorsePgpSubkey *subkey; 
	SeahorsePgpKey *pkey; 
	guint index;
	gboolean ret;
	const gchar *label;
	gchar *message; 
	gpgme_error_t err;

	pkey = SEAHORSE_PGP_KEY (SEAHORSE_OBJECT_WIDGET (swidget)->object);
	subkey = get_selected_subkey (swidget);
	if (!subkey)
		return;
	
	g_return_if_fail (SEAHORSE_IS_GPGME_SUBKEY (subkey));
	
	index = seahorse_pgp_subkey_get_index (subkey);
	label = seahorse_object_get_label (SEAHORSE_OBJECT (pkey));
	message = g_strdup_printf (_("Are you sure you want to permanently delete subkey %d of %s?"), index, label);
	ret = seahorse_util_prompt_delete (message, seahorse_widget_get_toplevel (swidget));
	g_free (message);
	
	if (ret == FALSE)
		return;
	
	err = seahorse_gpgme_key_op_del_subkey (SEAHORSE_GPGME_SUBKEY (subkey));
	if (!GPG_IS_OK (err))
		seahorse_gpgme_handle_error (err, _("Couldn't delete subkey"));
}

G_MODULE_EXPORT void
on_pgp_details_revoke_subkey_button (GtkButton *button,
                                     gpointer user_data)
{
	SeahorseWidget *swidget = SEAHORSE_WIDGET (user_data);
	SeahorsePgpSubkey *subkey = get_selected_subkey (swidget);
	if (subkey != NULL) {
		g_return_if_fail (SEAHORSE_IS_GPGME_SUBKEY (subkey));
		seahorse_gpgme_revoke_new (SEAHORSE_GPGME_SUBKEY (subkey), GTK_WINDOW (seahorse_widget_get_widget (swidget, swidget->name)));
	}
}

G_MODULE_EXPORT void
on_pgp_details_trust_changed (GtkComboBox *selection,
                              gpointer user_data)
{
	SeahorseWidget *swidget = SEAHORSE_WIDGET (user_data);
	SeahorseObject *object;
	gint trust;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gpgme_error_t err;
	gboolean set;
	
	set = gtk_combo_box_get_active_iter (selection, &iter);
	if (!set) 
		return;
	
	object = SEAHORSE_OBJECT_WIDGET (swidget)->object;
	g_return_if_fail (SEAHORSE_IS_GPGME_KEY (object));
    	
	model = gtk_combo_box_get_model (selection);
	gtk_tree_model_get (model, &iter, TRUST_VALIDITY, &trust, -1);
                                  
	if (seahorse_pgp_key_get_trust (SEAHORSE_PGP_KEY (object)) != trust) {
		err = seahorse_gpgme_key_op_set_trust (SEAHORSE_GPGME_KEY (object), trust);
		if (err)
    			seahorse_gpgme_handle_error (err, _("Unable to change trust"));
	}
}

static void
export_complete (GFile *file, GAsyncResult *result, gchar *contents)
{
	GError *err = NULL;
	gchar *uri, *unesc_uri;
	
	g_free (contents);

	if (!g_file_replace_contents_finish (file, result, NULL, &err)) {
		uri = g_file_get_uri (file);
		unesc_uri = g_uri_unescape_string (seahorse_util_uri_get_last (uri), NULL);
		seahorse_util_handle_error (&err, NULL, _("Couldn't export key to \"%s\""),
		                            unesc_uri);
        g_free (uri);
        g_free (unesc_uri);
	}
}

G_MODULE_EXPORT void
on_pgp_details_export_button (GtkWidget *widget,
                              gpointer user_data)
{
	SeahorseWidget *swidget = SEAHORSE_WIDGET (user_data);
	SeahorseObject *object;
	GtkDialog *dialog;
	gchar* uri = NULL;
	GError *err = NULL;
	GFile *file;
	gpgme_error_t gerr;
	gpgme_ctx_t ctx;
	gpgme_data_t data;
	gchar *results;
	gsize n_results;
	gpgme_key_t seckey;
    
	object = SEAHORSE_OBJECT_WIDGET (swidget)->object;
	g_return_if_fail (SEAHORSE_IS_GPGME_KEY (object));

	seckey = seahorse_gpgme_key_get_private (SEAHORSE_GPGME_KEY (object));
	g_return_if_fail (seckey && seckey->subkeys && seckey->subkeys->keyid);

	dialog = seahorse_util_chooser_save_new (_("Export Complete Key"), 
	                                         GTK_WINDOW (seahorse_widget_get_toplevel (swidget)));
	seahorse_util_chooser_show_key_files (dialog);
	seahorse_util_chooser_set_filename (dialog, object);
    
	uri = seahorse_util_chooser_save_prompt (dialog);
	if (!uri) 
		return;
	
	/* Export to a data block */
	gerr = gpgme_data_new (&data);
	g_return_if_fail (GPG_IS_OK (gerr));
	ctx = seahorse_gpgme_source_new_context ();
	g_return_if_fail (ctx);
	gerr = seahorse_gpg_op_export_secret (ctx, seckey->subkeys->keyid, data);
	gpgme_release (ctx);
	results = gpgme_data_release_and_get_mem (data, &n_results);

	if (GPG_IS_OK (gerr)) {
		file = g_file_new_for_uri (uri);
		g_file_replace_contents_async (file, results, n_results, NULL, FALSE, 
		                               G_FILE_CREATE_PRIVATE, NULL, 
		                               (GAsyncReadyCallback)export_complete, results);
	} else {
		seahorse_gpgme_propagate_error (gerr, &err);
		seahorse_util_handle_error (&err, NULL, _("Couldn't export key."));
	}
	
	g_free (uri);
}

G_MODULE_EXPORT void
on_pgp_details_expires_button (GtkWidget *widget,
                               gpointer user_data)
{
	SeahorseWidget *swidget = SEAHORSE_WIDGET (user_data);
	GList *subkeys;
	SeahorsePgpKey *pkey;
	
	pkey = SEAHORSE_PGP_KEY (SEAHORSE_OBJECT_WIDGET (swidget)->object);
	subkeys = seahorse_pgp_key_get_subkeys (pkey);
	g_return_if_fail (subkeys);
	
	seahorse_gpgme_expires_new (SEAHORSE_GPGME_SUBKEY (subkeys->data), 
	                            GTK_WINDOW (seahorse_widget_get_widget (swidget, swidget->name)));
}

G_MODULE_EXPORT void
on_pgp_details_expires_subkey (GtkWidget *widget,
                               gpointer user_data)
{
	SeahorseWidget *swidget = SEAHORSE_WIDGET (user_data);
	SeahorsePgpSubkey *subkey;
	SeahorsePgpKey *pkey;
	GList *subkeys;
	
	subkey = get_selected_subkey (swidget);
	if (subkey == NULL) {
		pkey = SEAHORSE_PGP_KEY (SEAHORSE_OBJECT_WIDGET (swidget)->object);
		subkeys = seahorse_pgp_key_get_subkeys (pkey);
		if (subkeys)
			subkey = subkeys->data;
	}
	
	g_return_if_fail (SEAHORSE_IS_GPGME_SUBKEY (subkey));

	if (subkey != NULL)
		seahorse_gpgme_expires_new (SEAHORSE_GPGME_SUBKEY (subkey), 
		                            GTK_WINDOW (seahorse_widget_get_widget (swidget, swidget->name)));
}

static void
setup_details_trust (SeahorseWidget *swidget)
{
    SeahorseObject *object;
    SeahorseUsage etype;
    GtkWidget *widget;
    GtkListStore *model;
    GtkTreeIter iter;
    GtkCellRenderer *text_cell = gtk_cell_renderer_text_new ();

    seahorse_debug ("KeyProperties: Setting up Trust Combo Box Store");

    object = SEAHORSE_OBJECT_WIDGET (swidget)->object;
    etype = seahorse_object_get_usage (object);
    
    widget = GTK_WIDGET (seahorse_widget_get_widget (swidget, "details-trust-combobox"));
    
    gtk_cell_layout_clear(GTK_CELL_LAYOUT (widget));
    
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT (widget), text_cell, FALSE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT (widget), text_cell,
                                   "text", TRUST_LABEL,
                                   NULL);
    
    /* Initialize the store */
    model = gtk_list_store_newv (TRUST_N_COLUMNS, (GType*)trust_columns);
    
    if (etype != SEAHORSE_USAGE_PRIVATE_KEY) {
        gtk_list_store_append (model, &iter);
        gtk_list_store_set (model, &iter,
                            TRUST_LABEL, _("Unknown"),
                            TRUST_VALIDITY,  SEAHORSE_VALIDITY_UNKNOWN,
                            -1);                               
       
        gtk_list_store_append (model, &iter);
        gtk_list_store_set (model, &iter,
                            TRUST_LABEL, C_("Validity","Never"),
                            TRUST_VALIDITY,  SEAHORSE_VALIDITY_NEVER,
                            -1);
    }
    
    gtk_list_store_append (model, &iter);
    gtk_list_store_set (model, &iter,
                        TRUST_LABEL, _("Marginal"),
                        TRUST_VALIDITY,  SEAHORSE_VALIDITY_MARGINAL,
                        -1);
    
    gtk_list_store_append (model, &iter);
    gtk_list_store_set (model, &iter,
                        TRUST_LABEL, _("Full"),
                        TRUST_VALIDITY,  SEAHORSE_VALIDITY_FULL,
                        -1);
    if (etype == SEAHORSE_USAGE_PRIVATE_KEY) {
        gtk_list_store_append (model, &iter);
        gtk_list_store_set (model, &iter,
                            TRUST_LABEL, _("Ultimate"),
                            TRUST_VALIDITY,  SEAHORSE_VALIDITY_ULTIMATE,
                            -1);
    }
    
    gtk_combo_box_set_model (GTK_COMBO_BOX (widget), GTK_TREE_MODEL (model));                                        

    seahorse_debug ("KeyProperties: Finished Setting up Trust Combo Box Store");
}

static void
do_details_signals (SeahorseWidget *swidget) 
{ 
    SeahorseObject *object;
    SeahorseUsage etype;
    GtkWidget *widget;
    
    object = SEAHORSE_OBJECT_WIDGET (swidget)->object;
    etype = seahorse_object_get_usage (object);
    
    /* 
     * if not the key owner, disable most everything
     * if key owner, add the callbacks to the subkey buttons
     */
     if (etype == SEAHORSE_USAGE_PUBLIC_KEY) {
         show_gtkbuilder_widget (swidget, "details-actions-label", FALSE);
         show_gtkbuilder_widget (swidget, "details-export-button", FALSE);
         show_gtkbuilder_widget (swidget, "details-add-button", FALSE);
         show_gtkbuilder_widget (swidget, "details-date-button", FALSE);
         show_gtkbuilder_widget (swidget, "details-revoke-button", FALSE);
         show_gtkbuilder_widget (swidget, "details-delete-button", FALSE);
         show_gtkbuilder_widget (swidget, "details-calendar-button", FALSE);
    } else {

        /* Connect so we can enable and disable buttons as subkeys are selected */
        widget = GTK_WIDGET (seahorse_widget_get_widget (swidget, "details-subkey-tree"));
        g_signal_connect (gtk_tree_view_get_selection (GTK_TREE_VIEW (widget)),
                          "changed", G_CALLBACK (details_subkey_selected), swidget);
        details_subkey_selected (NULL, swidget);
    }
}

static void 
do_details (SeahorseWidget *swidget)
{
    SeahorseObject *object;
    SeahorsePgpKey *pkey;
    SeahorsePgpSubkey *subkey;
    GtkWidget *widget;
    GtkListStore *store;
    GtkTreeModel *model;
    GtkTreeIter iter;
    gchar dbuffer[G_ASCII_DTOSTR_BUF_SIZE];
    gchar *fp_label;
    const gchar *label;
    gint trust;
    GList *subkeys, *l;
    guint keyloc;
    gboolean valid;

    object = SEAHORSE_OBJECT_WIDGET (swidget)->object;
    pkey = SEAHORSE_PGP_KEY (object);
    
    subkeys = seahorse_pgp_key_get_subkeys (pkey);
    g_return_if_fail (subkeys && subkeys->data);
    subkey = SEAHORSE_PGP_SUBKEY (subkeys->data);
    
    keyloc = seahorse_object_get_location (object);

    widget = GTK_WIDGET (gtk_builder_get_object (swidget->gtkbuilder, "details-id-label"));
    if (widget) {
        label = seahorse_object_get_identifier (object); 
        gtk_label_set_text (GTK_LABEL (widget), label);
    }

    widget = GTK_WIDGET (gtk_builder_get_object (swidget->gtkbuilder, "details-fingerprint-label"));
    if (widget) {
        fp_label = g_strdup (seahorse_pgp_key_get_fingerprint (pkey)); 
        if (strlen (fp_label) > 24)
            fp_label[24] = '\n';
        gtk_label_set_text (GTK_LABEL (widget), fp_label);
        g_free (fp_label);
    }

    widget = GTK_WIDGET (gtk_builder_get_object (swidget->gtkbuilder, "details-algo-label"));
    if (widget) {
        label = seahorse_pgp_key_get_algo (pkey);
        gtk_label_set_text (GTK_LABEL (widget), label);
    }

    widget = GTK_WIDGET (gtk_builder_get_object (swidget->gtkbuilder, "details-created-label"));
    if (widget) {
        fp_label = seahorse_util_get_display_date_string (seahorse_pgp_subkey_get_created (subkey));
        gtk_label_set_text (GTK_LABEL (widget), fp_label);
        g_free (fp_label);
    }

    widget = GTK_WIDGET (gtk_builder_get_object (swidget->gtkbuilder, "details-strength-label"));
    if (widget) {
        g_ascii_dtostr (dbuffer, G_ASCII_DTOSTR_BUF_SIZE, seahorse_pgp_subkey_get_length (subkey));
        gtk_label_set_text (GTK_LABEL (widget), dbuffer);
    }

    widget = GTK_WIDGET (gtk_builder_get_object (swidget->gtkbuilder, "details-expires-label"));
    if (widget) {
        gulong expires = seahorse_pgp_subkey_get_expires (subkey);
        if (seahorse_object_get_location (object) == SEAHORSE_LOCATION_REMOTE)
            fp_label = NULL;
        else if (expires == 0)
            fp_label = g_strdup (C_("Expires", "Never"));
        else
            fp_label = seahorse_util_get_display_date_string (expires);
        gtk_label_set_text (GTK_LABEL (widget), fp_label);
        g_free (fp_label);
    }

    show_gtkbuilder_widget (swidget, "details-trust-combobox", keyloc == SEAHORSE_LOCATION_LOCAL);
    widget = GTK_WIDGET (gtk_builder_get_object (swidget->gtkbuilder, "details-trust-combobox"));
    
    if (widget) {
        gtk_widget_set_sensitive (widget, !(seahorse_object_get_flags (object) & SEAHORSE_FLAG_DISABLED));
        model = gtk_combo_box_get_model (GTK_COMBO_BOX (widget));
        
        valid = gtk_tree_model_get_iter_first (model, &iter);
       
        while (valid) {
            gtk_tree_model_get (model, &iter,
                                TRUST_VALIDITY, &trust,
                                -1);
            
            if (trust == seahorse_pgp_key_get_trust (pkey)) {
                g_signal_handlers_block_by_func (widget, on_pgp_details_trust_changed, swidget);
                gtk_combo_box_set_active_iter (GTK_COMBO_BOX (widget),  &iter);
                g_signal_handlers_unblock_by_func (widget, on_pgp_details_trust_changed, swidget);
                break;
            }
            
            valid = gtk_tree_model_iter_next (model, &iter);             
        }
    }

    /* Clear/create table store */
    widget = GTK_WIDGET (seahorse_widget_get_widget (swidget, "details-subkey-tree"));
    store = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (widget)));

    if (store) {
        gtk_list_store_clear (store);
        
    } else {
        
        /* This is our first time so create a store */
        store = gtk_list_store_newv (SUBKEY_N_COLUMNS, (GType*)subkey_columns);
        gtk_tree_view_set_model (GTK_TREE_VIEW (widget), GTK_TREE_MODEL (store));

        /* Make the columns for the view */
        gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (widget), 
                                                     -1, _("ID"), gtk_cell_renderer_text_new (), 
                                                     "text", SUBKEY_ID, NULL);
        gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (widget),
                                                     -1, _("Type"), gtk_cell_renderer_text_new (), 
                                                     "text", SUBKEY_TYPE, NULL);
        gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (widget),
                                                     -1, _("Created"), gtk_cell_renderer_text_new (), 
                                                     "text", SUBKEY_CREATED, NULL);
        gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (widget),
                                                     -1, _("Expires"), gtk_cell_renderer_text_new (), 
                                                     "text", SUBKEY_EXPIRES, NULL);
        gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (widget),
                                                     -1, _("Status"), gtk_cell_renderer_text_new (), 
                                                     "text", SUBKEY_STATUS, NULL);
        gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (widget),
                                                     -1, _("Strength"), gtk_cell_renderer_text_new (), 
                                                     "text", SUBKEY_LENGTH, NULL);
    }

	for (l = subkeys; l; l = g_list_next (l)) {
		
		const gchar *status = NULL;
		gchar *expiration_date;
		gchar *created_date;
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
        
		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter,  
		                    SUBKEY_OBJECT, l->data,
		                    SUBKEY_ID, seahorse_pgp_subkey_get_keyid (l->data),
		                    SUBKEY_TYPE, seahorse_pgp_subkey_get_algorithm (l->data),
		                    SUBKEY_CREATED, created_date,
		                    SUBKEY_EXPIRES, expiration_date,
		                    SUBKEY_STATUS,  status, 
		                    SUBKEY_LENGTH, seahorse_pgp_subkey_get_length (subkey),
		                    -1);
        
		g_free (expiration_date);
		g_free (created_date);
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

const GType sign_columns[] = {
    G_TYPE_STRING,
    G_TYPE_STRING,
    G_TYPE_STRING,
    G_TYPE_BOOLEAN
};

G_MODULE_EXPORT void
seahorse_pgp_details_signatures_delete_button (GtkWidget *widget, SeahorseWidget *skey)
{

}

G_MODULE_EXPORT void
on_pgp_details_signatures_revoke_button (GtkWidget *widget, SeahorseWidget *skey)
{

}

G_MODULE_EXPORT void
on_pgp_trust_marginal_toggled (GtkToggleButton *toggle,
                               gpointer user_data)
{
    SeahorseWidget *swidget = SEAHORSE_WIDGET (user_data);
    SeahorseObject *object;
    guint trust;
    gpgme_error_t err;
    
    object = SEAHORSE_OBJECT_WIDGET (swidget)->object;
    g_return_if_fail (SEAHORSE_IS_GPGME_KEY (object));
    
    trust = gtk_toggle_button_get_active (toggle) ?
            SEAHORSE_VALIDITY_MARGINAL : SEAHORSE_VALIDITY_UNKNOWN;
    
    if (seahorse_pgp_key_get_trust (SEAHORSE_PGP_KEY (object)) != trust) {
        err = seahorse_gpgme_key_op_set_trust (SEAHORSE_GPGME_KEY (object), trust);
        if (err)
        	seahorse_gpgme_handle_error (err, _("Unable to change trust"));
    }
}

/* Is called whenever a signature key changes */
static void
trust_update_row (SeahorseObjectModel *skmodel, SeahorseObject *object, 
                  GtkTreeIter *iter, SeahorseWidget *swidget)
{
	SeahorseObject *preferred;
	gboolean trusted = FALSE;
	const gchar *icon;
	const gchar *name, *id;
    
    	/* Always use the most preferred key for this keyid */
	preferred = seahorse_object_get_preferred (object);
	if (SEAHORSE_IS_OBJECT (preferred)) {
		while (SEAHORSE_IS_OBJECT (preferred)) {
			object = SEAHORSE_OBJECT (preferred);
			preferred = seahorse_object_get_preferred (preferred);
		}
		seahorse_object_model_set_row_object (skmodel, iter, object);
	}


	if (seahorse_object_get_usage (object) == SEAHORSE_USAGE_PRIVATE_KEY) 
		trusted = TRUE;
	else if (seahorse_object_get_flags (object) & SEAHORSE_FLAG_TRUSTED)
		trusted = TRUE;
    
	icon = seahorse_object_get_location (object) < SEAHORSE_LOCATION_LOCAL ? 
	                GTK_STOCK_DIALOG_QUESTION : SEAHORSE_STOCK_SIGN;
	name = seahorse_object_get_label (object);
	id = seahorse_object_get_identifier (object);
	
	gtk_tree_store_set (GTK_TREE_STORE (skmodel), iter,
	                    SIGN_ICON, icon,
	                    SIGN_NAME, name ? name : _("[Unknown]"),
	                    SIGN_KEYID, id,
	                    SIGN_TRUSTED, trusted,
                        -1);
}

static void
signatures_populate_model (SeahorseWidget *swidget, SeahorseObjectModel *skmodel)
{
	SeahorseObject *object;
	SeahorsePgpKey *pkey;
	GtkTreeIter iter;
	GtkWidget *widget;
	gboolean have_sigs = FALSE;
	GList *rawids = NULL;
	GList *keys, *l, *uids;
	GList *sigs, *s;
	GCancellable *cancellable;

	pkey = SEAHORSE_PGP_KEY (SEAHORSE_OBJECT_WIDGET (swidget)->object);
	widget = GTK_WIDGET (seahorse_widget_get_widget (swidget, "signatures-tree"));
	if (!widget)
		return;
    
	uids = seahorse_pgp_key_get_uids (pkey);

	/* Build a list of all the keyids */        
	for (l = uids; l; l = g_list_next (l)) {
		sigs = seahorse_pgp_uid_get_signatures (l->data);
		for (s = sigs; s; s = g_list_next (s)) {
			/* Never show self signatures, they're implied */
			if (seahorse_pgp_key_has_keyid (pkey, seahorse_pgp_signature_get_keyid (s->data)))
				continue;
			have_sigs = TRUE;
			rawids = g_list_prepend (rawids, (gpointer)seahorse_pgp_signature_get_keyid (s->data));
		}
	}
    
	/* Only show signatures area when there are signatures */
	seahorse_widget_set_visible (swidget, "signatures-area", have_sigs);

	if (skmodel) {
    
		/* String out duplicates */
		rawids = unique_slist_strings (rawids);

		/*
		 * Pass it to 'DiscoverKeys' for resolution/download. cancellable ties
		 * search scope together
		 */
		cancellable = g_cancellable_new ();
		keys = seahorse_context_discover_objects (seahorse_context_instance (), SEAHORSE_PGP,
		                                          rawids, cancellable);
		g_object_unref (cancellable);
		g_list_free (rawids);
		rawids = NULL;
        
		/* Add the keys to the store */
		for (l = keys; l; l = g_list_next (l)) {
			object = SEAHORSE_OBJECT (l->data);
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
G_MODULE_EXPORT void
on_pgp_trust_sign (GtkWidget *widget,
                   gpointer user_data)
{
	SeahorseWidget *swidget = SEAHORSE_WIDGET (user_data);
	SeahorseObject *object = SEAHORSE_OBJECT_WIDGET (swidget)->object;
	g_return_if_fail (SEAHORSE_IS_GPGME_KEY (object));
	seahorse_gpgme_sign_prompt (SEAHORSE_GPGME_KEY (object), GTK_WINDOW (seahorse_widget_get_widget (swidget, swidget->name)));
}

static void
do_trust_signals (SeahorseWidget *swidget)
{
    SeahorseObject *object;
    SeahorseUsage etype;
    const gchar *user;
    
    object = SEAHORSE_OBJECT_WIDGET (swidget)->object;
    etype = seahorse_object_get_usage (object);
    
    if (etype != SEAHORSE_USAGE_PUBLIC_KEY)
        return;
    
    set_gtkbuilder_image (swidget, "image-good1", "seahorse-sign-ok");
    set_gtkbuilder_image (swidget, "image-good2", "seahorse-sign-ok");
    
    /* TODO: Hookup revoke handler */
    
    if (etype == SEAHORSE_USAGE_PUBLIC_KEY ) {
        
        show_gtkbuilder_widget (swidget, "signatures-revoke-button", FALSE);
        show_gtkbuilder_widget (swidget, "signatures-delete-button", FALSE);
        show_gtkbuilder_widget (swidget, "signatures-empty-label", FALSE);
        
        /* Fill in trust labels with name .This only happens once, so it sits here. */
        user = seahorse_object_get_label (object);
        printf_gtkbuilder_widget (swidget, "trust-marginal-check", user);
        printf_gtkbuilder_widget (swidget, "trust-sign-label", user);
        printf_gtkbuilder_widget (swidget, "trust-revoke-label", user);
    }
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
do_trust (SeahorseWidget *swidget)
{
    SeahorseObject *object;
    SeahorsePgpKey *pkey;
    GtkWidget *widget;
    GtkTreeStore *store;
    GtkTreeModelFilter *filter;
    gboolean sigpersonal;
    GtkCellRenderer *renderer;
    guint keyloc;
    
    object = SEAHORSE_OBJECT_WIDGET (swidget)->object;
    pkey = SEAHORSE_PGP_KEY (object);
    keyloc = seahorse_object_get_location (object);
    
    if (seahorse_object_get_usage (object) != SEAHORSE_USAGE_PUBLIC_KEY)
        return;
    
    /* Remote keys */
    if (keyloc < SEAHORSE_LOCATION_LOCAL) {
        
        show_gtkbuilder_widget (swidget, "manual-trust-area", FALSE);
        show_gtkbuilder_widget (swidget, "manage-trust-area", TRUE);
        show_gtkbuilder_widget (swidget, "sign-area", FALSE);
        show_gtkbuilder_widget (swidget, "revoke-area", FALSE);
        sensitive_gtkbuilder_widget (swidget, "trust-marginal-check", FALSE);
        set_gtkbuilder_image (swidget, "sign-image", SEAHORSE_STOCK_SIGN_UNKNOWN);
        
    /* Local keys */
    } else {
        guint trust;
        gboolean managed;
        const gchar *icon = NULL;
        
        trust = seahorse_pgp_key_get_trust (pkey);

        managed = FALSE;
        
        switch (trust) {
    
        /* We shouldn't be seeing this page with these trusts */
        case SEAHORSE_VALIDITY_REVOKED:
        case SEAHORSE_VALIDITY_DISABLED:
            return;
        
        /* Trust is specified manually */
        case SEAHORSE_VALIDITY_ULTIMATE:
            managed = FALSE;
            icon = SEAHORSE_STOCK_SIGN_OK;
            break;
        
        /* Trust is specified manually */
        case SEAHORSE_VALIDITY_NEVER:
            managed = FALSE;
            icon = SEAHORSE_STOCK_SIGN_BAD;
            break;
        
        /* We manage the trust through this page */
        case SEAHORSE_VALIDITY_FULL:
        case SEAHORSE_VALIDITY_MARGINAL:
            managed = TRUE;
            icon = SEAHORSE_STOCK_SIGN_OK;
            break;
        
        /* We manage the trust through this page */
        case SEAHORSE_VALIDITY_UNKNOWN:
            managed = TRUE;
            icon = SEAHORSE_STOCK_SIGN;
            break;
        
        default:
            g_warning ("unknown trust value: %d", trust);
            g_assert_not_reached ();
            return;
        }
        
        
        /* Managed and unmanaged areas */
        show_gtkbuilder_widget (swidget, "manual-trust-area", !managed);
        show_gtkbuilder_widget (swidget, "manage-trust-area", managed);
    
        /* Managed check boxes */
        if (managed) {
            widget = GTK_WIDGET (seahorse_widget_get_widget (swidget, "trust-marginal-check"));
            if (widget != NULL) {
                gtk_widget_set_sensitive (widget, TRUE);
            
                g_signal_handlers_block_by_func (widget, on_pgp_trust_marginal_toggled, swidget);
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), trust != SEAHORSE_VALIDITY_UNKNOWN);
                g_signal_handlers_unblock_by_func (widget, on_pgp_trust_marginal_toggled, swidget);
            }
        }
    
        /* Signing and revoking */
        sigpersonal = key_have_signatures (pkey, SKEY_PGPSIG_PERSONAL);
        show_gtkbuilder_widget (swidget, "sign-area", !sigpersonal);
        show_gtkbuilder_widget (swidget, "revoke-area", sigpersonal);
        
        /* The image */
        set_gtkbuilder_image (swidget, "sign-image", icon);
    }
    
	/* The actual signatures listing */
	widget = GTK_WIDGET (seahorse_widget_get_widget (swidget, "signatures-tree"));
	if(widget) {
		filter = GTK_TREE_MODEL_FILTER (gtk_tree_view_get_model (GTK_TREE_VIEW (widget)));
    
		if (filter) {
			store = GTK_TREE_STORE (gtk_tree_model_filter_get_model (filter));
			gtk_tree_store_clear (store);
    
			/* First time create the store */
		} else {
        
			/* Create a new SeahorseObjectModel store.... */
			store = GTK_TREE_STORE (seahorse_object_model_new (SIGN_N_COLUMNS, (GType*)sign_columns));
			g_signal_connect (store, "update-row", G_CALLBACK (trust_update_row), swidget);
        
			/* .... and a filter to go ontop of it */
			filter = GTK_TREE_MODEL_FILTER (gtk_tree_model_filter_new (GTK_TREE_MODEL (store), NULL));
			gtk_tree_model_filter_set_visible_func (filter, 
			                                        (GtkTreeModelFilterVisibleFunc)trust_filter, NULL, NULL);
        
			/* Make the colunms for the view */
			renderer = gtk_cell_renderer_pixbuf_new ();
			g_object_set (renderer, "stock-size", GTK_ICON_SIZE_LARGE_TOOLBAR, NULL);
			gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (widget),
			                                             -1, "", renderer,
			                                             "stock-id", SIGN_ICON, NULL);
			gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (widget), 
			                                             -1, _("Name/Email"), gtk_cell_renderer_text_new (), 
			                                             "text", SIGN_NAME, NULL);
			gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (widget), 
			                                             -1, _("Key ID"), gtk_cell_renderer_text_new (), 
			                                             "text", SIGN_KEYID, NULL);
			
			gtk_tree_view_set_model (GTK_TREE_VIEW (widget), GTK_TREE_MODEL(filter));
			widget = GTK_WIDGET (seahorse_widget_get_widget (swidget, "signatures-toggle"));
			g_signal_connect (widget, "toggled", G_CALLBACK (on_pgp_trusted_toggled), filter);
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);
		}

		signatures_populate_model (swidget, SEAHORSE_OBJECT_MODEL (store));
	}
}

/* -----------------------------------------------------------------------------
 * GENERAL 
 */

static void
key_notify (SeahorseObject *object, SeahorseWidget *swidget)
{
	do_owner (swidget);
        do_names (swidget);
        do_trust (swidget);
        do_details (swidget);
}

static void
properties_response (GtkDialog *dialog, int response, SeahorseWidget *swidget)
{
    if (response == GTK_RESPONSE_HELP) {
        seahorse_widget_show_help(swidget);
        return;
    }

    seahorse_widget_destroy (swidget);
}

static SeahorseWidget*
setup_public_properties (SeahorsePgpKey *pkey, GtkWindow *parent)
{
    SeahorseWidget *swidget;
    GtkWidget *widget;

    swidget = seahorse_object_widget_new ("pgp-public-key-properties", parent, SEAHORSE_OBJECT (pkey));    
    
    /* This happens if the window is already open */
    if (swidget == NULL)
        return NULL;

    /* 
     * The signals don't need to keep getting connected. Everytime a key changes the
     * do_* functions get called. Therefore, seperate functions connect the signals
     * have been created
     */

    do_owner (swidget);
    do_owner_signals (swidget);

    setup_details_trust (swidget);
    do_details (swidget);
    do_details_signals (swidget);

    do_trust (swidget);
    do_trust_signals (swidget);        

    widget = seahorse_widget_get_toplevel(swidget);
    g_signal_connect (widget, "response", G_CALLBACK (properties_response), swidget);
    seahorse_bind_objects (NULL, pkey, (SeahorseTransfer)key_notify, swidget);

    return swidget;
}

static SeahorseWidget*
setup_private_properties (SeahorsePgpKey *pkey, GtkWindow *parent)
{
    SeahorseWidget *swidget;
    GtkWidget *widget;

    swidget = seahorse_object_widget_new ("pgp-private-key-properties", parent, SEAHORSE_OBJECT (pkey));

    /* This happens if the window is already open */
    if (swidget == NULL)
        return NULL;

    /* 
     * The signals don't need to keep getting connected. Everytime a key changes the
     * do_* functions get called. Therefore, seperate functions connect the signals
     * have been created
     */

    do_owner (swidget);
    do_owner_signals (swidget);
    
    do_names (swidget);
    do_names_signals (swidget);

    setup_details_trust (swidget);
    do_details (swidget);
    do_details_signals (swidget);

    widget = GTK_WIDGET (seahorse_widget_get_widget (swidget, swidget->name));
    g_signal_connect (widget, "response", G_CALLBACK (properties_response), swidget);
    seahorse_bind_objects (NULL, pkey, (SeahorseTransfer)key_notify, swidget);
    
    return swidget;
}

void
seahorse_pgp_key_properties_show (SeahorsePgpKey *pkey, GtkWindow *parent)
{
    SeahorseObject *sobj = SEAHORSE_OBJECT (pkey);
    SeahorseSource *sksrc;
    SeahorseWidget *swidget;

    /* Reload the key to make sure to get all the props */
    sksrc = seahorse_object_get_source (sobj);
    g_return_if_fail (sksrc != NULL);
    
    /* Don't trigger the import of remote keys if possible */
    if (SEAHORSE_IS_GPGME_KEY (pkey)) {
        /* This causes the key source to get any specific info about the key */
        seahorse_gpgme_key_refresh (SEAHORSE_GPGME_KEY (pkey));
        seahorse_gpgme_key_ensure_signatures (SEAHORSE_GPGME_KEY (pkey));
        sobj = seahorse_context_get_object (SCTX_APP(), sksrc, seahorse_object_get_id (sobj));
        g_return_if_fail (sobj != NULL);
    }
    
    if (seahorse_object_get_usage (sobj) == SEAHORSE_USAGE_PUBLIC_KEY)
        swidget = setup_public_properties (pkey, parent);
    else
        swidget = setup_private_properties (pkey, parent);       
    
    if (swidget)
        seahorse_widget_show (swidget);
}
