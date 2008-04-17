/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
 * Copyright (C) 2005 Jim Pharis
 * Copyright (C) 2005-2006 Stefan Walter
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
#include <gnome.h>
#include <time.h>

#include "seahorse-gpgmex.h"
#include "seahorse-key-dialogs.h"
#include "seahorse-key-widget.h"
#include "seahorse-util.h"
#include "seahorse-key.h"
#include "seahorse-pgp-key.h"
#include "seahorse-pgp-key-op.h"
#include "seahorse-gtkstock.h"
#include "seahorse-windows.h"
#include "seahorse-vfs-data.h"
#include "seahorse-key-model.h"
#include "seahorse-gconf.h"

#ifdef WITH_KEYSERVER
#include "seahorse-keyserver-sync.h"
#endif

#define NOTEBOOK "notebook"

static void 
show_glade_widget (SeahorseWidget *swidget, const gchar *name, gboolean show)
{
    GtkWidget *widget = glade_xml_get_widget (swidget->xml, name);
    if (widget != NULL)
        seahorse_widget_set_visible (swidget, name, show);
}

static void
set_glade_image (SeahorseWidget *swidget, const gchar *name, const gchar *stock)
{
    GtkWidget *widget = glade_xml_get_widget (swidget->xml, name);
    
    if (!widget)
        return;
    
    gtk_image_set_from_stock (GTK_IMAGE (widget), stock, GTK_ICON_SIZE_DIALOG);
}

static void
sensitive_glade_widget (SeahorseWidget *swidget, const gchar *name, gboolean sens)
{
    GtkWidget *widget = glade_xml_get_widget (swidget->xml, name);
    if (widget)
        gtk_widget_set_sensitive (widget, sens);
}

static void
printf_glade_widget (SeahorseWidget *swidget, const gchar *name, const gchar *str)
{
    GtkWidget *widget;
    const gchar *label;
    gchar *text;
    
    widget = glade_xml_get_widget (swidget->xml, name);
    if (!widget)
        return; 
    
    label = gtk_button_get_label (GTK_BUTTON (widget));
    text = g_strdup_printf (label, str);
    gtk_button_set_label (GTK_BUTTON (widget), text);
    g_free (text);
}

static gint
get_selected_row (SeahorseWidget *swidget, const gchar *gladeid, guint column)
{
    GtkTreeSelection *selection;
    GtkWidget *widget;
    GtkTreeIter iter;
    GtkTreeModel *model;
    GList *rows;
    gint index = -1;

    widget = glade_xml_get_widget (swidget->xml, gladeid);
    model = gtk_tree_view_get_model (GTK_TREE_VIEW (widget));

    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));
    g_assert (gtk_tree_selection_get_mode (selection) == GTK_SELECTION_SINGLE);

    rows = gtk_tree_selection_get_selected_rows (selection, NULL);

    if (g_list_length (rows) > 0) {
        gtk_tree_model_get_iter (model, &iter, rows->data);
        gtk_tree_model_get (model, &iter, column, &index, -1);
    }

    g_list_foreach (rows, (GFunc)gtk_tree_path_free, NULL);
    g_list_free (rows);

    return index;
}

static void
signature_row_activated(GtkTreeView *treeview, GtkTreePath *path, GtkTreeViewColumn *arg2, 
                        SeahorseWidget *swidget)
{
    SeahorseKey *skey = NULL;
    GtkTreeModel *model;
    GtkTreeIter iter;
    
    model = gtk_tree_view_get_model (treeview);
    
    if (GTK_IS_TREE_MODEL_FILTER (model)) 
        model = gtk_tree_model_filter_get_model (GTK_TREE_MODEL_FILTER (model));
        
    g_return_if_fail (gtk_tree_model_get_iter (model, &iter, path));
    
    skey = seahorse_key_model_get_row_key (SEAHORSE_KEY_MODEL (model), &iter);
    
	if (skey != NULL) {
	   if (SEAHORSE_IS_PGP_KEY (skey))
	       seahorse_key_properties_new (SEAHORSE_PGP_KEY (skey), GTK_WINDOW (glade_xml_get_widget (swidget->xml, swidget->name)));
    }
}

static GSList*
unique_slist_strings (GSList *keyids)
{
    GSList *l;
    
    keyids = g_slist_sort (keyids, (GCompareFunc)g_ascii_strcasecmp);
    for (l = keyids; l; l = g_slist_next (l)) {
        while (l->next && l->data && l->next->data && 
               g_ascii_strcasecmp (l->data, l->next->data) == 0)
            keyids = g_slist_delete_link (keyids, l->next);
    }    
    
    return keyids;
}

/* -----------------------------------------------------------------------------
 * NAMES PAGE (PRIVATE KEYS)
 */

enum {
    UIDSIG_INDEX,
    UIDSIG_ICON,
    UIDSIG_NAME,
    UIDSIG_KEYID,
    UIDSIG_N_COLUMNS
};

const GType uidsig_columns[] = {
    G_TYPE_INT,     /* index */
    G_TYPE_STRING,  /* icon */
    G_TYPE_STRING,  /* name */
    G_TYPE_STRING   /* keyid */
};

static gint
names_get_selected_uid (SeahorseWidget *swidget)
{
    return get_selected_row (swidget, "names-tree", UIDSIG_INDEX);
}

static void
names_add_clicked (GtkWidget *widget, SeahorseWidget *swidget)
{
    SeahorseKey *skey;
    
    skey = SEAHORSE_KEY_WIDGET (swidget)->skey;
    seahorse_add_uid_new (SEAHORSE_PGP_KEY (skey), 
                          GTK_WINDOW (glade_xml_get_widget (swidget->xml, swidget->name)));
}

static void 
names_primary_clicked (GtkWidget *widget, SeahorseWidget *swidget)
{
    SeahorseKey *skey;
    gpgme_error_t err;
    gint index;
    
    skey = SEAHORSE_KEY_WIDGET (swidget)->skey;
    index = names_get_selected_uid (swidget);
    
    if (index > 0) {
        err = seahorse_pgp_key_op_primary_uid (SEAHORSE_PGP_KEY (skey), index);
        if (!GPG_IS_OK (err)) 
            seahorse_util_handle_gpgme (err, _("Couldn't change primary user ID"));
    }
}

static void 
names_delete_clicked (GtkWidget *widget, SeahorseWidget *swidget)
{
    SeahorseKey *skey = SEAHORSE_KEY_WIDGET (swidget)->skey;
    gint index = names_get_selected_uid (swidget);
    
    if (index >= 1) 
        seahorse_delete_userid_show (skey, index);
}

static void 
names_sign_clicked (GtkWidget *widget, SeahorseWidget *swidget)
{
    SeahorseKey *skey;
    gint index;
    GList *keys = NULL;

    skey = SEAHORSE_KEY_WIDGET (swidget)->skey;
    index = names_get_selected_uid (swidget);

    if (index >= 1) {
        seahorse_sign_show (SEAHORSE_PGP_KEY (skey), 
                            GTK_WINDOW (glade_xml_get_widget (swidget->xml, swidget->name)),
                            index - 1);

#ifdef WITH_KEYSERVER
        if (seahorse_gconf_get_boolean(AUTOSYNC_KEY) == TRUE) {
            keys = g_list_append (keys, skey);
            seahorse_keyserver_sync (keys);
            g_list_free(keys);
        }
#endif
    }
}

static void 
names_revoke_clicked (GtkWidget *widget, SeahorseWidget *swidget)
{
    /* TODO: */
/*    SeahorseKey *skey;
    gint index;
    Glist *keys = NULL;

    skey = SEAHORSE_KEY_WIDGET (swidget)->skey;
    index = names_get_selected_uid (swidget);

    if (index >= 1) {
        seahorse_revoke_show (SEAHORSE_PGP_KEY (skey), index - 1);

#ifdef WITH_KEYSERVER
        if (seahorse_gconf_get_boolean(AUTOSYNC_KEY) == TRUE) {
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
    gint index = names_get_selected_uid (swidget);

    sensitive_glade_widget (swidget, "names-primary-button", index >= 1);
    sensitive_glade_widget (swidget, "names-delete-button", index >= 1);
    sensitive_glade_widget (swidget, "names-sign-button", index >= 1);
    show_glade_widget (swidget, "names-revoke-button", FALSE);
}

/* Is called whenever a signature key changes, to update row */
static void
names_update_row (SeahorseKeyModel *skmodel, SeahorseKey *skey, 
                  GtkTreeIter *iter, SeahorseWidget *swidget)
{
    const gchar *icon;
    gchar *name;
    
    /* Always use the most preferred key for this keyid */
    if (skey->preferred) {
        while (skey->preferred)
            skey = skey->preferred;
        seahorse_key_model_set_row_key (skmodel, iter, skey);
    }

    icon = seahorse_key_get_location (skey) < SKEY_LOC_LOCAL ? 
                GTK_STOCK_DIALOG_QUESTION : SEAHORSE_STOCK_SIGN;
    name = seahorse_key_get_name_markup (skey, 0);
    
    gtk_tree_store_set (GTK_TREE_STORE (skmodel), iter,
                        UIDSIG_INDEX, -1,
                        UIDSIG_ICON, icon,
                        UIDSIG_NAME, name ? name : _("[Unknown]"),
                        UIDSIG_KEYID, seahorse_key_get_short_keyid (skey), -1);
            
    g_free (name);
}

static void
names_populate (SeahorseWidget *swidget, GtkTreeStore *store, SeahorsePGPKey *pkey)
{
    SeahorseKey *skey;
    gpgme_user_id_t uid;
    gpgme_key_sig_t sig;
    GtkTreeIter uiditer, sigiter;
    gchar *name;
    const gchar *keyid;
    GSList *rawids = NULL;
    int i, j;
    GList *keys, *l;

    keyid = seahorse_pgp_key_get_id (pkey->pubkey, 0);
    
    /* Insert all the fun-ness */
    for (i = 1, uid = pkey->pubkey->uids; uid; uid = uid->next, i++) {

        name = seahorse_key_get_name_markup (SEAHORSE_KEY (pkey), i - 1);

        gtk_tree_store_append (store, &uiditer, NULL);
        gtk_tree_store_set (store, &uiditer,  
                            UIDSIG_INDEX, i,
                            UIDSIG_ICON, SEAHORSE_STOCK_PERSON,
                            UIDSIG_NAME, name,
                            -1);
        
        g_free (name);
        
        
        /* Build a list of all the keyids */
        for (j = 1, sig = uid->signatures; sig; sig = sig->next, j++) {
            /* Never show self signatures, they're implied */
            if (strcmp (sig->keyid, keyid) == 0)
                continue;
            rawids = g_slist_prepend (rawids, sig->keyid);
        }
        
        /* Pass it to 'DiscoverKeys' for resolution/download */
        keys = seahorse_context_discover_keys (SCTX_APP (), SKEY_PGP, rawids);
        g_slist_free (rawids);
        rawids = NULL;
        
        /* Add the keys to the store */
        for (l = keys; l; l = g_list_next (l)) {
            skey = SEAHORSE_KEY (l->data);
            gtk_tree_store_append (store, &sigiter, &uiditer);
            
            /* This calls the 'update-row' callback, to set the values for the key */
            seahorse_key_model_set_row_key (SEAHORSE_KEY_MODEL (store), &sigiter, skey);
        }
    } 
}

static void
do_names (SeahorseWidget *swidget)
{
    SeahorseKey *skey;
    SeahorsePGPKey *pkey;
    GtkWidget *widget;
    GtkTreeStore *store;
    GtkCellRenderer *renderer;
    
    skey = SEAHORSE_KEY_WIDGET (swidget)->skey;
    pkey = SEAHORSE_PGP_KEY (skey);
    
    if (seahorse_key_get_etype (skey) != SKEY_PRIVATE)
        return;

    /* Clear/create table store */
    widget = seahorse_widget_get_widget (swidget, "names-tree");
    g_return_if_fail (widget != NULL);
    
    store = GTK_TREE_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (widget)));
    if (store) {
        gtk_tree_store_clear (store);
        
    } else {
        
        /* This is our first time so create a store */
        store = GTK_TREE_STORE (seahorse_key_model_new (UIDSIG_N_COLUMNS, (GType*)uidsig_columns));
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
    SeahorseKey *skey;
    GtkTreeSelection *selection;
    GtkWidget *widget;
    
    skey = SEAHORSE_KEY_WIDGET (swidget)->skey;
    
    if (seahorse_key_get_etype (skey) != SKEY_PRIVATE)
        return;

    glade_xml_signal_connect_data (swidget->xml, "on_names_add",
            G_CALLBACK (names_add_clicked), swidget);
    glade_xml_signal_connect_data (swidget->xml, "on_names_primary",
            G_CALLBACK (names_primary_clicked), swidget);
    glade_xml_signal_connect_data (swidget->xml, "on_names_sign",
            G_CALLBACK (names_sign_clicked), swidget);
    glade_xml_signal_connect_data (swidget->xml, "on_names_delete",
            G_CALLBACK (names_delete_clicked), swidget);
    glade_xml_signal_connect_data (swidget->xml, "on_names_revoke",
            G_CALLBACK (names_revoke_clicked), swidget);
    glade_xml_signal_connect_data (swidget->xml, "on_names_tree_row_activated",
                                   G_CALLBACK (signature_row_activated), swidget);
    
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

static void
owner_photo_drag_received (GtkWidget *widget, GdkDragContext *context, gint x, gint y,
                           GtkSelectionData *sel_data, guint target_type, guint time,
                           SeahorseWidget *swidget)
{       
    gboolean dnd_success = FALSE;
    SeahorsePGPKey *pkey;
    gchar **uri_list;
    gint len = 0;
    gchar *uri;
    
    pkey = SEAHORSE_PGP_KEY (SEAHORSE_KEY_WIDGET (swidget)->skey);
    g_assert (SEAHORSE_IS_PGP_KEY (pkey)); 
    
    /* 
     * This needs to be improved, support should be added for remote images 
     * and there has to be a better way to get rid of the trailing \r\n appended
     * to the end of the path after the call to g_filename_from_uri
     */
    if((sel_data != NULL) && (sel_data->length >= 0)) {
        g_return_if_fail (target_type == TARGET_URI);
        
        uri_list = gtk_selection_data_get_uris (sel_data);
        while (uri_list && uri_list[len]) {
                
            uri = g_filename_from_uri (uri_list[len], NULL, NULL);
            if (!uri)
                continue;
                
            dnd_success = seahorse_photo_add (pkey, GTK_WINDOW (seahorse_widget_get_top (swidget)),
                                              uri);
            g_free (uri);
            
            if (!dnd_success)
                break;
            len++;
        }
        
        g_strfreev (uri_list);
    }
    
    gtk_drag_finish (context, dnd_success, FALSE, time);
}

static void
owner_photo_add_button_clicked (GtkWidget *widget, SeahorseWidget *swidget)
{
    SeahorsePGPKey *pkey;

    pkey = SEAHORSE_PGP_KEY (SEAHORSE_KEY_WIDGET (swidget)->skey);
    g_assert (SEAHORSE_IS_PGP_KEY (pkey));
    
    if (seahorse_photo_add (pkey, GTK_WINDOW (seahorse_widget_get_top (swidget)), NULL))
        g_object_set_data (G_OBJECT (swidget), "current-photoid", NULL);
}
 
static void
owner_photo_delete_button_clicked (GtkWidget *widget, SeahorseWidget *swidget)
{
    SeahorsePGPKey *pkey;
    gpgmex_photo_id_t photoid;

    photoid = (gpgmex_photo_id_t)g_object_get_data (G_OBJECT (swidget), 
                                                    "current-photoid");
    g_return_if_fail (photoid != NULL);

    pkey = SEAHORSE_PGP_KEY (SEAHORSE_KEY_WIDGET (swidget)->skey);
    g_assert (SEAHORSE_IS_PGP_KEY (pkey));
    
    if (seahorse_photo_delete (pkey, GTK_WINDOW (seahorse_widget_get_top (swidget)),
                               photoid))
        g_object_set_data (G_OBJECT (swidget), "current-photoid", NULL);
}

static void
owner_photo_primary_button_clicked (GtkWidget *widget, SeahorseWidget *swidget)
{
    SeahorseKey *skey;
    SeahorsePGPKey *pkey;
    gpgme_error_t gerr;
    gint uid;
    gpgmex_photo_id_t photoid;

    photoid = (gpgmex_photo_id_t)g_object_get_data (G_OBJECT (swidget), 
                                                    "current-photoid");
    g_return_if_fail (photoid != NULL);
        
    skey = SEAHORSE_KEY_WIDGET (swidget)->skey;
    pkey = SEAHORSE_PGP_KEY (skey);
    
    uid = photoid->uid;
    
    gerr = seahorse_pgp_key_op_photoid_primary (pkey, uid);
    if (!GPG_IS_OK (gerr))
        seahorse_util_handle_gpgme (gerr, _("Couldn't change primary photo"));
}

static void
set_photoid_state(SeahorseWidget *swidget, SeahorsePGPKey *pkey)
{
    SeahorseKeyEType etype; 
    GtkWidget *photo_image;

    gpgmex_photo_id_t photoid;

    etype = seahorse_key_get_etype (SEAHORSE_KEY(pkey));
    photoid = (gpgmex_photo_id_t)g_object_get_data (G_OBJECT (swidget), 
                                                    "current-photoid");

    /* Show when adding a photo is possible */
    show_glade_widget (swidget, "owner-photo-add-button", 
                       etype == SKEY_PRIVATE);

    /* Show when we have a photo to set as primary */
    show_glade_widget (swidget, "owner-photo-primary-button", 
                       etype == SKEY_PRIVATE && photoid);

    /* Display this when there are any photo ids */
    show_glade_widget (swidget, "owner-photo-delete-button", 
                       etype == SKEY_PRIVATE && photoid != NULL);

    /* Sensitive when not the first photo id */
    sensitive_glade_widget (swidget, "owner-photo-previous-button", 
                            photoid && photoid != pkey->photoids);
    
    /* Sensitive when not the last photo id */
    sensitive_glade_widget (swidget, "owner-photo-next-button", 
                            photoid && photoid->next && photoid->next->photo);
    
    /* Display *both* of these when there are more than one photo id */
    show_glade_widget (swidget, "owner-photo-previous-button", 
                       pkey->photoids && pkey->photoids->next);
                       
    show_glade_widget (swidget, "owner-photo-next-button", 
                       pkey->photoids && pkey->photoids->next);
                       
    photo_image = glade_xml_get_widget (swidget->xml, "photoid");
    if (photo_image) {
        if (!photoid || !photoid->photo)
            gtk_image_set_from_stock (GTK_IMAGE (photo_image), 
                etype == SKEY_PRIVATE ? SEAHORSE_STOCK_SECRET : SEAHORSE_STOCK_KEY, (GtkIconSize)-1);
        else 
            gtk_image_set_from_pixbuf (GTK_IMAGE (photo_image), photoid->photo);
    }
}

static void
do_photo_id (SeahorseWidget *swidget)
{
    SeahorseKey *skey;
    SeahorsePGPKey *pkey;

    skey = SEAHORSE_KEY_WIDGET (swidget)->skey;
    pkey = SEAHORSE_PGP_KEY (skey);

    g_object_set_data (G_OBJECT (swidget), "current-photoid", pkey->photoids);
    
    set_photoid_state (swidget, pkey);
}

static void
photoid_next_clicked (GtkWidget *widget, SeahorseWidget *swidget)
{
    SeahorseKey *skey;
    SeahorsePGPKey *pkey;
    gpgmex_photo_id_t photoid;

    skey = SEAHORSE_KEY_WIDGET (swidget)->skey;
    pkey = SEAHORSE_PGP_KEY (skey);

    photoid = (gpgmex_photo_id_t)g_object_get_data (G_OBJECT (swidget), "current-photoid");

    if (photoid && photoid->next)
        g_object_set_data (G_OBJECT (swidget), "current-photoid", photoid->next);
        
    set_photoid_state (swidget, pkey);
}

static void
photoid_prev_clicked (GtkWidget *widget, SeahorseWidget *swidget)
{
    SeahorseKey *skey;
    SeahorsePGPKey *pkey;
    gpgmex_photo_id_t itter, photoid;

    skey = SEAHORSE_KEY_WIDGET (swidget)->skey;
    pkey = SEAHORSE_PGP_KEY (skey);

    photoid = (gpgmex_photo_id_t)g_object_get_data (G_OBJECT (swidget), "current-photoid");

    if (photoid != pkey->photoids) {
        itter = pkey->photoids;
    
        while (itter->next != photoid && itter->next)
           itter = itter->next;
    
        g_object_set_data (G_OBJECT (swidget), "current-photoid", itter);
    }

    set_photoid_state (swidget, pkey);
}

static void
photoid_button_pressed(GtkWidget *widget, GdkEvent *event, SeahorseWidget *swidget)
{
    GdkEventScroll *event_scroll;
    
    if(event->type == GDK_SCROLL){
        event_scroll = (GdkEventScroll *) event;
        
        if(event_scroll->direction == GDK_SCROLL_UP)
            photoid_prev_clicked (widget, swidget);
        else if(event_scroll->direction == GDK_SCROLL_DOWN)
            photoid_next_clicked(widget, swidget);
    }
}

/* -----------------------------------------------------------------------------
 * OWNER PAGE
 */

/* owner uid list */
enum {
    UID_INDEX,
    UID_ICON,
    UID_MARKUP,
    UID_N_COLUMNS
};

const GType uid_columns[] = {
    G_TYPE_INT,     /* index */
    G_TYPE_STRING,  /* icon */
    G_TYPE_STRING,  /* name */
    G_TYPE_STRING,  /* email */
    G_TYPE_STRING   /* comment */
};

static gint
owner_get_selected_uid (SeahorseWidget *swidget)
{
    return get_selected_row (swidget, "owner-userid-tree", UID_INDEX);
}

static void 
owner_sign_button_clicked (GtkWidget *widget, SeahorseWidget *swidget)
{
    SeahorseKey *skey;
    gint index;

    skey = SEAHORSE_KEY_WIDGET (swidget)->skey;
    index = owner_get_selected_uid (swidget);

    if (index >= 1) 
        seahorse_sign_show (SEAHORSE_PGP_KEY (skey),
                            GTK_WINDOW (glade_xml_get_widget (swidget->xml, swidget->name)), 
                            index - 1);
}

static void
owner_passphrase_button_clicked (GtkWidget *widget, SeahorseWidget *swidget)
{
    SeahorseKey *skey;
    skey = SEAHORSE_KEY_WIDGET (swidget)->skey;
    
    if (seahorse_key_get_etype (skey) == SKEY_PRIVATE)
        seahorse_pgp_key_pair_op_change_pass (SEAHORSE_PGP_KEY (skey));
}

static void
do_owner_signals (SeahorseWidget *swidget)
{ 
    SeahorseKey *skey;
    SeahorseKeyEType etype;
    GtkWidget *frame;
    
    skey = SEAHORSE_KEY_WIDGET (swidget)->skey;
    etype = seahorse_key_get_etype (skey);
    
    glade_xml_signal_connect_data (swidget->xml, "on_owner_sign_button_clicked",
                                G_CALLBACK (owner_sign_button_clicked), swidget);
    glade_xml_signal_connect_data (swidget->xml, "on_owner_photo_next_clicked",
                                G_CALLBACK (photoid_next_clicked), swidget);
    glade_xml_signal_connect_data (swidget->xml, "on_owner_photo_previous_clicked",
                                G_CALLBACK (photoid_prev_clicked), swidget);
    glade_xml_signal_connect_data(swidget->xml, "on_image_eventbox_scroll_event", 
                                G_CALLBACK(photoid_button_pressed), swidget);
    
    if (etype == SKEY_PRIVATE ) {
        glade_xml_signal_connect_data (swidget->xml, "on_owner_photo_primary_button_clicked",
                G_CALLBACK (owner_photo_primary_button_clicked), swidget);
        glade_xml_signal_connect_data (swidget->xml, "on_owner_photo_add_button_clicked",
                G_CALLBACK (owner_photo_add_button_clicked), swidget);
        glade_xml_signal_connect_data (swidget->xml, "on_owner_photo_delete_button_clicked",
                G_CALLBACK (owner_photo_delete_button_clicked), swidget);
        glade_xml_signal_connect_data (swidget->xml, "on_passphrase_button",
                G_CALLBACK (owner_passphrase_button_clicked), swidget);
        glade_xml_signal_connect_data (swidget->xml, "on_owner_photo_drag_received",
                G_CALLBACK (owner_photo_drag_received), swidget);
        frame = glade_xml_get_widget (swidget->xml, "owner-photo-frame");
        gtk_drag_dest_set (frame, GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_HIGHLIGHT |
                           GTK_DEST_DEFAULT_DROP, target_list, n_targets, GDK_ACTION_COPY);
    } else {
        show_glade_widget (swidget, "owner-photo-add-button", FALSE);
        show_glade_widget (swidget, "owner-photo-delete-button", FALSE);
        show_glade_widget (swidget, "owner-photo-primary-button", FALSE);
        show_glade_widget (swidget, "passphrase-button", FALSE);
    }
}

static void
do_owner (SeahorseWidget *swidget)
{
    SeahorseKey *skey;
    SeahorsePGPKey *pkey;
    gpgme_user_id_t uid;
    GtkWidget *widget;
    GtkCellRenderer *renderer;
    GtkListStore *store;
    GtkTreeIter iter;
    gchar *text, *t;
    gulong expires_date;
    guint flags;
    gchar *markup;
    int i;

    skey = SEAHORSE_KEY_WIDGET (swidget)->skey;
    pkey = SEAHORSE_PGP_KEY (skey);

    flags = seahorse_key_get_flags (skey);
    
    /* Display appropriate warnings */    
    show_glade_widget (swidget, "expired-area", flags & SKEY_FLAG_EXPIRED);
    show_glade_widget (swidget, "revoked-area", flags & SKEY_FLAG_REVOKED);
    show_glade_widget (swidget, "disabled-area", flags & SKEY_FLAG_DISABLED);
    
    /* Update the expired message */
    if (flags & SKEY_FLAG_EXPIRED) {
        widget = glade_xml_get_widget (swidget->xml, "expired-message");
        if (widget) {
            
            expires_date = seahorse_key_get_expires (skey);
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
    show_glade_widget (swidget, "trust-page", !((flags & SKEY_FLAG_EXPIRED) ||
                       (flags & SKEY_FLAG_REVOKED) || (flags & SKEY_FLAG_DISABLED)));

    /* Hide or show the uids area */
    show_glade_widget (swidget, "uids-area", seahorse_pgp_key_get_num_userids (pkey) > 1);

    if ((text = seahorse_pgp_key_get_userid_name (pkey, 0)) != NULL) {
        widget = glade_xml_get_widget (swidget->xml, "owner-name-label");
        gtk_label_set_text (GTK_LABEL (widget), text);  

        widget = glade_xml_get_widget (swidget->xml, swidget->name);
        gtk_window_set_title (GTK_WINDOW (widget), text);
        g_free (text);
    }

    if ((text = seahorse_pgp_key_get_userid_email (pkey, 0)) != NULL) {
        widget = glade_xml_get_widget (swidget->xml, "owner-email-label");
        gtk_label_set_text (GTK_LABEL (widget), text);  
        g_free (text);
    }

    if ((text = seahorse_pgp_key_get_userid_comment (pkey, 0)) != NULL) {
        widget = glade_xml_get_widget (swidget->xml, "owner-comment-label");
        gtk_label_set_text (GTK_LABEL (widget), text);  
        g_free (text);
    }
    
    widget = glade_xml_get_widget (swidget->xml, "owner-keyid-label");
    if (widget)
        gtk_label_set_text (GTK_LABEL (widget), seahorse_key_get_short_keyid (skey)); 
    
    /* Clear/create table store */
    widget = glade_xml_get_widget (swidget->xml, "owner-userid-tree");
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
    
        for (i = 1, uid = pkey->pubkey->uids; uid; uid = uid->next, i++) {
    
            markup = seahorse_key_get_name_markup (SEAHORSE_KEY (pkey), i - 1);
    
            gtk_list_store_append (store, &iter);
            gtk_list_store_set (store, &iter,  
                                UID_INDEX, i,
                                UID_ICON, SEAHORSE_STOCK_PERSON,
                                UID_MARKUP, markup, -1);
            
            g_free (markup);
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
	SUBKEY_INDEX,
	SUBKEY_ID,
	SUBKEY_TYPE,
	SUBKEY_CREATED,
	SUBKEY_EXPIRES,
	SUBKEY_STATUS,
	SUBKEY_LENGTH,
	SUBKEY_N_COLUMNS
};

const GType subkey_columns[] = {
    G_TYPE_INT,     /* index */
    G_TYPE_STRING,  /* id */
    G_TYPE_STRING,  /* created */
    G_TYPE_STRING,  /* expires */
    G_TYPE_STRING,  /* status  */
    G_TYPE_STRING,  /* type */
    G_TYPE_STRING   /* length*/
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

static gint 
get_selected_subkey (SeahorseWidget *swidget)
{
    return get_selected_row (swidget, "details-subkey-tree", SUBKEY_INDEX);
}

static void
details_add_subkey_button_clicked (GtkButton *button, SeahorseWidget *swidget)
{
	SeahorseKey *skey;
	SeahorsePGPKey *pkey;
	
	skey = SEAHORSE_KEY_WIDGET (swidget)->skey;
	pkey = SEAHORSE_PGP_KEY (skey);
	
	seahorse_add_subkey_new (pkey, 
	                         GTK_WINDOW (glade_xml_get_widget (swidget->xml, swidget->name)));
}

static void
details_del_subkey_button_clicked (GtkButton *button, SeahorseWidget *swidget)
{
	SeahorseKey *skey = SEAHORSE_KEY_WIDGET (swidget)->skey;

	seahorse_delete_subkey_new (SEAHORSE_PGP_KEY (skey), get_selected_subkey (swidget));
}

static void
details_revoke_subkey_button_clicked (GtkButton *button, SeahorseWidget *swidget)
{
	SeahorseKey *skey = SEAHORSE_KEY_WIDGET (swidget)->skey;

	seahorse_revoke_new (SEAHORSE_PGP_KEY (skey), 
	                     GTK_WINDOW (glade_xml_get_widget (swidget->xml, swidget->name)), 
	                     get_selected_subkey (swidget));
}

static void
trust_changed (GtkComboBox *selection, SeahorseWidget *swidget)
{
	SeahorseKey *skey;
	gint trust;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gpgme_error_t err;
	gboolean set;
	
    set = gtk_combo_box_get_active_iter (selection, &iter);
	
    if (set) {
    	skey = SEAHORSE_KEY_WIDGET (swidget)->skey;
    	
    	model = gtk_combo_box_get_model (selection);
    	gtk_tree_model_get (model, &iter,
                            TRUST_VALIDITY, &trust,
                            -1);
                                  
    	if (seahorse_key_get_trust (skey) != trust) {
    		err = seahorse_pgp_key_op_set_trust (SEAHORSE_PGP_KEY (skey), trust);
    		if (err) {
    			seahorse_util_handle_gpgme (err, _("Unable to change trust"));
    		}
    	}
    }
}

static void
details_export_button_clicked (GtkWidget *widget, SeahorseWidget *swidget)
{
    SeahorseKeySource *sksrc;
    SeahorseOperation *op;
    SeahorseKey *skey;
    GtkWidget *dialog;
    gchar* uri = NULL;
    GError *err = NULL;
    gpgme_data_t data;
    GList *keys = NULL;
    
    skey = SEAHORSE_KEY_WIDGET (swidget)->skey;
    keys = g_list_prepend (NULL, skey);
    
    dialog = seahorse_util_chooser_save_new (_("Export Complete Key"), 
                                             GTK_WINDOW (seahorse_widget_get_top (swidget)));
    seahorse_util_chooser_show_key_files (dialog);
    seahorse_util_chooser_set_filename (dialog, keys);
    
    uri = seahorse_util_chooser_save_prompt (dialog);
    if(!uri) 
        return;
    
    sksrc = seahorse_key_get_source (skey);
    g_assert (SEAHORSE_IS_KEY_SOURCE (sksrc));
    
    data = seahorse_vfs_data_create (uri, SEAHORSE_VFS_WRITE, &err);
    
    if (data) {
        op = seahorse_key_source_export (sksrc, keys, TRUE, data);
    
        seahorse_operation_wait (op);
        gpgmex_data_release (data);
        
        if (!seahorse_operation_is_successful (op))
            seahorse_operation_copy_error (op, &err);
    }
    
    if (err)
        seahorse_util_handle_error (err, _("Couldn't export key to \"%s\""),
                                    seahorse_util_uri_get_last (uri));
    
    g_list_free (keys);
    g_free (uri);
}

/*
 * This function is called by 2 different buttons. There may not
 * be a selected subkey which will cause an index of -1
 */

static void
details_calendar_button_clicked (GtkWidget *widget, SeahorseWidget *swidget)
{
	SeahorseKey *skey;
	gint index;
	
	g_return_if_fail (SEAHORSE_IS_KEY_WIDGET (swidget));
	skey = SEAHORSE_KEY_WIDGET (swidget)->skey;

	index = get_selected_subkey (swidget);
	
	if (-1 == index) {
		index = 0;
	}
	seahorse_expires_new (SEAHORSE_PGP_KEY (skey), 
	                      GTK_WINDOW (glade_xml_get_widget (swidget->xml, swidget->name)), 
	                      index);	
}

static void
setup_details_trust (SeahorseWidget *swidget)
{
    SeahorseKey *skey;
    SeahorseKeyEType etype;
    GtkWidget *widget;
    GtkListStore *model;
    GtkTreeIter iter;
    GtkCellRenderer *text_cell = gtk_cell_renderer_text_new ();

    DBG_PRINT(("KeyProperties: Setting up Trust Combo Box Store\n"));
    
    skey = SEAHORSE_KEY_WIDGET (swidget)->skey;
    etype = seahorse_key_get_etype (skey);
    
    widget = glade_xml_get_widget (swidget->xml, "details-trust-combobox");
    
    gtk_cell_layout_clear(GTK_CELL_LAYOUT (widget));
    
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT (widget), text_cell, FALSE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT (widget), text_cell,
                                   "text", TRUST_LABEL,
                                   NULL);
    
    /* Initialize the store */
    model = gtk_list_store_newv (TRUST_N_COLUMNS, (GType*)trust_columns);
    
    if (etype != SKEY_PRIVATE) {
        gtk_list_store_append (model, &iter);
        gtk_list_store_set (model, &iter,
                            TRUST_LABEL, _("Unknown"),
                            TRUST_VALIDITY,  SEAHORSE_VALIDITY_UNKNOWN,
                            -1);                               
       
        gtk_list_store_append (model, &iter);
        gtk_list_store_set (model, &iter,
                            TRUST_LABEL, _("Never"),
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
    if (etype == SKEY_PRIVATE) {
        gtk_list_store_append (model, &iter);
        gtk_list_store_set (model, &iter,
                            TRUST_LABEL, _("Ultimate"),
                            TRUST_VALIDITY,  SEAHORSE_VALIDITY_ULTIMATE,
                            -1);
    }
    
    gtk_combo_box_set_model (GTK_COMBO_BOX (widget), GTK_TREE_MODEL (model));                                        

    DBG_PRINT(("KeyProperties: Finished Setting up Trust Combo Box Store\n"));
}

static void
do_details_signals (SeahorseWidget *swidget) 
{ 
    SeahorseKey *skey;
    SeahorseKeyEType etype;
    
    skey = SEAHORSE_KEY_WIDGET (swidget)->skey;
    etype = seahorse_key_get_etype (skey);
    
    /* 
     * if not the key owner, disable most everything
     * if key owner, add the callbacks to the subkey buttons
     */
     if (etype == SKEY_PUBLIC ) {
         show_glade_widget (swidget, "details-actions-label", FALSE);
         show_glade_widget (swidget, "details-export-button", FALSE);
         show_glade_widget (swidget, "details-add-button", FALSE);
         show_glade_widget (swidget, "details-date-button", FALSE);
         show_glade_widget (swidget, "details-revoke-button", FALSE);
         show_glade_widget (swidget, "details-delete-button", FALSE);
         show_glade_widget (swidget, "details-calendar-button", FALSE);
    } else {

        glade_xml_signal_connect_data (swidget->xml, "on_details_add_button_clicked", 
                                        G_CALLBACK (details_add_subkey_button_clicked), swidget);
        glade_xml_signal_connect_data (swidget->xml, "on_details_calendar1_button_clicked",
                                        G_CALLBACK (details_calendar_button_clicked), swidget);
        glade_xml_signal_connect_data (swidget->xml, "on_details_calendar2_button_clicked",
                                        G_CALLBACK (details_calendar_button_clicked), swidget);
        glade_xml_signal_connect_data (swidget->xml, "on_details_revoke_button_clicked",
                                        G_CALLBACK (details_revoke_subkey_button_clicked), swidget);
        glade_xml_signal_connect_data (swidget->xml, "on_details_delete_button_clicked",
                                        G_CALLBACK (details_del_subkey_button_clicked), swidget);
        glade_xml_signal_connect_data (swidget->xml, "on_details_export_button_clicked",
                                        G_CALLBACK (details_export_button_clicked), swidget);
    }
    glade_xml_signal_connect_data (swidget->xml, "on_details_trust_changed",
                                   G_CALLBACK (trust_changed), swidget);
}

static void 
do_details (SeahorseWidget *swidget)
{
    SeahorseKey *skey;
    SeahorsePGPKey *pkey;
    gpgme_subkey_t subkey;
    GtkWidget *widget;
    GtkListStore *store;
    GtkTreeModel *model;
    GtkTreeIter iter;
    GtkTreeIter default_key;
    GtkTreeSelection *selection;
    gchar dbuffer[G_ASCII_DTOSTR_BUF_SIZE];
    gchar *fp_label, *expiration_date, *created_date;
    const gchar *label, *status, *length;
    gint subkey_number, key, trust;
    guint keyloc;
    gboolean valid;

    skey = SEAHORSE_KEY_WIDGET (swidget)->skey;
    pkey = SEAHORSE_PGP_KEY (skey);
    subkey = pkey->pubkey->subkeys;
    keyloc = seahorse_key_get_location (skey);

    widget = glade_xml_get_widget (swidget->xml, "details-id-label");
    if (widget) {
        label = seahorse_key_get_short_keyid (skey); 
        gtk_label_set_text (GTK_LABEL (widget), label);
    }

    widget = glade_xml_get_widget (swidget->xml, "details-fingerprint-label");
    if (widget) {
        fp_label = seahorse_key_get_fingerprint (skey);  
        if (strlen (fp_label) > 24)
            fp_label[24] = '\n';
        gtk_label_set_text (GTK_LABEL (widget), fp_label);
        g_free (fp_label);
    }

    widget = glade_xml_get_widget (swidget->xml, "details-algo-label");
    if (widget) {
        label = seahorse_pgp_key_get_algo (pkey, 0);
        gtk_label_set_text (GTK_LABEL (widget), label);
    }

    widget = glade_xml_get_widget (swidget->xml, "details-created-label");
    if (widget) {
        fp_label = seahorse_util_get_display_date_string (subkey->timestamp); 
        gtk_label_set_text (GTK_LABEL (widget), fp_label);
        g_free (fp_label);
    }

    widget = glade_xml_get_widget (swidget->xml, "details-strength-label");
    if (widget) {
        g_ascii_dtostr (dbuffer, G_ASCII_DTOSTR_BUF_SIZE, subkey->length);
        gtk_label_set_text (GTK_LABEL (widget), dbuffer);
    }

    widget = glade_xml_get_widget (swidget->xml, "details-expires-label");
    if (widget) {
        if (seahorse_key_get_location (skey) == SKEY_LOC_REMOTE)
            fp_label = NULL;
        else if (subkey->expires == 0)
            fp_label = g_strdup (_("Never"));
        else
            fp_label = seahorse_util_get_display_date_string (subkey->expires);
        gtk_label_set_text (GTK_LABEL (widget), fp_label);
        g_free (fp_label);
    }

    show_glade_widget (swidget, "details-trust-combobox", keyloc == SKEY_LOC_LOCAL);
    widget = glade_xml_get_widget (swidget->xml, "details-trust-combobox");
    
    if (widget) {
        gtk_widget_set_sensitive (widget, !(pkey->pubkey->disabled));
        model = gtk_combo_box_get_model (GTK_COMBO_BOX (widget));
        
        valid = gtk_tree_model_get_iter_first (model, &iter);
       
        while (valid) {
            gtk_tree_model_get (model, &iter,
                                TRUST_VALIDITY, &trust,
                                -1);
            
            if (trust == seahorse_key_get_trust (skey)) {
                gtk_combo_box_set_active_iter (GTK_COMBO_BOX (widget),  &iter);
                break;
            }
            
            valid = gtk_tree_model_iter_next (model, &iter);             
        }
    }

    /* Clear/create table store */
    widget = glade_xml_get_widget (swidget->xml, "details-subkey-tree");
    store = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (widget)));

    if (store) {
        gtk_list_store_clear (store);
        
    } else {
        
        /* This is our first time so create a store */
        store = gtk_list_store_newv (SUBKEY_N_COLUMNS, (GType*)subkey_columns);

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

    subkey_number = seahorse_pgp_key_get_num_subkeys (pkey);
    for (key = 0; key < subkey_number; key++) {

        subkey = seahorse_pgp_key_get_nth_subkey (pkey, key);

        status = subkey->revoked ? _("Revoked") : _("Good");
        status = subkey->expired ? _("Expired") : _("Good");

        if (subkey->expires == 0)
            expiration_date = g_strdup (_("Never"));
        else
            expiration_date = seahorse_util_get_display_date_string (subkey->expires);

        created_date = seahorse_util_get_display_date_string (subkey->timestamp);
        
        length = g_ascii_dtostr(dbuffer, G_ASCII_DTOSTR_BUF_SIZE, subkey->length);

        gtk_list_store_append (store, &iter);
        gtk_list_store_set (store, &iter,  
                            SUBKEY_INDEX, key,
                            SUBKEY_ID, seahorse_pgp_key_get_id (pkey->pubkey, key),
                            SUBKEY_TYPE, seahorse_pgp_key_get_algo (pkey, key),
                            SUBKEY_CREATED, created_date,
                            SUBKEY_EXPIRES, expiration_date,
                            SUBKEY_STATUS,  status, 
                            SUBKEY_LENGTH, length,
                            -1);
        
        g_free (expiration_date);
        g_free (created_date);

        if (!key) 
            default_key = iter;
    } 
    
    gtk_tree_view_set_model (GTK_TREE_VIEW (widget), GTK_TREE_MODEL(store));
    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));
    gtk_tree_selection_select_iter (selection, &default_key);
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

static void
signatures_delete_button_clicked (GtkWidget *widget, SeahorseWidget *skey) 
{
    /* TODO: */
}

static void
signatures_revoke_button_clicked (GtkWidget *widget, SeahorseWidget *skey)
{
    /* TODO: */
}

static void
trust_marginal_toggled (GtkToggleButton *toggle, SeahorseWidget *swidget)
{
    SeahorseKey *skey;
    guint trust;
    gpgme_error_t err;
    
    skey = SEAHORSE_KEY_WIDGET (swidget)->skey;
    
    trust = gtk_toggle_button_get_active (toggle) ?
            SEAHORSE_VALIDITY_MARGINAL : SEAHORSE_VALIDITY_UNKNOWN;
    
    if (seahorse_key_get_trust (skey) != trust) {
        err = seahorse_pgp_key_op_set_trust (SEAHORSE_PGP_KEY (skey), trust);
        if (err)
            seahorse_util_handle_gpgme (err, _("Unable to change trust"));
    }
}

static void
trust_complete_toggled (GtkToggleButton *toggle, SeahorseWidget *swidget)
{
    SeahorseKey *skey;
    guint trust;
    gpgme_error_t err;
    
    skey = SEAHORSE_KEY_WIDGET (swidget)->skey;
    
    trust = gtk_toggle_button_get_active (toggle) ?
            SEAHORSE_VALIDITY_FULL : SEAHORSE_VALIDITY_MARGINAL;
    
    if (seahorse_key_get_trust (skey) != trust) {
        err = seahorse_pgp_key_op_set_trust (SEAHORSE_PGP_KEY (skey), trust);
        if (err)
            seahorse_util_handle_gpgme (err, _("Unable to change trust"));
    }
}

/* Is called whenever a signature key changes */
static void
trust_update_row (SeahorseKeyModel *skmodel, SeahorseKey *skey, 
                  GtkTreeIter *iter, SeahorseWidget *swidget)
{
    gboolean trusted = FALSE;
    const gchar *icon;
    gchar *name;    
    
    /* Always use the most preferred key for this keyid */
    if (skey->preferred) {
        while (skey->preferred)
            skey = skey->preferred;
        seahorse_key_model_set_row_key (skmodel, iter, skey);
    }

    if (seahorse_key_get_etype (skey) == SKEY_PRIVATE) 
        trusted = TRUE;
    else if (seahorse_key_get_flags (skey) & SKEY_FLAG_TRUSTED)
        trusted = TRUE;
    
    icon = seahorse_key_get_location (skey) < SKEY_LOC_LOCAL ? 
                GTK_STOCK_DIALOG_QUESTION : SEAHORSE_STOCK_SIGN;
    name = seahorse_key_get_display_name (skey);
    
    gtk_tree_store_set (GTK_TREE_STORE (skmodel), iter,
                        SIGN_ICON, icon,
                        SIGN_NAME, name ? name : _("[Unknown]"),
                        SIGN_KEYID, seahorse_key_get_short_keyid (skey),
                        SIGN_TRUSTED, trusted,
                        -1);
            
    g_free (name);
}

static void
signatures_populate_model (SeahorseWidget *swidget, SeahorseKeyModel *skmodel)
{
    SeahorseKey *skey;
    SeahorsePGPKey *pkey;
    GtkTreeIter iter;
    GtkWidget *widget;
    const gchar *keyid;
    gpgme_user_id_t uid;
    gpgme_key_sig_t sig;
    gboolean have_sigs = FALSE;
    GSList *rawids = NULL;
    GList *keys, *l;

    pkey = SEAHORSE_PGP_KEY (SEAHORSE_KEY_WIDGET (swidget)->skey);
    widget = glade_xml_get_widget (swidget->xml, "signatures-tree");
    
    keyid = seahorse_pgp_key_get_id (pkey->pubkey, 0);

    /* Build a list of all the keyids */        
    for (uid = pkey->pubkey->uids; uid; uid = uid->next) {
        for (sig = uid->signatures; sig; sig = sig->next) {
            /* Never show self signatures, they're implied */
            if (strcmp (sig->keyid, keyid) == 0)
                continue;
            have_sigs = TRUE;
            rawids = g_slist_prepend (rawids, sig->keyid);
        }
    }
    
    /* Only show signatures area when there are signatures */
    seahorse_widget_set_visible (swidget, "signatures-area", have_sigs);

    if (skmodel) {
    
        /* String out duplicates */
        rawids = unique_slist_strings (rawids);
        
        /* Pass it to 'DiscoverKeys' for resolution/download */
        keys = seahorse_context_discover_keys (SCTX_APP (), SKEY_PGP, rawids);
        g_slist_free (rawids);
        rawids = NULL;
        
        /* Add the keys to the store */
        for (l = keys; l; l = g_list_next (l)) {
            skey = SEAHORSE_KEY (l->data);
            gtk_tree_store_append (GTK_TREE_STORE (skmodel), &iter, NULL);
            
            /* This calls the 'update-row' callback, to set the values for the key */
            seahorse_key_model_set_row_key (SEAHORSE_KEY_MODEL (skmodel), &iter, skey);
        }
    }
}

/* Refilter when the user toggles the 'only show trusted' checkbox */
static void
trusted_toggled (GtkToggleButton *toggle, GtkTreeModelFilter *filter)
{
    /* Set flag on the store */
    GtkTreeModel *model = gtk_tree_model_filter_get_model (filter);
    g_object_set_data (G_OBJECT (model), "only-trusted", 
                GINT_TO_POINTER (gtk_toggle_button_get_active (toggle)));
    gtk_tree_model_filter_refilter (filter);
}

/* Add a signature */
static void 
trust_sign_clicked (GtkWidget *widget, SeahorseWidget *swidget)
{
    g_return_if_fail (SEAHORSE_IS_PGP_KEY (SEAHORSE_KEY_WIDGET (swidget)->skey));
    seahorse_sign_show (SEAHORSE_PGP_KEY (SEAHORSE_KEY_WIDGET (swidget)->skey), 
                        GTK_WINDOW (glade_xml_get_widget (swidget->xml, swidget->name)),
                        0);
}

static void
do_trust_signals (SeahorseWidget *swidget)
{
    SeahorseKey *skey;
    SeahorseKeyEType etype;
    gchar *user;
    
    skey = SEAHORSE_KEY_WIDGET (swidget)->skey;
    etype = seahorse_key_get_etype (skey);
    
    if (etype != SKEY_PUBLIC)
        return;
    
    set_glade_image (swidget, "image-good1", "seahorse-sign-ok");
    set_glade_image (swidget, "image-good2", "seahorse-sign-ok");
    
    glade_xml_signal_connect_data (swidget->xml, "trust_sign_clicked", 
                                   G_CALLBACK (trust_sign_clicked), swidget);
    /* TODO: Hookup revoke handler */
    
    if (etype == SKEY_PUBLIC ) {
        
        show_glade_widget (swidget, "signatures-revoke-button", FALSE);
        show_glade_widget (swidget, "signatures-delete-button", FALSE);
        show_glade_widget (swidget, "signatures-empty-label", FALSE);
        
        /* Fill in trust labels with name .This only happens once, so it sits here. */
        user = seahorse_pgp_key_get_userid_name (SEAHORSE_PGP_KEY (skey), 0);
        printf_glade_widget (swidget, "trust-marginal-check", user);
        printf_glade_widget (swidget, "trust-complete-check", user);
        g_free (user);
        
    } else {
        
        glade_xml_signal_connect_data (swidget->xml, "signatures_revoke_button_clicked",
                                       G_CALLBACK (signatures_revoke_button_clicked), swidget);
        glade_xml_signal_connect_data (swidget->xml, "signatures_delete_button_clicked",
                                       G_CALLBACK (signatures_delete_button_clicked), swidget);
        
    }

    glade_xml_signal_connect_data (swidget->xml, "trust_marginal_toggled",
                                   G_CALLBACK (trust_marginal_toggled), swidget);
    glade_xml_signal_connect_data (swidget->xml, "trust_complete_toggled",
                                   G_CALLBACK (trust_complete_toggled), swidget);
                                   
    glade_xml_signal_connect_data (swidget->xml, "on_signatures_tree_row_activated",
                                   G_CALLBACK (signature_row_activated), swidget);
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

static void 
do_trust (SeahorseWidget *swidget)
{
    SeahorseKey *skey;
    SeahorsePGPKey *pkey;
    GtkWidget *widget;
    GtkTreeStore *store;
    GtkTreeModelFilter *filter;
    gboolean sigpersonal;
    GtkCellRenderer *renderer;
    guint keyloc;
    
    skey = SEAHORSE_KEY_WIDGET (swidget)->skey;
    pkey = SEAHORSE_PGP_KEY (skey);
    keyloc = seahorse_key_get_location (skey);
    
    if (seahorse_key_get_etype (skey) != SKEY_PUBLIC)
        return;
    
    /* Remote keys */
    if (keyloc < SKEY_LOC_LOCAL) {
        
        show_glade_widget (swidget, "manual-trust-area", FALSE);
        show_glade_widget (swidget, "manage-trust-area", TRUE);
        show_glade_widget (swidget, "sign-area", FALSE);
        show_glade_widget (swidget, "revoke-area", FALSE);
        sensitive_glade_widget (swidget, "trust-marginal-check", FALSE);
        sensitive_glade_widget (swidget, "trust-complete-check", FALSE);
        set_glade_image (swidget, "sign-image", SEAHORSE_STOCK_SIGN_UNKNOWN);
        
    /* Local keys */
    } else {
        guint trust;
        gboolean trusted, managed;
        const gchar *icon = NULL;
        
        trust = seahorse_key_get_trust (skey);
    
        trusted = FALSE;
        managed = FALSE;
        
        switch (trust) {
    
        /* We shouldn't be seeing this page with these trusts */
        case SEAHORSE_VALIDITY_REVOKED:
        case SEAHORSE_VALIDITY_DISABLED:
            return;
        
        /* Trust is specified manually */
        case SEAHORSE_VALIDITY_ULTIMATE:
            trusted = TRUE;
            managed = FALSE;
            icon = SEAHORSE_STOCK_SIGN_OK;
            break;
        
        /* Trust is specified manually */
        case SEAHORSE_VALIDITY_NEVER:
            trusted = FALSE;
            managed = FALSE;
            icon = SEAHORSE_STOCK_SIGN_BAD;
            break;
        
        /* We manage the trust through this page */
        case SEAHORSE_VALIDITY_FULL:
        case SEAHORSE_VALIDITY_MARGINAL:
            trusted = TRUE;
            managed = TRUE;
            icon = SEAHORSE_STOCK_SIGN_OK;
            break;
        
        /* We manage the trust through this page */
        case SEAHORSE_VALIDITY_UNKNOWN:
            trusted = FALSE;
            managed = TRUE;
            icon = SEAHORSE_STOCK_SIGN;
            break;
        
        default:
            g_warning ("unknown trust value: %d", trust);
            g_assert_not_reached ();
            return;
        }
        
        
        /* Managed and unmanaged areas */
        show_glade_widget (swidget, "manual-trust-area", !managed);
        show_glade_widget (swidget, "manage-trust-area", managed);
    
        /* Managed check boxes */
        if (managed) {
            widget = seahorse_widget_get_widget (swidget, "trust-marginal-check");
            g_return_if_fail (widget != NULL);
            gtk_widget_set_sensitive (widget, TRUE);
            
            g_signal_handlers_block_by_func (widget, trust_marginal_toggled, swidget);
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), trust != SEAHORSE_VALIDITY_UNKNOWN);
            g_signal_handlers_unblock_by_func (widget, trust_marginal_toggled, swidget);
        
            widget = seahorse_widget_get_widget (swidget, "trust-complete-check");
            g_return_if_fail (widget != NULL);
            
            g_signal_handlers_block_by_func (widget, trust_complete_toggled, swidget);
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), trust == SEAHORSE_VALIDITY_FULL);
            gtk_widget_set_sensitive (widget, trust != SEAHORSE_VALIDITY_UNKNOWN);
            g_signal_handlers_unblock_by_func (widget, trust_complete_toggled, swidget);
        }
    
        /* Signing and revoking */
        sigpersonal = seahorse_pgp_key_have_signatures (pkey, SKEY_PGPSIG_PERSONAL);
        show_glade_widget (swidget, "sign-area", !sigpersonal && trusted);
        show_glade_widget (swidget, "revoke-area", sigpersonal);
        
        /* The image */
        set_glade_image (swidget, "sign-image", icon);
    }
    
    /* The actual signatures listing */
    widget = glade_xml_get_widget (swidget->xml, "signatures-tree");
    filter = GTK_TREE_MODEL_FILTER (gtk_tree_view_get_model (GTK_TREE_VIEW (widget)));
    
    if (filter) {
        store = GTK_TREE_STORE (gtk_tree_model_filter_get_model (filter));
        gtk_tree_store_clear (store);
    
    /* First time create the store */
    } else {
        
        /* Create a new SeahorseKeyModel store.... */
        store = GTK_TREE_STORE (seahorse_key_model_new (SIGN_N_COLUMNS, (GType*)sign_columns));
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
        
        glade_xml_signal_connect_data (swidget->xml, "on_signatures_toggle_toggled",
                                       G_CALLBACK (trusted_toggled), filter);
        widget = glade_xml_get_widget (swidget->xml, "signatures-toggle");
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);
        trusted_toggled (GTK_TOGGLE_BUTTON (widget), filter);
    } 

    signatures_populate_model (swidget, SEAHORSE_KEY_MODEL (store));
}

/* -----------------------------------------------------------------------------
 * GENERAL 
 */

static void
key_changed (SeahorseKey *skey, SeahorseKeyChange change, SeahorseWidget *swidget)
{
    switch (change) {
    case SKEY_CHANGE_PHOTOS:
        do_owner (swidget);
        break;
    
    case SKEY_CHANGE_UIDS:
    case SKEY_CHANGE_SIGNERS:
        do_owner (swidget);
        do_names (swidget);
        do_trust (swidget);
        break;
        
    case SKEY_CHANGE_SUBKEYS: 
        do_details (swidget);
        break;
    
    case SKEY_CHANGE_EXPIRES:
        do_details (swidget);
        break;
    
    case SKEY_CHANGE_TRUST:
        do_trust (swidget);
        do_details (swidget);
        break;
    
    default:
        do_owner (swidget);
        do_names (swidget);
        do_trust (swidget);
        do_details (swidget);
        break;
    }
}

static void
key_destroyed (GtkObject *object, SeahorseWidget *swidget)
{
    seahorse_widget_destroy (swidget);
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

static void
properties_destroyed (GtkObject *object, SeahorseWidget *swidget)
{
    g_signal_handlers_disconnect_by_func (SEAHORSE_KEY_WIDGET (swidget)->skey,
                                          key_changed, swidget);
    g_signal_handlers_disconnect_by_func (SEAHORSE_KEY_WIDGET (swidget)->skey,
                                          key_destroyed, swidget);
}

static SeahorseWidget*
setup_public_properties (SeahorsePGPKey *pkey, GtkWindow *parent)
{
    SeahorseWidget *swidget;
    GtkWidget *widget;

    swidget = seahorse_key_widget_new ("pgp-public-key-properties", parent, SEAHORSE_KEY (pkey));    
    
    /* This happens if the window is already open */
    if (swidget == NULL)
        return NULL;

    widget = glade_xml_get_widget (swidget->xml, swidget->name);
    g_signal_connect (widget, "response", G_CALLBACK (properties_response), swidget);
    g_signal_connect (GTK_OBJECT (widget), "destroy", G_CALLBACK (properties_destroyed), swidget);
    g_signal_connect_after (pkey, "changed", G_CALLBACK (key_changed), swidget);
    g_signal_connect_after (pkey, "destroy", G_CALLBACK (key_destroyed), swidget);

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

    return swidget;
}

static SeahorseWidget*
setup_private_properties (SeahorsePGPKey *pkey, GtkWindow *parent)
{
    SeahorseWidget *swidget;
    GtkWidget *widget;

    swidget = seahorse_key_widget_new ("pgp-private-key-properties", parent, SEAHORSE_KEY (pkey));

    /* This happens if the window is already open */
    if (swidget == NULL)
        return NULL;

    widget = glade_xml_get_widget (swidget->xml, swidget->name);
    g_signal_connect (widget, "response", G_CALLBACK (properties_response), swidget);
    g_signal_connect (GTK_OBJECT (widget), "destroy", G_CALLBACK (properties_destroyed), swidget);
    g_signal_connect_after (pkey, "changed", G_CALLBACK (key_changed), swidget);
    g_signal_connect_after (pkey, "destroy", G_CALLBACK (key_destroyed), swidget);

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

    return swidget;
}

void
seahorse_key_properties_new (SeahorsePGPKey *pkey, GtkWindow *parent)
{
    SeahorseKey *skey = SEAHORSE_KEY (pkey);
    SeahorseKeySource *sksrc;
    SeahorseWidget *swidget;
    gboolean remote;

    remote = seahorse_key_get_location (skey) == SKEY_LOC_REMOTE;

    /* Reload the key to make sure to get all the props */
    sksrc = seahorse_key_get_source (skey);
    g_return_if_fail (sksrc != NULL);
    
    /* Don't trigger the import of remote keys if possible */
    if (!remote) {
        /* This causes the key source to get any specific info about the key */
        seahorse_key_source_load_sync (sksrc, seahorse_key_get_keyid (skey));
        skey = seahorse_context_get_key (SCTX_APP(), sksrc, seahorse_key_get_keyid (skey));
        g_return_if_fail (skey != NULL);
    }
    
    if (seahorse_key_get_etype (skey) == SKEY_PUBLIC)
        swidget = setup_public_properties (pkey, parent);
    else
        swidget = setup_private_properties (pkey, parent);       
    
    if (swidget)
        seahorse_widget_show (swidget);
}
