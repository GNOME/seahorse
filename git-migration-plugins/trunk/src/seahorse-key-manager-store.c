/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
 * Copyright (C) 2004, 2005, 2006 Stefan Walter
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

#include "config.h"
#include <gnome.h>

#include "seahorse-key-manager-store.h"
#include "seahorse-preferences.h"
#include "seahorse-validity.h"
#include "seahorse-util.h"
#include "seahorse-gconf.h"
#include "seahorse-gpgmex.h"
#include "eggtreemultidnd.h"
#include "seahorse-pgp-key.h"
#include "seahorse-vfs-data.h"

#ifdef WITH_SSH
#include "seahorse-ssh-key.h"
#endif 

#define KEY_MANAGER_SORT_KEY "/apps/seahorse/listing/sort_by"

enum {
    PROP_0,
    PROP_KEYSET,
    PROP_MODE,
    PROP_FILTER
};

enum {
    COL_PAIR,
    COL_STOCK_ID,
    COL_NAME,
    COL_KEYID,
    COL_UID,
    COL_VALIDITY_STR,
    COL_TRUST_STR,
    COL_TYPE,
    COL_EXPIRES_STR,
    COL_VALIDITY,
    COL_EXPIRES,
    COL_TRUST,
    N_COLS
};

static const gchar* col_ids[] = {
    "pair",
    NULL,
    "name",
    "id",
    NULL,
    "validity",
    "trust",
    "type",
    "expires",
    "validity",
    "expires",
    "trust"
};

static const GType col_types[] = {
    G_TYPE_BOOLEAN,
    G_TYPE_STRING,
    G_TYPE_STRING,
    G_TYPE_STRING,
    G_TYPE_INT,
    G_TYPE_STRING,
    G_TYPE_STRING, 
    G_TYPE_STRING, 
    G_TYPE_STRING,
    G_TYPE_INT, 
    G_TYPE_LONG, 
    G_TYPE_INT
};

struct _SeahorseKeyManagerStorePriv {    
    GtkTreeModelFilter      *filter;
    GtkTreeModelSort        *sort;
    
    SeahorseKeyManagerStoreMode    filter_mode;
    gchar*                  filter_text;
    guint                   filter_stag;
};

G_DEFINE_TYPE (SeahorseKeyManagerStore, seahorse_key_manager_store, SEAHORSE_TYPE_KEY_MODEL);

/* -----------------------------------------------------------------------------
 * INTERNAL 
 */

/* Given a treeview iter, get the base store iterator */
static void 
get_base_iter (SeahorseKeyManagerStore *skstore, GtkTreeIter *base_iter, 
               const GtkTreeIter *iter)
{
    GtkTreeIter i;
    g_assert (skstore->priv->sort && skstore->priv->filter);
    
    gtk_tree_model_sort_convert_iter_to_child_iter (skstore->priv->sort, &i, (GtkTreeIter*)iter);
    gtk_tree_model_filter_convert_iter_to_child_iter (skstore->priv->filter, base_iter, &i);
}

/* Given a base store iter, get the treeview iter */
static gboolean 
get_upper_iter (SeahorseKeyManagerStore *skstore, GtkTreeIter *upper_iter, 
                const GtkTreeIter *iter)
{
    GtkTreeIter i;
    GtkTreePath *child_path, *path;
    gboolean ret;
    
    child_path = gtk_tree_model_get_path (gtk_tree_model_filter_get_model (skstore->priv->filter), 
                                          (GtkTreeIter*)iter);
    g_return_val_if_fail (child_path != NULL, FALSE);
    path = gtk_tree_model_filter_convert_child_path_to_path (skstore->priv->filter, child_path);
    gtk_tree_path_free (child_path);

    if (!path)
        return FALSE;

    ret = gtk_tree_model_get_iter (GTK_TREE_MODEL (skstore->priv->filter), &i, path);
    gtk_tree_path_free (path);
    
    if (!ret)
        return FALSE;
  
    gtk_tree_model_sort_convert_child_iter_to_iter (skstore->priv->sort, upper_iter, &i);
    return TRUE;
}

/* Try to find our key store given a tree model */
static SeahorseKeyManagerStore* 
key_store_from_model (GtkTreeModel *model)
{
    /* Sort models are what's set on the tree */
    if (GTK_IS_TREE_MODEL_SORT (model))
        model = gtk_tree_model_sort_get_model (GTK_TREE_MODEL_SORT (model));
    if (GTK_IS_TREE_MODEL_FILTER (model)) 
        model = gtk_tree_model_filter_get_model (GTK_TREE_MODEL_FILTER (model));
    if (SEAHORSE_IS_KEY_MANAGER_STORE (model))
        return SEAHORSE_KEY_MANAGER_STORE (model);
    
    g_assert_not_reached ();
    return NULL;
}

/* Given an iterator find the associated key */
static SeahorseKey*
key_from_iterator (GtkTreeModel* model, GtkTreeIter* iter, guint *uid)
{
    GtkTreeIter i;
    SeahorseKey *skey = NULL;
    
    /* Convert to base iter if necessary */
    if (!SEAHORSE_IS_KEY_MANAGER_STORE (model)) {
        SeahorseKeyManagerStore* skstore = key_store_from_model (model);
        get_base_iter (skstore, &i, iter);
        
        iter = &i;
        model = GTK_TREE_MODEL (skstore);
    }
    
    skey = seahorse_key_model_get_row_key (SEAHORSE_KEY_MODEL (model), iter);
    if (skey && uid)
        gtk_tree_model_get (model, iter, COL_UID, uid, -1);
    return skey;
}

/* Search through row for text */
static gboolean
row_contains_filtered_text (GtkTreeModel* model, GtkTreeIter* iter, const gchar* text)
{
    gchar* name = NULL;
    gchar* id = NULL;
    gchar* t;
    gboolean ret = FALSE;
    
    /* Empty search text results in a match */
    if (!text || !text[0])
        return TRUE;
    
    /* Note that we only search the name and id */
    gtk_tree_model_get (model, iter, COL_NAME, &name, COL_KEYID, &id, -1);
    
    if(name) {
        t = g_utf8_strdown (name, -1);
        if (strstr (t, text))
            ret = TRUE;
        g_free (t);
    }
    
    if (!ret && id) {
        t = g_utf8_strdown (id, -1);
        if (strstr (t, text))
            ret = TRUE;
        g_free (t);
    }
    
    g_free (name);
    g_free (id);
    return ret;
}

/* Called to filter each row */
static gboolean 
filter_callback (GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
    SeahorseKeyManagerStore* skstore = SEAHORSE_KEY_MANAGER_STORE(data);
    GtkTreeIter child;
    gboolean ret = FALSE;
    
    /* Check the row requested */
    switch (skstore->priv->filter_mode)
    {
    case KEY_STORE_MODE_FILTERED:
        ret = row_contains_filtered_text (model, iter, skstore->priv->filter_text);
        break;
        
    case KEY_STORE_MODE_ALL:
        ret = TRUE;
        break;
        
    default:
        g_assert_not_reached ();
        break;
    };
    
    /* If current is not being shown, double check with children */
    if (!ret && gtk_tree_model_iter_children (model, &child, iter)) {
        do {
            ret = filter_callback (model, &child, data);
        } while (!ret && gtk_tree_model_iter_next (model, &child));
    }

    return ret;
}

/* Refilter the tree */
static gboolean
refilter_now (SeahorseKeyManagerStore* skstore)
{
    seahorse_keyset_refresh (skstore->skset);
    gtk_tree_model_filter_refilter (skstore->priv->filter);    
    skstore->priv->filter_stag = 0;
    return FALSE;
}

/* Refilter the tree after a timeout has passed */
static void
refilter_later (SeahorseKeyManagerStore* skstore)
{
    if (skstore->priv->filter_stag != 0)
        g_source_remove (skstore->priv->filter_stag);
    skstore->priv->filter_stag = g_timeout_add (200, (GSourceFunc)refilter_now, skstore);
}

/* Sets Name and KeyID */
static void
update_key_row (SeahorseKeyManagerStore *skstore, SeahorseKey *skey, guint uid, 
                GtkTreeIter *iter)
{
    SeahorseValidity validity, trust;
    SeahorseKeyPredicate *pred;
    SeahorseKeySource *sksrc;
    const gchar *stockid;
    gulong expires_date;
    gchar *markup;
    gboolean sec;
    gchar *expires;
    gchar *type;

    markup = seahorse_key_get_name_markup (skey, uid);
    sec = seahorse_key_get_etype (skey) == SKEY_PRIVATE;
    stockid = seahorse_key_get_stock_id (skey);
    validity = seahorse_key_get_name_validity (skey, uid);
    
    if (uid == 0) {
        
        trust = seahorse_key_get_trust (skey);
        
        if (seahorse_key_get_flags (skey) & SKEY_FLAG_EXPIRED) {
            expires = g_strdup (_("Expired"));
            expires_date = -1;
        } else {
            expires_date = seahorse_key_get_expires (skey);
            
            if (expires_date == 0) {
                expires = g_strdup ("");
                expires_date = G_MAXLONG;
            }
            else
                expires = seahorse_util_get_display_date_string (expires_date);
        }
        
        /* Only differentiate if the view shows more than one type of key */
        g_object_get (skstore->skset, "predicate", &pred, NULL);
        
        /* If mixed etypes, then get specific description */
        if (pred->etype == 0 || pred->etype == SKEY_CREDENTIALS) {
            type = g_strdup (seahorse_key_get_desc (skey));
            
        /* Otherwise general description */
        } else {
            sksrc = seahorse_key_get_source (skey);
            g_return_if_fail (sksrc);
            g_object_get (sksrc, "key-desc", &type, NULL);
        }

        gtk_tree_store_set (GTK_TREE_STORE (skstore), iter,
            COL_PAIR, uid == 0 ? sec : FALSE,
            COL_STOCK_ID, uid == 0 ? stockid : NULL,
            COL_NAME, markup,
            COL_KEYID, seahorse_key_get_short_keyid (skey),
            COL_UID, uid,
            COL_VALIDITY_STR, validity == SEAHORSE_VALIDITY_UNKNOWN ? "" : seahorse_validity_get_string (validity),
            COL_VALIDITY, validity,
            COL_TRUST_STR, trust == SEAHORSE_VALIDITY_UNKNOWN ? "" : seahorse_validity_get_string (trust),
            COL_TRUST, trust,
            COL_TYPE, type,
            COL_EXPIRES_STR, expires ? expires : "",
            COL_EXPIRES, expires_date,
            -1);
         
        g_free (type);
        g_free (expires);
       
    } else {

        gtk_tree_store_set (GTK_TREE_STORE (skstore), iter,
            COL_KEYID, "", 
            COL_NAME, markup,
            COL_UID, uid,
            COL_VALIDITY_STR, validity == SEAHORSE_VALIDITY_UNKNOWN ? "" : seahorse_validity_get_string (validity),
            COL_VALIDITY, validity,
            COL_TYPE, _("User ID"), 
            -1);
    }
    
    g_free (markup);
}

static gboolean
append_key_row (SeahorseKeyManagerStore *skstore, SeahorseKey *skey, guint uid, 
                GtkTreeIter *iter)
{
    GtkTreeIter child;

    if (uid == 0) {
        gtk_tree_store_append (GTK_TREE_STORE (skstore), iter, NULL);
        update_key_row (skstore, skey, uid, iter);
        seahorse_key_model_set_row_key (SEAHORSE_KEY_MODEL (skstore), iter, skey);
    } else {
        gtk_tree_store_append (GTK_TREE_STORE (skstore), &child, iter);
        update_key_row (skstore, skey, uid, &child);
        seahorse_key_model_set_row_key (SEAHORSE_KEY_MODEL (skstore), &child, skey);
    }
    
    return TRUE;
}

static void
key_added (SeahorseKeyset *skset, SeahorseKey *skey, SeahorseKeyManagerStore *skstore)
{
    GtkTreeIter iter = { 0, };
    guint i, uids = seahorse_key_get_num_names (skey);
 
    for (i = 0; i < uids; i++) {
        if (!append_key_row (skstore, skey, i, &iter))
            break;
    }
}

static void
key_removed (SeahorseKeyset *skset, SeahorseKey *skey, gpointer closure, 
             SeahorseKeyManagerStore *skstore)
{
    seahorse_key_model_remove_rows_for_key (SEAHORSE_KEY_MODEL (skstore), skey);
}

static void
key_changed (SeahorseKeyset *skset, SeahorseKey *skey, SeahorseKeyChange change, 
             gpointer closure, SeahorseKeyManagerStore *skstore)
{
    guint i, uid, old_uids, num_uids;
    GtkTreeIter first;
    GtkTreeIter *iter;
    GSList *rows, *l;
    
    old_uids = 0;
    num_uids = seahorse_key_get_num_names (skey);
    rows = seahorse_key_model_get_rows_for_key (SEAHORSE_KEY_MODEL (skstore), skey);
    
    for (l = rows; l; l = g_slist_next (l)) {
        
        iter = (GtkTreeIter*)l->data;
        gtk_tree_model_get (GTK_TREE_MODEL (skstore), iter, COL_UID, &uid, -1);
        
        /* Remove any extra rows */
        if (uid >= num_uids) {
            seahorse_key_model_set_row_key (SEAHORSE_KEY_MODEL (skstore), iter, NULL);
            gtk_tree_store_remove (GTK_TREE_STORE (skstore), iter);
            continue;
        }

        update_key_row (skstore, skey, uid, iter);
        
        /* The top parent row */
        if (uid == 0)
            memcpy (&first, iter, sizeof (first));            

        /* Find the max uid on the rows */
        if (uid >= old_uids)
            old_uids = uid + 1;
    }

    /* Add all new rows */    
    for (i = old_uids; i < num_uids; i++)
        append_key_row (skstore, skey, i, &first);
    
    seahorse_key_model_free_rows (rows);
}

static void
populate_store (SeahorseKeyManagerStore *skstore)
{
    GList *keys, *list = NULL;
    SeahorseKey *skey;
    
    keys = list = seahorse_keyset_get_keys (skstore->skset);
    while (list != NULL && (skey = list->data) != NULL) {
        key_added (skstore->skset, skey, skstore);
        list = g_list_next (list);
    }
    g_list_free (keys);
}

/* Update the sort order for a column */
static void
set_sort_to (SeahorseKeyManagerStore *skstore, const gchar *name)
{
    GtkTreeSortable* sort;
    gint i, id = -1;
    GtkSortType ord = GTK_SORT_ASCENDING;
    const gchar* n;
    
    g_return_if_fail (name != NULL);
    
    /* Prefix with a minus means descending */
    if (name[0] == '-') {
        ord = GTK_SORT_DESCENDING;
        name++;
    }
    
    /* Prefix with a plus means ascending */
    else if (name[0] == '+') {
        ord = GTK_SORT_ASCENDING;
        name++;
    }
    
    /* Find the column id */
    for (i = N_COLS - 1; i >= 0 ; i--) {
        n = col_ids[i];
        if (n && g_ascii_strcasecmp (name, n) == 0) {
            id = i;
            break;
        }
    }
    
    if (id != -1) {
        sort = GTK_TREE_SORTABLE (skstore->priv->sort);
        gtk_tree_sortable_set_sort_column_id (sort, id, ord);
    }
}

/* Called when the column sort is changed */
static void
sort_changed (GtkTreeSortable *sort, gpointer user_data)
{
    gint id;
    GtkSortType ord;
    SeahorseKeyManagerStore *skstore;
    const gchar* t;
    gchar* x;
    
    skstore = SEAHORSE_KEY_MANAGER_STORE (user_data);
        
    /* We have a sort so save it */
    if (gtk_tree_sortable_get_sort_column_id (sort, &id, &ord)) {
        if (id >= 0 && id < N_COLS) {
            t = col_ids[id];
            if (t != NULL) {
                x = g_strconcat (ord == GTK_SORT_DESCENDING ? "-" : "", t, NULL);
                seahorse_gconf_set_string (KEY_MANAGER_SORT_KEY, x);
                g_free (x);
            }
        }
    }
    
    /* No sort so save blank */
    else {
        seahorse_gconf_set_string (KEY_MANAGER_SORT_KEY, "");
    }
}

static void
gconf_notification (GConfClient *gclient, guint id, GConfEntry *entry, GtkTreeView *view)
{
    SeahorseKeyManagerStore *skstore;
    GtkTreeViewColumn *col = NULL;
    GList *columns, *l;
    GConfValue *value;
    const gchar *key;
    const gchar *t;

    key = gconf_entry_get_key (entry);

    g_assert (key != NULL);
    g_assert (GTK_IS_TREE_VIEW (view));
    
    if (g_str_equal (key, KEY_MANAGER_SORT_KEY)) {
        skstore = key_store_from_model (gtk_tree_view_get_model(view));
        g_return_if_fail (skstore != NULL);
        set_sort_to (skstore, gconf_value_get_string (gconf_entry_get_value (entry)));
    }
    
    columns = gtk_tree_view_get_columns (view);
    for (l = columns; l; l = g_list_next (l)) {
        t = (const gchar*)g_object_get_data (G_OBJECT (l->data), "gconf-key");
        if (t && g_str_equal (t, key)) {
            col = GTK_TREE_VIEW_COLUMN (l->data);
            break;
        }
    }
    
    if (col != NULL) {
        value = gconf_entry_get_value (entry);
        gtk_tree_view_column_set_visible (col, gconf_value_get_bool (value));
    }
    
    g_list_free (columns);
}

static GtkTreeViewColumn*
append_text_column (SeahorseKeyManagerStore *skstore, GtkTreeView *view, 
               const gchar *label, const gint index)
{
    GtkTreeViewColumn *column;
    GtkCellRenderer *renderer;
    
    renderer = gtk_cell_renderer_text_new ();
    g_object_set (renderer, "xpad", 3, NULL);
    column = gtk_tree_view_column_new_with_attributes (label, renderer, "text", index, NULL);
    gtk_tree_view_column_set_resizable (column, TRUE);
    gtk_tree_view_append_column (view, column);
    
    return column;
}

static void  
drag_begin (GtkWidget *widget, GdkDragContext *context, SeahorseKeyManagerStore *skstore)
{
    GtkTreeView *view = GTK_TREE_VIEW (widget);
    SeahorseKeySource *sksrc;
    SeahorseMultiOperation *mop = NULL;
    SeahorseOperation *op = NULL;
    GList *next, *keys, *sel_keys = NULL;
    gpgme_data_t data = NULL;
    SeahorseKey *skey;
    
    DBG_PRINT (("drag_begin -->\n"));
    
    sel_keys = seahorse_key_manager_store_get_selected_keys (view);
    if(sel_keys != NULL) {
        
        /* Sort by key source */
        keys = g_list_copy (sel_keys);
        keys = seahorse_util_keylist_sort (keys);
    
        while (keys) {
     
            /* Break off one set (same keysource) */
            next = seahorse_util_keylist_splice (keys);

            g_assert (SEAHORSE_IS_KEY (keys->data));
            skey = SEAHORSE_KEY (keys->data);

            /* Export from this key source */        
            sksrc = seahorse_key_get_source (skey);
            g_return_if_fail (sksrc != NULL);
            
            if (!mop) 
                mop = seahorse_multi_operation_new ();
            
            /* The data object where we export to */
            if (!data) {
                data = gpgmex_data_new ();
                g_object_set_data_full (G_OBJECT (mop), "result-data", data,
                                        (GDestroyNotify)gpgmex_data_release);
            }
        
            /* We pass our own data object, to which data is appended */
            op = seahorse_key_source_export (sksrc, keys, FALSE, data);
            g_return_if_fail (op != NULL);

            g_list_free (keys);
            keys = next;

            seahorse_multi_operation_take (mop, op);
        }
        
        g_object_set_data_full (G_OBJECT (view), "drag-operation", mop,
                                (GDestroyNotify)g_object_unref);
        g_object_set_data_full (G_OBJECT (view), "drag-keys", sel_keys,
                                (GDestroyNotify)g_list_free);
        g_object_set_data (G_OBJECT (view), "drag-file", NULL);
    }
    
    DBG_PRINT (("drag_begin <--\n"));
}

static void
cleanup_file (gchar *file)
{
    g_assert (file != NULL);
    DBG_PRINT (("deleting temp file: %s\n", file));
    unlink (file);
    g_free (file);
}

static void  
drag_end (GtkWidget *widget, GdkDragContext *context, SeahorseKeyManagerStore *skstore)
{
    DBG_PRINT (("drag_end -->\n"));
    
    /* This frees the operation and key list if present */
    g_object_set_data (G_OBJECT (widget), "drag-operation", NULL);
    g_object_set_data (G_OBJECT (widget), "drag-keys", NULL);

    DBG_PRINT (("drag_end <--\n"));
}

static void  
drag_data_get (GtkWidget *widget, GdkDragContext *context, 
               GtkSelectionData *selection_data, guint info, 
               guint time, SeahorseKeyManagerStore *skstore)
{
    SeahorseOperation *op;
    gchar *t, *n;
    GList *keys;
    GError *err = NULL;
    gpgme_data_t data;
    gchar *text;

    DBG_PRINT (("drag_data_get %d -->\n", info)); 

    op = (SeahorseOperation*)g_object_get_data (G_OBJECT (widget), "drag-operation");
    if (op == NULL) {
        DBG_PRINT (("No operation in drag"));
        return;
    }

    /* Make sure it's complete before we can return data */
    seahorse_operation_wait (op);

    if (!seahorse_operation_is_successful (op)) {
        g_object_set_data (G_OBJECT (widget), "drag-operation", NULL);
        seahorse_operation_copy_error (op, &err);
        seahorse_util_handle_error (err, _("Couldn't retrieve key data"));
        return;
    }
    
    data = (gpgme_data_t)g_object_get_data (G_OBJECT (op), "result-data");
    g_return_if_fail (data != NULL);
    
    text = seahorse_util_write_data_to_text (data, NULL);
    g_return_if_fail (text != NULL);

    if (info == TEXT_PLAIN) {
        DBG_PRINT (("returning key text\n"));
        gtk_selection_data_set_text (selection_data, text, strlen (text));
    } else if (info == TEXT_URIS) {
        t = (gchar*)g_object_get_data (G_OBJECT (widget), "drag-file");

        if (t == NULL) {
            keys = g_object_get_data (G_OBJECT (widget), "drag-keys");
            g_return_if_fail (keys != NULL);
            
            n = seahorse_util_filename_for_keys (keys);
            g_return_if_fail (n != NULL);
            t = g_build_filename(g_get_tmp_dir (), n, NULL);
            g_free (n);

            g_object_set_data_full (G_OBJECT (widget), "drag-file", t,
                                    (GDestroyNotify)cleanup_file);
            
            DBG_PRINT (("writing to temp file: %s\n", t));
            
            if (!seahorse_vfs_set_file_contents (t, text, strlen (text), &err)) {
                seahorse_util_handle_error (err, _("Couldn't write key to file"));
                g_object_set_data (G_OBJECT (widget), "drag-file", NULL);
                t = NULL;
            }
        }

        if (t != NULL) {
            char *uris[2] = { t, NULL };
            gtk_selection_data_set_uris (selection_data, uris);
        }
    }
    
    DBG_PRINT(("drag_data_get <--\n"));

    g_free(text);
}

static gint 
compare_pointers (gconstpointer a, gconstpointer b)
{
    if (a == b)
        return 0;
    return a > b ? 1 : -1;
}

/* -----------------------------------------------------------------------------
 * OBJECT 
 */

static void
seahorse_key_manager_store_init (SeahorseKeyManagerStore *skstore)
{
    /* init private vars */
    skstore->priv = g_new0 (SeahorseKeyManagerStorePriv, 1);
    
    /* Setup the store */
    seahorse_key_model_set_column_types (SEAHORSE_KEY_MODEL (skstore), N_COLS, (GType *) col_types);
    
    /* Setup the sort and filter */
    skstore->priv->filter = GTK_TREE_MODEL_FILTER (gtk_tree_model_filter_new (GTK_TREE_MODEL (skstore), NULL));
    gtk_tree_model_filter_set_visible_func (skstore->priv->filter, filter_callback, skstore, NULL);
    skstore->priv->sort = GTK_TREE_MODEL_SORT (gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (skstore->priv->filter)));
}

static void
seahorse_key_manager_store_get_property (GObject *gobject, guint prop_id,
                                         GValue *value, GParamSpec *pspec)
{
    SeahorseKeyManagerStore *skstore = SEAHORSE_KEY_MANAGER_STORE (gobject);
    
    switch (prop_id) {
    case PROP_KEYSET:
        g_value_set_object (value, skstore->skset);
        break;
        
    /* The filtering mode */
    case PROP_MODE:
        g_value_set_uint (value, skstore->priv->filter_mode);
        break;
        
    /* The filter text. Note that we act as if we don't have any 
     * filter text when not in filtering mode */
    case PROP_FILTER:
        g_value_set_string (value, 
            skstore->priv->filter_mode == KEY_STORE_MODE_FILTERED ? skstore->priv->filter_text : "");
        break;
    
    default:
        break;
    }
}

static void
seahorse_key_manager_store_set_property (GObject *gobject, guint prop_id,
                                         const GValue *value, GParamSpec *pspec)
{
    SeahorseKeyManagerStore *skstore = SEAHORSE_KEY_MANAGER_STORE (gobject);
    const gchar* t;
    
    switch (prop_id) {
    case PROP_KEYSET:
        g_assert (skstore->skset == NULL);
        skstore->skset = g_value_get_object (value);
        g_object_ref (skstore->skset);
        g_signal_connect_after (skstore->skset, "added", G_CALLBACK (key_added), skstore);
        g_signal_connect_after (skstore->skset, "removed", G_CALLBACK (key_removed), skstore);
        g_signal_connect_after (skstore->skset, "changed", G_CALLBACK (key_changed), skstore);
        break;

    /* The filtering mode */
    case PROP_MODE:
        if (skstore->priv->filter_mode != g_value_get_uint (value)) {
            skstore->priv->filter_mode = g_value_get_uint (value);
            refilter_later (skstore);
        }
        break;
        
    /* The filter text */
    case PROP_FILTER:
        t = g_value_get_string (value);
            
        /* 
         * If we're not in filtered mode and there is text OR
         * we're in filtered mode (regardless of text or not)
         * then update the filter
         */
        if ((skstore->priv->filter_mode != KEY_STORE_MODE_FILTERED && t && t[0]) ||
            (skstore->priv->filter_mode == KEY_STORE_MODE_FILTERED)) {
            skstore->priv->filter_mode = KEY_STORE_MODE_FILTERED;
            g_free (skstore->priv->filter_text);
                
            /* We always use lower case text (see filter_callback) */
            skstore->priv->filter_text = g_utf8_strdown (t, -1);
            refilter_later (skstore);
        }
        break;
        
    default:
        break;
    }
}

static void
seahorse_key_manager_store_dispose (GObject *gobject)
{
    SeahorseKeyManagerStore *skstore;
    
    /*
     * Note that after this executes the rest of the object should
     * still work without a segfault. This basically nullifies the 
     * object, but doesn't free it.
     * 
     * This function should also be able to run multiple times.
     */

    skstore = SEAHORSE_KEY_MANAGER_STORE (gobject);  

    if (skstore->skset) {
        g_signal_handlers_disconnect_by_func (skstore->skset, key_added, skstore);
        g_signal_handlers_disconnect_by_func (skstore->skset, key_removed, skstore);
        g_signal_handlers_disconnect_by_func (skstore->skset, key_changed, skstore);
        g_object_unref (skstore->skset);        
        skstore->skset = NULL;
    }
    
    G_OBJECT_CLASS (seahorse_key_manager_store_parent_class)->dispose (gobject);
}
    
static void
seahorse_key_manager_store_finalize (GObject *gobject)
{
    SeahorseKeyManagerStore *skstore = SEAHORSE_KEY_MANAGER_STORE (gobject);
    g_assert (skstore->skset == NULL);
    
    /* These were allocated in the constructor */
    g_object_unref (skstore->priv->sort);
    g_object_unref (skstore->priv->filter);
     
    /* Allocated in property setter */
    g_free (skstore->priv->filter_text); 
    
    G_OBJECT_CLASS (seahorse_key_manager_store_parent_class)->finalize (gobject);
}

static void
seahorse_key_manager_store_class_init (SeahorseKeyManagerStoreClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    seahorse_key_manager_store_parent_class = g_type_class_peek_parent (klass);
    
    /* Some simple checks to make sure all column info is on the same page */
    g_assert (N_COLS == G_N_ELEMENTS (col_ids));
    g_assert (N_COLS == G_N_ELEMENTS (col_types));

    gobject_class->finalize = seahorse_key_manager_store_finalize;
    gobject_class->dispose = seahorse_key_manager_store_dispose;
    gobject_class->set_property = seahorse_key_manager_store_set_property;
    gobject_class->get_property = seahorse_key_manager_store_get_property;
  
    g_object_class_install_property (gobject_class, PROP_KEYSET,
        g_param_spec_object ("keyset", "Seahorse Keyset", "Current Seahorse Key Source to use",
                             SEAHORSE_TYPE_KEYSET, G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class, PROP_MODE,
        g_param_spec_uint ("mode", "Key Store Mode", "Key store mode controls which keys to display",
                           0, KEY_STORE_MODE_FILTERED, KEY_STORE_MODE_ALL, G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class, PROP_FILTER,
        g_param_spec_string ("filter", "Key Store Filter", "Key store filter for when in filtered mode",
                             "", G_PARAM_READWRITE));
}

/* -----------------------------------------------------------------------------
 * PUBLIC 
 */

SeahorseKeyManagerStore*
seahorse_key_manager_store_new (SeahorseKeyset *skset, GtkTreeView *view)
{
    SeahorseKeyManagerStore *skstore;
    GtkTreeViewColumn *col;
    SeahorseKeyPredicate *pred;
    GtkCellRenderer *renderer;
    gchar *sort;
    GtkTargetList *targets;
    
    skstore = g_object_new (SEAHORSE_TYPE_KEY_MANAGER_STORE, "keyset", skset, NULL);

    populate_store (skstore);

    /* The sorted model is the top level model */
    g_assert (GTK_IS_TREE_MODEL (skstore->priv->sort));
    gtk_tree_view_set_model (view, GTK_TREE_MODEL (skstore->priv->sort));

    /* add the icon column */
    renderer = gtk_cell_renderer_pixbuf_new ();
    g_object_set (renderer, "stock-size", GTK_ICON_SIZE_LARGE_TOOLBAR, NULL);
    g_object_set (renderer, "xpad", 6, NULL);
    col = gtk_tree_view_column_new_with_attributes ("", renderer, "stock-id", COL_STOCK_ID, NULL);
    gtk_tree_view_column_set_resizable (col, FALSE);
    gtk_tree_view_append_column (view, col);
    gtk_tree_view_column_set_sort_column_id (col, COL_PAIR);
    
    /* Name column */
    renderer = gtk_cell_renderer_text_new ();
    col = gtk_tree_view_column_new_with_attributes (_("Name"), renderer, "markup", COL_NAME, NULL);
    gtk_tree_view_column_set_resizable (col, TRUE);
    gtk_tree_view_column_set_expand (col, TRUE);
    gtk_tree_view_append_column (view, col);
    gtk_tree_view_set_expander_column (view, col);
    gtk_tree_view_column_set_sort_column_id (col, COL_NAME);

    /* Use predicate to figure out which columns to add */
    g_object_get (skset, "predicate", &pred, NULL);
    
    /* Key ID column, don't show for passwords */
    if (pred->etype != SKEY_CREDENTIALS) {
        col = append_text_column (skstore, view, _("Key ID"), COL_KEYID);
        gtk_tree_view_column_set_sort_column_id (col, COL_KEYID);
    }

    /* Public keys show validity */
    if (pred->etype == SKEY_PUBLIC) {
        col = append_text_column (skstore, view, _("Validity"), COL_VALIDITY_STR);
        g_object_set_data (G_OBJECT (col), "gconf-key", SHOW_VALIDITY_KEY);
        gtk_tree_view_column_set_visible (col, seahorse_gconf_get_boolean (SHOW_VALIDITY_KEY));
        gtk_tree_view_column_set_sort_column_id (col, COL_VALIDITY);
    }

    /* Trust column */
    col = append_text_column (skstore, view, _("Trust"), COL_TRUST_STR);
    g_object_set_data (G_OBJECT (col), "gconf-key", SHOW_TRUST_KEY);
    gtk_tree_view_column_set_visible (col, seahorse_gconf_get_boolean (SHOW_TRUST_KEY));
    gtk_tree_view_column_set_sort_column_id (col, COL_TRUST);
    
    /* The key type column */
    col = append_text_column (skstore, view, _("Type"), COL_TYPE);
    g_object_set_data (G_OBJECT (col), "gconf-key", SHOW_TYPE_KEY);
    gtk_tree_view_column_set_visible (col, seahorse_gconf_get_boolean (SHOW_TYPE_KEY));
    gtk_tree_view_column_set_sort_column_id (col, COL_TYPE);

    /* Expiry date column */
    col = append_text_column (skstore, view, _("Expiration Date"), COL_EXPIRES_STR);
    g_object_set_data (G_OBJECT (col), "gconf-key", SHOW_EXPIRES_KEY);
    gtk_tree_view_column_set_visible (col, seahorse_gconf_get_boolean (SHOW_EXPIRES_KEY));
    gtk_tree_view_column_set_sort_column_id (col, COL_EXPIRES);

    /* Also watch for sort-changed on the store */
    g_signal_connect (skstore->priv->sort, "sort-column-changed", G_CALLBACK (sort_changed), skstore);

    /* Update sort order in case the sorted column was added */
    if ((sort = seahorse_gconf_get_string (KEY_MANAGER_SORT_KEY)) != NULL) {
        set_sort_to (skstore, sort);
        g_free (sort);
    }  
    
    seahorse_gconf_notify_lazy (LISTING_SCHEMAS, (GConfClientNotifyFunc)gconf_notification, 
                                view, GTK_WIDGET (view));

    /* Tree drag */
    egg_tree_multi_drag_add_drag_support (view);    
    
    g_signal_connect (G_OBJECT (view), "drag_data_get", G_CALLBACK (drag_data_get), skstore);
    g_signal_connect (G_OBJECT (view), "drag_begin",  G_CALLBACK (drag_begin), skstore);
    g_signal_connect (G_OBJECT (view), "drag_end",  G_CALLBACK (drag_end), skstore);

    gtk_drag_source_set (GTK_WIDGET (view), GDK_BUTTON1_MASK | GDK_BUTTON2_MASK,
                         NULL, 0, GDK_ACTION_COPY);
    targets = gtk_target_list_new (NULL, 0);
    gtk_target_list_add_uri_targets (targets, TEXT_URIS);
    gtk_target_list_add_text_targets (targets, TEXT_PLAIN);
    gtk_drag_source_set_target_list (GTK_WIDGET (view), targets);
    gtk_target_list_unref (targets);

    return skstore;
}

SeahorseKey*
seahorse_key_manager_store_get_key_from_path (GtkTreeView *view, GtkTreePath *path, guint *uid)
{
    GtkTreeModel *model;
    GtkTreeIter iter;
    
    g_return_val_if_fail (GTK_IS_TREE_VIEW (view), NULL);
    g_return_val_if_fail (path != NULL, NULL);
    
    model = gtk_tree_view_get_model (view);
    g_return_val_if_fail (gtk_tree_model_get_iter (model, &iter, path), NULL);
    return key_from_iterator (model, &iter, uid);
}

GList*
seahorse_key_manager_store_get_all_keys (GtkTreeView *view)
{
    SeahorseKeyManagerStore* skstore;
    
    g_return_val_if_fail (GTK_IS_TREE_VIEW (view), NULL);
    skstore = key_store_from_model (gtk_tree_view_get_model (view));
    return seahorse_keyset_get_keys (skstore->skset);
}

GList*
seahorse_key_manager_store_get_selected_keys (GtkTreeView *view)
{
    SeahorseKey *skey;
    GList *l, *keys = NULL;
    SeahorseKeyManagerStore* skstore;
    GList *list, *paths = NULL;
    GtkTreeSelection *selection;    
    
    g_return_val_if_fail (GTK_IS_TREE_VIEW (view), NULL);
    skstore = key_store_from_model (gtk_tree_view_get_model (view));
    g_return_val_if_fail (SEAHORSE_IS_KEY_MANAGER_STORE (skstore), NULL);
    
    selection = gtk_tree_view_get_selection (view);
    paths = gtk_tree_selection_get_selected_rows (selection, NULL);

    /* make key list */
    for (list = paths; list != NULL; list = g_list_next (list)) {
        skey = seahorse_key_manager_store_get_key_from_path (view, list->data, NULL);
        if (skey != NULL)
            keys = g_list_append (keys, skey);
    }
        
    /* free selected paths */
    g_list_foreach (paths, (GFunc)gtk_tree_path_free, NULL);
    g_list_free (paths);
    
    /* Remove duplicates */
    keys = g_list_sort (keys, compare_pointers);
    for (l = keys; l; l = g_list_next (l)) {
        while (l->next && l->data == l->next->data)
            keys = g_list_delete_link (keys, l->next);
    }    
    
    return keys;
}

void
seahorse_key_manager_store_set_selected_keys (GtkTreeView *view, GList* keys)
{
    SeahorseKeyManagerStore* skstore;
    GtkTreeSelection* selection;
    gboolean first = TRUE;
    GtkTreePath *path;
    GList *l;
    GSList *rows, *rl;
    GtkTreeIter it;
    
    g_return_if_fail (GTK_IS_TREE_VIEW (view));
    selection = gtk_tree_view_get_selection (view);
    gtk_tree_selection_unselect_all (selection);
    
    skstore = key_store_from_model (gtk_tree_view_get_model (view));
    g_return_if_fail (SEAHORSE_IS_KEY_MANAGER_STORE (skstore));
    
    for(l = keys; l; l = g_list_next (l)) {
        
        /* Get all the rows for that key .... */
        rows = seahorse_key_model_get_rows_for_key (SEAHORSE_KEY_MODEL (skstore), 
                                                    SEAHORSE_KEY (l->data));
        for(rl = rows; rl; rl = g_slist_next (rl)) {

            /* And select them ... */
            if (get_upper_iter(skstore, &it, (GtkTreeIter*)rl->data)) {
                gtk_tree_selection_select_iter (selection, &it);
                
                /* Scroll the first row selected into view */
                if (first) {
                    path = gtk_tree_model_get_path (gtk_tree_view_get_model (view), &it);
                    gtk_tree_view_scroll_to_cell (view, path, NULL, FALSE, 0.0, 0.0);
                    gtk_tree_path_free (path);
                    first = FALSE;
                }
            }
        }
        
        seahorse_key_model_free_rows (rows);
    }
}

SeahorseKey*
seahorse_key_manager_store_get_selected_key (GtkTreeView *view, guint *uid)
{
    SeahorseKeyManagerStore* skstore;
    SeahorseKey *skey = NULL;
    GList *paths = NULL;
    GtkTreeSelection *selection;
    
    g_return_val_if_fail (GTK_IS_TREE_VIEW (view), NULL);
    skstore = key_store_from_model (gtk_tree_view_get_model (view));
    g_return_val_if_fail (SEAHORSE_IS_KEY_MANAGER_STORE (skstore), NULL);
    
    selection = gtk_tree_view_get_selection (view);
    paths = gtk_tree_selection_get_selected_rows (selection, NULL);
    
    /* make key list */
    if (paths != NULL) {
        skey = seahorse_key_manager_store_get_key_from_path (view, paths->data, uid);
            
        /* free selected paths */
        g_list_foreach (paths, (GFunc)gtk_tree_path_free, NULL);
        g_list_free (paths);
    }
    
    return skey;
}
