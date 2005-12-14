/*
 * Seahorse
 *
 * Copyright (C) 2005 Nate Nielsen
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
 * along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"
#include <gtk/gtk.h>
#include <string.h>

#include "cryptui-key-store.h"

enum {
    PROP_0,
    PROP_KEYSET,
    PROP_MODE,
    PROP_FILTER,
    PROP_USE_CHECK,
};

/* This should always match the list in cryptui-keystore.h */
static const GType col_types[] = {
    G_TYPE_STRING,
    G_TYPE_STRING,
    G_TYPE_BOOLEAN,
    G_TYPE_BOOLEAN,
    G_TYPE_STRING,
    G_TYPE_POINTER
};

struct _CryptUIKeyStorePriv {    
    GtkTreeModelFilter      *filter;
    GtkTreeStore            *store;
    
    CryptUIKeyStoreMode     filter_mode;
    gchar                   *filter_text;
    guint                   filter_stag;
    
    gboolean                use_check;
};

G_DEFINE_TYPE (CryptUIKeyStore, cryptui_key_store, GTK_TYPE_TREE_MODEL_SORT);

/* -----------------------------------------------------------------------------
 * INTERNAL
 */

static gint 
compare_pointers (gconstpointer a, gconstpointer b)
{
    if (a == b)
        return 0;
    return a > b ? 1 : -1;
}

/* Try to find our key store given a tree model */
static CryptUIKeyStore* 
key_store_from_model (GtkTreeModel *model)
{
    /* Sort models are what's set on the tree */
    g_assert (GTK_IS_TREE_MODEL_SORT (model));
    g_assert (CRYPTUI_IS_KEY_STORE (model));
    
    return CRYPTUI_KEY_STORE (model);
}

static void
key_store_row_add (CryptUIKeyStore *ckstore, const gchar *key, GtkTreeIter *iter)
{
    GtkTreePath *path;
    GtkTreeRowReference *ref;
    
    /* Do we already have a row for this key? */
    ref = (GtkTreeRowReference*)cryptui_keyset_get_closure (ckstore->ckset, key);
    g_return_if_fail (ref == NULL);

    gtk_tree_store_append (GTK_TREE_STORE (ckstore->priv->store), iter, NULL);
    path = gtk_tree_model_get_path (GTK_TREE_MODEL (ckstore->priv->store), iter);
    ref = gtk_tree_row_reference_new (GTK_TREE_MODEL (ckstore->priv->store), path);
    gtk_tree_path_free (path);
    
    cryptui_keyset_set_closure (ckstore->ckset, key, ref);
    gtk_tree_store_set (GTK_TREE_STORE (ckstore->priv->store), iter, CRYPTUI_KEY_STORE_KEY, key, -1);
}

/* Sets Name and KeyID */
static void
key_store_set (CryptUIKeyStore *ckstore, const gchar *key, GtkTreeIter *iter)
{
    gchar *userid = cryptui_key_get_display_name (key);
    gchar *keyid = cryptui_key_get_display_id (key);
    gboolean sec = cryptui_key_get_enctype (key) == CRYPTUI_ENCTYPE_PRIVATE;
    
    gtk_tree_store_set (GTK_TREE_STORE (ckstore->priv->store), iter,
                        CRYPTUI_KEY_STORE_CHECK, FALSE,
                        CRYPTUI_KEY_STORE_PAIR, sec,
                        CRYPTUI_KEY_STORE_STOCK_ID, NULL,
                        CRYPTUI_KEY_STORE_NAME, userid,
                        CRYPTUI_KEY_STORE_KEYID, keyid,
                        -1);
    
    g_free (userid);
    g_free (keyid);
}

/* Appends @key and all uids */
static void
key_store_key_added (CryptUIKeyset *ckset, const gchar *key, CryptUIKeyStore *ckstore)
{
    GtkTreeIter iter;

    key_store_row_add (ckstore, key, &iter);
    key_store_set (ckstore, key, &iter);
}

static void
key_store_key_changed (CryptUIKeyset *ckset, const gchar *key, 
                       GtkTreeRowReference *ref, CryptUIKeyStore *ckstore)
{
    GtkTreeIter iter;
    GtkTreePath *path;

    g_return_if_fail (ref != NULL);
    
    path = gtk_tree_row_reference_get_path (ref);
    if (path) {
        if (gtk_tree_model_get_iter (GTK_TREE_MODEL (ckstore->priv->store), &iter, path))
            key_store_set (ckstore, key, &iter);
        gtk_tree_path_free (path);
    }
}

static void
key_store_key_removed (CryptUIKeyset *ckset, const gchar *key, 
                       GtkTreeRowReference *ref, CryptUIKeyStore *ckstore)
{
    GtkTreeIter iter;
    GtkTreePath *path;
    
    g_return_if_fail (ref != NULL);
    
    path = gtk_tree_row_reference_get_path (ref);
    if (path) {
        if(gtk_tree_model_get_iter (GTK_TREE_MODEL (ckstore->priv->store), &iter, path))
            gtk_tree_store_remove (GTK_TREE_STORE (ckstore->priv->store), &iter);
        gtk_tree_path_free (path);
    }
    
    /* Key is no more, free the closure */
    gtk_tree_row_reference_free (ref);
}

static void
key_store_populate (CryptUIKeyStore *ckstore)
{
    GList *keys, *l = NULL;
    
    /* Clear the store and then add all the keys */
    gtk_tree_store_clear (ckstore->priv->store);
    
    g_return_if_fail (CRYPTUI_IS_KEYSET (ckstore->ckset));
    keys = cryptui_keyset_get_keys (ckstore->ckset);
    
    for (l = keys; l; l = g_list_next (l)) {
        g_return_if_fail (l->data != NULL);
        key_store_key_added (ckstore->ckset, l->data, ckstore);
    }
     
    g_list_free (keys);
}

/* Given a treeview iter, get the base store iterator */
static void 
key_store_get_base_iter (CryptUIKeyStore* ckstore, const GtkTreeIter* iter, 
                         GtkTreeIter* base_iter)
{
    GtkTreeIter i;
    
    g_return_if_fail (CRYPTUI_IS_KEY_STORE (ckstore));
    g_assert (ckstore->priv->store && ckstore->priv->filter);
    
    gtk_tree_model_sort_convert_iter_to_child_iter (GTK_TREE_MODEL_SORT (ckstore), &i, (GtkTreeIter*)iter);
    gtk_tree_model_filter_convert_iter_to_child_iter (ckstore->priv->filter, base_iter, &i);
}

/* Given an iterator find the associated key */
static const gchar*
key_from_iterator (GtkTreeModel* model, GtkTreeIter* iter)
{
    GtkTreeIter i;
    const gchar *key;
    
    /* Convert to base iter if necessary */
    if (CRYPTUI_IS_KEY_STORE (model)) {
        CryptUIKeyStore* ckstore = key_store_from_model (model);
        key_store_get_base_iter (ckstore, iter, &i);
        
        iter = &i;
        model = GTK_TREE_MODEL (ckstore->priv->store);
    }
    
    gtk_tree_model_get (model, iter, CRYPTUI_KEY_STORE_KEY, &key, -1);
    return key;
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
    gtk_tree_model_get (model, iter, CRYPTUI_KEY_STORE_NAME, &name, CRYPTUI_KEY_STORE_KEYID, &id, -1);
    
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
filter_callback (GtkTreeModel *model, GtkTreeIter *iter, CryptUIKeyStore *ckstore)
{
    gboolean ret = FALSE;
    
    /* Check the row requested */
    switch (ckstore->priv->filter_mode) {
    case CRYPTUI_KEY_STORE_MODE_FILTERED:
        ret = row_contains_filtered_text (model, iter, ckstore->priv->filter_text);
        break;
        
    case CRYPTUI_KEY_STORE_MODE_SELECTED:
        if (ckstore->priv->use_check)
            gtk_tree_model_get (model, iter, CRYPTUI_KEY_STORE_CHECK, &ret, -1); 
        break;
        
    case CRYPTUI_KEY_STORE_MODE_ALL:
        ret = TRUE;
        break;
        
    default:
        g_assert_not_reached ();
        break;
    };

    /* Note that we never have children, no need to filter them */
    return ret;        
}

/* Refilter the tree */
static gboolean
refilter_now (CryptUIKeyStore* ckstore)
{
    cryptui_keyset_refresh (ckstore->ckset);
    gtk_tree_model_filter_refilter (ckstore->priv->filter);    
    ckstore->priv->filter_stag = 0;
    return FALSE;
}

/* Refilter the tree after a timeout has passed */
static void
refilter_later (CryptUIKeyStore* ckstore)
{
    if (ckstore->priv->filter_stag != 0)
        g_source_remove (ckstore->priv->filter_stag);
    ckstore->priv->filter_stag = g_timeout_add (200, (GSourceFunc)refilter_now, ckstore);
}

/* -----------------------------------------------------------------------------
 * OBJECT 
 */

static void
cryptui_key_store_init (CryptUIKeyStore *ckstore)
{
    /* init private vars */
    ckstore->priv = g_new0 (CryptUIKeyStorePriv, 1);

    /* The base store */
    ckstore->priv->store = gtk_tree_store_newv (CRYPTUI_KEY_STORE_NCOLS, (GType*)col_types);
    
    /* The filtering model */
    ckstore->priv->filter = GTK_TREE_MODEL_FILTER (gtk_tree_model_filter_new (GTK_TREE_MODEL (ckstore->priv->store), NULL));
    gtk_tree_model_filter_set_visible_func (ckstore->priv->filter, (GtkTreeModelFilterVisibleFunc)filter_callback, ckstore, NULL);
}

static GObject*  
cryptui_key_store_constructor (GType type, guint n_props, GObjectConstructParam* props)
{
    GObject *obj = G_OBJECT_CLASS (cryptui_key_store_parent_class)->constructor (type, n_props, props);
    CryptUIKeyStore *ckstore = CRYPTUI_KEY_STORE (obj);

    /* Hookup to the current object */
    g_object_set (ckstore, "model", ckstore->priv->filter, NULL);

    key_store_populate (ckstore);

    return obj;
}

/* dispose of all our internal references */
static void
cryptui_key_store_dispose (GObject *gobject)
{
    CryptUIKeyStore *ckstore = CRYPTUI_KEY_STORE (gobject);  
    
    /*
     * Note that after this executes the rest of the object should
     * still work without a segfault. This basically nullifies the 
     * object, but doesn't free it.
     * 
     * This function should also be able to run multiple times.
     */

    if (ckstore->ckset) {
        g_signal_handlers_disconnect_by_func (ckstore->ckset, key_store_key_added, ckstore);
        g_signal_handlers_disconnect_by_func (ckstore->ckset, key_store_key_removed, ckstore);
        g_signal_handlers_disconnect_by_func (ckstore->ckset, key_store_key_changed, ckstore);
        g_object_unref (ckstore->ckset);        
        ckstore->ckset = NULL;
    }
    
    G_OBJECT_CLASS (cryptui_key_store_parent_class)->dispose (gobject);
}

static void
cryptui_key_store_finalize (GObject *gobject)
{
    CryptUIKeyStore *ckstore = CRYPTUI_KEY_STORE (gobject);
    
    g_assert (ckstore->ckset == NULL);
    
    /* These were allocated in the constructor */
    g_object_unref (ckstore->priv->store);
    g_object_unref (ckstore->priv->filter);
     
    /* Allocated in property setter */
    g_free (ckstore->priv->filter_text); 
    
    G_OBJECT_CLASS (cryptui_key_store_parent_class)->finalize (gobject);
}

static void
cryptui_key_store_set_property (GObject *gobject, guint prop_id,
                                const GValue *value, GParamSpec *pspec)
{
    CryptUIKeyStore *ckstore;
    const gchar* t;

    ckstore = CRYPTUI_KEY_STORE (gobject);
    
    switch (prop_id) {
    case PROP_KEYSET:
        g_return_if_fail (ckstore->ckset == NULL);
        ckstore->ckset = g_value_get_object (value);
        g_object_ref (ckstore->ckset);
        g_signal_connect_after (ckstore->ckset, "added",
                    G_CALLBACK (key_store_key_added), ckstore);
        g_signal_connect_after (ckstore->ckset, "removed",
                    G_CALLBACK (key_store_key_removed), ckstore);
        g_signal_connect_after (ckstore->ckset, "changed",
                    G_CALLBACK (key_store_key_changed), ckstore);
        break;
        
    /* The filtering mode */
    case PROP_MODE:
        if (ckstore->priv->filter_mode != g_value_get_uint (value)) {
            ckstore->priv->filter_mode = g_value_get_uint (value);
            refilter_later (ckstore);
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
        if ((ckstore->priv->filter_mode != CRYPTUI_KEY_STORE_MODE_FILTERED && t && t[0]) ||
            (ckstore->priv->filter_mode == CRYPTUI_KEY_STORE_MODE_FILTERED)) {
            ckstore->priv->filter_mode = CRYPTUI_KEY_STORE_MODE_FILTERED;
            g_free (ckstore->priv->filter_text);
                
            /* We always use lower case text (see filter_callback) */
            ckstore->priv->filter_text = g_utf8_strdown (t, -1);
            refilter_later (ckstore);
        }
        break;
        
    case PROP_USE_CHECK:
        ckstore->priv->use_check = g_value_get_boolean (value);
        break;
    
    default:
        break;
    }
}

static void
cryptui_key_store_get_property (GObject *gobject, guint prop_id,
                                GValue *value, GParamSpec *pspec)
{
    CryptUIKeyStore *ckstore;

    ckstore = CRYPTUI_KEY_STORE (gobject);

    switch (prop_id) {
    case PROP_KEYSET:
        g_value_set_object (value, ckstore->ckset);
        break;
        
    /* The filtering mode */
    case PROP_MODE:
        g_value_set_uint (value, ckstore->priv->filter_mode);
        break;
    
    /* The filter text. Note that we act as if we don't have any 
     * filter text when not in filtering mode */
    case PROP_FILTER:
        g_value_set_string (value, 
            ckstore->priv->filter_mode == CRYPTUI_KEY_STORE_MODE_FILTERED ? ckstore->priv->filter_text : "");
        break;
    
    case PROP_USE_CHECK:
        g_value_set_boolean (value, ckstore->priv->use_check);
        break;
        
    default:
        break;
    }
}

static void
cryptui_key_store_class_init (CryptUIKeyStoreClass *klass)
{
    GObjectClass *gobject_class;

    cryptui_key_store_parent_class = g_type_class_peek_parent (klass);
    gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->constructor = cryptui_key_store_constructor;
    gobject_class->dispose = cryptui_key_store_dispose;
    gobject_class->finalize = cryptui_key_store_finalize;
    gobject_class->set_property = cryptui_key_store_set_property;
    gobject_class->get_property = cryptui_key_store_get_property;
    
    g_object_class_install_property (gobject_class, PROP_KEYSET,
        g_param_spec_object ("keyset", "CryptUI Keyset", "Current CryptUI Key Source to use",
                             CRYPTUI_TYPE_KEYSET, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
                    
    g_object_class_install_property (gobject_class, PROP_MODE,
        g_param_spec_uint ("mode", "Key Store Mode", "Key store mode controls which keys to display",
                           0, CRYPTUI_KEY_STORE_MODE_FILTERED, CRYPTUI_KEY_STORE_MODE_ALL, G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class, PROP_FILTER,
        g_param_spec_string ("filter", "Key Store Filter", "Key store filter for when in filtered mode",
                             "", G_PARAM_READWRITE));
                             
    g_object_class_install_property (gobject_class, PROP_USE_CHECK,
        g_param_spec_boolean ("use-check", "Use check boxes", "Use check box column to denote selection",
                              FALSE, G_PARAM_READWRITE));
                              
}


/* -----------------------------------------------------------------------------
 * PUBLIC 
 */

CryptUIKeyStore*
cryptui_key_store_new (CryptUIKeyset *keyset, gboolean use_check)
{
    GObject *obj = g_object_new (CRYPTUI_TYPE_KEY_STORE, "keyset", keyset, "use-check", use_check, NULL);
    return (CryptUIKeyStore*)obj;
}

void
cryptui_key_store_check_toggled (CryptUIKeyStore *ckstore, GtkTreeView *view, gchar *path)
{
    GtkTreeModel* fmodel = GTK_TREE_MODEL (ckstore);
    GtkTreeSelection *selection;
    gboolean prev = FALSE;
    GtkTreeIter iter;
    GtkTreeIter base;
    GValue v;

    memset (&v, 0, sizeof(v));
    g_return_if_fail (path != NULL);
    g_return_if_fail (gtk_tree_model_get_iter_from_string (fmodel, &iter, path));
    
    /* We get notified in filtered coordinates, we have to convert those to base */
    key_store_get_base_iter (ckstore, &iter, &base);
 
    gtk_tree_model_get_value (GTK_TREE_MODEL (ckstore), &base, CRYPTUI_KEY_STORE_CHECK, &v);
    if(G_VALUE_TYPE (&v) == G_TYPE_BOOLEAN)
        prev = g_value_get_boolean (&v);
    g_value_unset (&v);    
    
    gtk_tree_store_set (GTK_TREE_STORE (ckstore), &base, CRYPTUI_KEY_STORE_CHECK, prev ? FALSE : TRUE, -1);

    selection = gtk_tree_view_get_selection (view);
    g_signal_emit_by_name (selection, "changed");
}

gboolean
cryptui_key_store_get_iter_from_key (CryptUIKeyStore *ckstore, const gchar *key,
                                     GtkTreeIter *iter)
{
    GtkTreeRowReference *ref;
    GtkTreePath *path;
    gboolean ret = FALSE;
    
    g_return_val_if_fail (CRYPTUI_IS_KEY_STORE (ckstore), FALSE);
    g_return_val_if_fail (iter != NULL, FALSE);
    
    /* TODO: When key is NULL return none_option if it exists */
    
    ref = (GtkTreeRowReference*)cryptui_keyset_get_closure (ckstore->ckset, key);
    path = gtk_tree_row_reference_get_path (ref);
    if (path) {
        if (gtk_tree_model_get_iter (GTK_TREE_MODEL (ckstore->priv->store), iter, path))
            ret = TRUE;
        gtk_tree_path_free (path);
    }
    
    return ret;
}

const gchar*
cryptui_key_store_get_key_from_iter (CryptUIKeyStore *ckstore, GtkTreeIter *iter)
{
    const gchar *key;

    g_return_val_if_fail (CRYPTUI_IS_KEY_STORE (ckstore), FALSE);
    g_return_val_if_fail (iter != NULL, FALSE);
    
    gtk_tree_model_get (GTK_TREE_MODEL (ckstore->priv->store), iter, 
                        CRYPTUI_KEY_STORE_KEY, &key, -1);
    return key;
}

const gchar*
cryptui_key_store_get_key_from_path (CryptUIKeyStore *ckstore, GtkTreePath *path)
{
    GtkTreeIter iter;

    g_return_val_if_fail (CRYPTUI_IS_KEY_STORE (ckstore), NULL);
    g_return_val_if_fail (path != NULL, NULL);

    g_return_val_if_fail (gtk_tree_model_get_iter (GTK_TREE_MODEL (ckstore), &iter, path), NULL);
    
    return key_from_iterator (GTK_TREE_MODEL (ckstore), &iter);
}

GList*
cryptui_key_store_get_all_keys (CryptUIKeyStore *ckstore)
{
    g_return_val_if_fail (CRYPTUI_KEY_STORE (ckstore), NULL);
    return cryptui_keyset_get_keys (ckstore->ckset);
}

GList*
cryptui_key_store_get_selected_keys (CryptUIKeyStore *ckstore, GtkTreeView *view)
{
    GList *l, *keys = NULL;
    
    g_return_val_if_fail (CRYPTUI_IS_KEY_STORE (ckstore), NULL);
    g_return_val_if_fail (GTK_IS_TREE_VIEW (view), NULL);
    
    if (ckstore->priv->use_check) {
        GtkTreeModel* model = GTK_TREE_MODEL (ckstore);
        GtkTreeIter iter;
        gboolean check;
            
        if (gtk_tree_model_get_iter_first (model, &iter)) {
            do {
                check = FALSE;
                gtk_tree_model_get (model, &iter, CRYPTUI_KEY_STORE_CHECK, &check, -1); 
                if (check)
                    keys = g_list_append (keys, (gchar*)key_from_iterator (model, &iter));
            } while (gtk_tree_model_iter_next (model, &iter));
        }
    }
    
    /* Fall back if none checked, or not using checks */
    if (keys == NULL) {
        GList *list, *paths = NULL;
        GtkTreeSelection *selection;
        
        selection = gtk_tree_view_get_selection (view);
        paths = gtk_tree_selection_get_selected_rows (selection, NULL);
    
        /* Make key list */
        for (list = paths; list != NULL; list = g_list_next (list))
            keys = g_list_append (keys, (gchar*)cryptui_key_store_get_key_from_path (ckstore, list->data));
            
        /* free selected paths */
        g_list_foreach (paths, (GFunc)gtk_tree_path_free, NULL);
        g_list_free (paths);
    }
    
    /* Remove duplicates */
    keys = g_list_sort (keys, compare_pointers);
    for (l = keys; l; l = g_list_next (l)) {
        while (l->next && strcmp ((const gchar*)l->data, (const gchar*)l->next->data) == 0)
            keys = g_list_delete_link (keys, l->next);
    }    
    
    return keys;
}

const gchar*
cryptui_key_store_get_selected_key (CryptUIKeyStore *ckstore, GtkTreeView *view)
{
    const gchar *key = NULL;

    g_return_val_if_fail (CRYPTUI_IS_KEY_STORE (ckstore), NULL);
    g_return_val_if_fail (GTK_IS_TREE_VIEW (view), NULL);
    
    if (ckstore->priv->use_check) {
        GtkTreeModel* model = GTK_TREE_MODEL (ckstore->priv->store);
        GtkTreeIter iter;
        gboolean check;
            
        if (gtk_tree_model_get_iter_first (model, &iter)) {
            do {
                check = FALSE;
                gtk_tree_model_get (model, &iter, CRYPTUI_KEY_STORE_CHECK, &check, -1); 
                if (check) {
                    key = key_from_iterator (model, &iter);
                    break;
                }
            } while (gtk_tree_model_iter_next (model, &iter));
        }
    }

    /* Fall back if none checked, or not using checks */
    if (key == NULL) {
        GList *paths = NULL;
        GtkTreeSelection *selection;
        
        selection = gtk_tree_view_get_selection (view);
        paths = gtk_tree_selection_get_selected_rows (selection, NULL);
    
        /* Make key list */
        if (paths != NULL)
            key = cryptui_key_store_get_key_from_path (ckstore, paths->data);
            
        /* free selected paths */
        g_list_foreach (paths, (GFunc)gtk_tree_path_free, NULL);
        g_list_free (paths);
    }
    
    return key;
}
