/*
 * Seahorse
 *
 * Copyright (C) 2005 Nate Nielsen
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
    PROP_SEARCH,
    PROP_USE_CHECKS,
    PROP_NONE_OPTION
};

/* This should always match the list in cryptui-keystore.h */
static const GType col_types[] = {
    G_TYPE_STRING,
    G_TYPE_STRING,
    G_TYPE_BOOLEAN,
    G_TYPE_BOOLEAN,
    G_TYPE_STRING,
    G_TYPE_BOOLEAN,
    G_TYPE_POINTER
};

struct _CryptUIKeyStorePriv {
    gboolean                    initialized;
    
    GHashTable                  *rows;
    GtkTreeModelFilter          *filter;
    GtkTreeStore                *store;
    
    /* For text searches */
    CryptUIKeyStoreMode         filter_mode;
    gchar                       *search_text;
    guint                       filter_stag;
    
    /* Custom filtering */
    CryptUIKeyStoreFilterFunc   filter_func;
    gpointer                    filter_data;
    
    gboolean                    use_checks;
    gchar                       *none_option;
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

gboolean    
hashtable_remove_all (gpointer key, gpointer value, gpointer user_data)
{
    return TRUE;
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
    ref = (GtkTreeRowReference*)g_hash_table_lookup (ckstore->priv->rows, key);
    g_return_if_fail (ref == NULL);

    gtk_tree_store_append (GTK_TREE_STORE (ckstore->priv->store), iter, NULL);
    path = gtk_tree_model_get_path (GTK_TREE_MODEL (ckstore->priv->store), iter);
    ref = gtk_tree_row_reference_new (GTK_TREE_MODEL (ckstore->priv->store), path);
    gtk_tree_path_free (path);
    
    g_hash_table_replace (ckstore->priv->rows, (gchar*)key, ref);
    gtk_tree_store_set (GTK_TREE_STORE (ckstore->priv->store), iter, CRYPTUI_KEY_STORE_KEY, key, -1);
}

/* Sets Name and KeyID */
static void
key_store_set (CryptUIKeyStore *ckstore, const gchar *key, GtkTreeIter *iter)
{
    gchar *userid, *keyid;
    gboolean sec;
    
    cryptui_keyset_cache_key (ckstore->ckset, key);
    
    userid = cryptui_keyset_key_display_name (ckstore->ckset, key);
    keyid = cryptui_keyset_key_display_id (ckstore->ckset, key);
    sec = cryptui_key_get_enctype (key) == CRYPTUI_ENCTYPE_PRIVATE;
    
    gtk_tree_store_set (ckstore->priv->store, iter,
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
    
    /* Key is no more, free the reference */
    g_hash_table_remove (ckstore->priv->rows, key);
}

static void
key_store_populate (CryptUIKeyStore *ckstore)
{
    GList *keys, *l = NULL;
    
    if (!ckstore->priv->initialized)
        return;
    
    /* Clear the store and then add all the keys */
    gtk_tree_store_clear (ckstore->priv->store);
    g_hash_table_foreach_remove (ckstore->priv->rows, hashtable_remove_all, NULL);
    
    /* Add the none option */
    if (ckstore->priv->none_option) {
        GtkTreeIter iter;
        
        /* Second row is a separator */
        gtk_tree_store_prepend (ckstore->priv->store, &iter, NULL);
        gtk_tree_store_set (ckstore->priv->store, &iter, 
                        CRYPTUI_KEY_STORE_NAME, NULL,
                        CRYPTUI_KEY_STORE_SEPARATOR, TRUE,
                        -1);
        
        /* The first row is the none option */
        gtk_tree_store_prepend (ckstore->priv->store, &iter, NULL);
        gtk_tree_store_set (ckstore->priv->store, &iter,
                        CRYPTUI_KEY_STORE_NAME, ckstore->priv->none_option,
                        CRYPTUI_KEY_STORE_KEY, NULL, 
                        -1);
    }

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
key_store_get_base_iter (CryptUIKeyStore *ckstore, const GtkTreeIter *iter, 
                         GtkTreeIter *base_iter)
{
    GtkTreeIter i;
    gtk_tree_model_sort_convert_iter_to_child_iter (GTK_TREE_MODEL_SORT (ckstore), &i, (GtkTreeIter*)iter);
    gtk_tree_model_filter_convert_iter_to_child_iter (ckstore->priv->filter, base_iter, &i);
}

/* Given a base iter get the treeview iter */
static void
key_store_get_view_iter (CryptUIKeyStore *ckstore, const GtkTreeIter *base,
                         GtkTreeIter *iter)
{
    GtkTreeIter i;
    gtk_tree_model_filter_convert_child_iter_to_iter (ckstore->priv->filter, &i, (GtkTreeIter*)base);
    gtk_tree_model_sort_convert_child_iter_to_iter (GTK_TREE_MODEL_SORT (ckstore), iter, &i);
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
    const gchar *key = NULL;
    gboolean ret = FALSE;
    
    /* Special rows (such as the none-option, separators) are always visible */
    gtk_tree_model_get (model, iter, CRYPTUI_KEY_STORE_KEY, &key, -1);
    if (!key)
        return TRUE;
    
    /* First the custom filter */
    if (ckstore->priv->filter_func)
        if (!(ckstore->priv->filter_func) (ckstore->ckset, key, ckstore->priv->filter_data))
            return FALSE;
    
    switch (ckstore->priv->filter_mode) {
        
    /* Search for specified text */
    case CRYPTUI_KEY_STORE_MODE_RESULTS:
        return row_contains_filtered_text (model, iter, ckstore->priv->search_text);
        
    /* Anything checked off */
    case CRYPTUI_KEY_STORE_MODE_SELECTED:
        if (ckstore->priv->use_checks)
            gtk_tree_model_get (model, iter, CRYPTUI_KEY_STORE_CHECK, &ret, -1); 
        else
            ret = TRUE;
        return ret;
        
    /* And all the keys */
    case CRYPTUI_KEY_STORE_MODE_ALL:
        return TRUE;
        
    default:
        g_assert_not_reached ();
        return FALSE;
    };
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

gint        
sort_default_comparator (GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b,
                         gpointer user_data)
{
    const gchar *keya, *keyb;
    gchar *namea, *nameb;
    
    /* By default we sort by:
     *  - Presence of key pointer (to keep none-option at top)
     *  - Name 
     */
    
    gtk_tree_model_get (model, a, CRYPTUI_KEY_STORE_KEY, &keya, 
                        CRYPTUI_KEY_STORE_NAME, &namea, -1);
    gtk_tree_model_get (model, b, CRYPTUI_KEY_STORE_KEY, &keyb, 
                        CRYPTUI_KEY_STORE_NAME, &nameb, -1);
    
    /* This somewhat strage set of checks keep the none-option,
       separator, and keys in proper order */
    if (!keya && keyb)
        return -1;
    else if (!keyb && keya)
        return 1;
    else if (!namea && nameb)
        return 1;
    else if (!nameb && namea)
        return -1;
    else if (!keya && !keyb)
        return 0;
    else if (!namea && !nameb)
        return 0;

    return g_utf8_collate (namea, nameb);
    
}

/* -----------------------------------------------------------------------------
 * OBJECT 
 */

static void
cryptui_key_store_init (CryptUIKeyStore *ckstore)
{
    /* init private vars */
    ckstore->priv = g_new0 (CryptUIKeyStorePriv, 1);
    
    /* Our key -> row ref mapping */
    ckstore->priv->rows = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, 
                                                 (GDestroyNotify)gtk_tree_row_reference_free);

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

    /* Default sort function */
    gtk_tree_sortable_set_default_sort_func (GTK_TREE_SORTABLE (ckstore), sort_default_comparator, NULL, NULL);
    
    ckstore->priv->initialized = TRUE;
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
    
    g_hash_table_foreach_remove (ckstore->priv->rows, hashtable_remove_all, NULL);
    
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
    g_hash_table_destroy (ckstore->priv->rows);
     
    /* Allocated in property setter */
    g_free (ckstore->priv->search_text); 
    g_free (ckstore->priv->none_option);
    
    g_free (ckstore->priv);
    
    G_OBJECT_CLASS (cryptui_key_store_parent_class)->finalize (gobject);
}

static void
cryptui_key_store_set_property (GObject *gobject, guint prop_id,
                                const GValue *value, GParamSpec *pspec)
{
    CryptUIKeyStore *ckstore = CRYPTUI_KEY_STORE (gobject);
    
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
        cryptui_key_store_set_search_mode (ckstore, g_value_get_uint (value));
        break;
        
    /* The search text */
    case PROP_SEARCH:
        cryptui_key_store_set_search_text (ckstore, g_value_get_string (value));
        break; 
    
    case PROP_USE_CHECKS:
        ckstore->priv->use_checks = g_value_get_boolean (value);
        break;
    
    case PROP_NONE_OPTION:
        g_free (ckstore->priv->none_option);
        ckstore->priv->none_option = g_strdup (g_value_get_string (value));
        key_store_populate (ckstore);
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
    
    /* The search text. Note that we act as if we don't have any 
     * filter text when not in filtering mode */
    case PROP_SEARCH:
        g_value_set_string (value, 
            ckstore->priv->filter_mode == CRYPTUI_KEY_STORE_MODE_RESULTS ? ckstore->priv->search_text : "");
        break;
    
    case PROP_USE_CHECKS:
        g_value_set_boolean (value, ckstore->priv->use_checks);
        break;
    
    case PROP_NONE_OPTION:
        g_value_set_string (value, ckstore->priv->none_option);
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
                           0, CRYPTUI_KEY_STORE_MODE_RESULTS, CRYPTUI_KEY_STORE_MODE_ALL, G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class, PROP_SEARCH,
        g_param_spec_string ("search", "Search text", "Key store search text for when in results mode",
                             "", G_PARAM_READWRITE));
                             
    g_object_class_install_property (gobject_class, PROP_USE_CHECKS,
        g_param_spec_boolean ("use-checks", "Use check boxes", "Use check box column to denote selection",
                              FALSE, G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class, PROP_NONE_OPTION,
        g_param_spec_string ("none-option", "Option for 'No Selection'", "Text for row that denotes 'No Selection'",
                              "", G_PARAM_READWRITE));
}

/* -----------------------------------------------------------------------------
 * PUBLIC 
 */

CryptUIKeyStore*
cryptui_key_store_new (CryptUIKeyset *keyset, gboolean use_checks, 
                       const gchar *none_option)
{
    GObject *obj = g_object_new (CRYPTUI_TYPE_KEY_STORE, "keyset", keyset, 
                                 "use-checks", use_checks, "none-option", none_option, NULL);
    return (CryptUIKeyStore*)obj;
}

void
cryptui_key_store_check_toggled (CryptUIKeyStore *ckstore, GtkTreeView *view, GtkTreeIter *iter)
{
    GtkTreeSelection *selection;
    gboolean prev = FALSE;
    GtkTreeIter base;
    GValue v;

    memset (&v, 0, sizeof(v));
    g_return_if_fail (iter != NULL);
    
    /* We get notified in filtered coordinates, we have to convert those to base */
    key_store_get_base_iter (ckstore, iter, &base);
 
    gtk_tree_model_get_value (GTK_TREE_MODEL (ckstore->priv->store), &base, CRYPTUI_KEY_STORE_CHECK, &v);
    if(G_VALUE_TYPE (&v) == G_TYPE_BOOLEAN)
        prev = g_value_get_boolean (&v);
    g_value_unset (&v);    
    
    gtk_tree_store_set (GTK_TREE_STORE (ckstore->priv->store), &base, CRYPTUI_KEY_STORE_CHECK, prev ? FALSE : TRUE, -1);

    selection = gtk_tree_view_get_selection (view);
    g_signal_emit_by_name (selection, "changed");
}

gboolean
cryptui_key_store_get_iter_from_key (CryptUIKeyStore *ckstore, const gchar *key,
                                     GtkTreeIter *iter)
{
    GtkTreeRowReference *ref;
    GtkTreePath *path;
    GtkTreeIter base;
    gboolean ret = FALSE;
    
    g_return_val_if_fail (CRYPTUI_IS_KEY_STORE (ckstore), FALSE);
    g_return_val_if_fail (iter != NULL, FALSE);
    
    if (key == NULL) {
        /* The none option for NULL */
        if (ckstore->priv->none_option) {
            if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (ckstore->priv->store), &base)) {
                key_store_get_view_iter (ckstore, &base, iter);
                return TRUE;
            }
        }
        
        return FALSE;
    }
    
    ref = (GtkTreeRowReference*)g_hash_table_lookup (ckstore->priv->rows, key);
    path = gtk_tree_row_reference_get_path (ref);
    if (path) {
        if (gtk_tree_model_get_iter (GTK_TREE_MODEL (ckstore->priv->store), &base, path)) {
            key_store_get_view_iter (ckstore, &base, iter);
            ret = TRUE;
        }
        gtk_tree_path_free (path);
    }
    
    return ret;
}

const gchar*
cryptui_key_store_get_key_from_iter (CryptUIKeyStore *ckstore, GtkTreeIter *iter)
{
    g_return_val_if_fail (CRYPTUI_IS_KEY_STORE (ckstore), FALSE);
    g_return_val_if_fail (iter != NULL, FALSE);
    
    /* The none option will automatically have NULL as it's key pointer */
    return key_from_iterator (GTK_TREE_MODEL (ckstore), iter);
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
    
    if (ckstore->priv->use_checks) {
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

void
cryptui_key_store_set_selected_keys (CryptUIKeyStore *ckstore, GtkTreeView *view,
                                    GList *keys)
{
    GtkTreeModel* model = GTK_TREE_MODEL (ckstore->priv->store);
    GtkTreeSelection *sel;
    GHashTable *keyset;
    GtkTreeIter iter;
    const gchar *key;
    gboolean have;
    
    g_return_if_fail (CRYPTUI_IS_KEY_STORE (ckstore));
    g_return_if_fail (GTK_IS_TREE_VIEW (view));
    
    sel = gtk_tree_view_get_selection (view);
    
    /* A quick lookup table for the keys */
    keyset = g_hash_table_new (g_str_hash, g_str_equal);
    for (; keys; keys = g_list_next (keys)) 
        g_hash_table_insert (keyset, keys->data, GINT_TO_POINTER (TRUE));

    /* Go through all rows and select deselect as necessary */
    if (gtk_tree_model_get_iter_first (model, &iter)) {
        do {
            
            /* Is this row in our selection? */
            gtk_tree_model_get (model, &iter, CRYPTUI_KEY_STORE_KEY, &key, -1);
            have = (key && g_hash_table_lookup (keyset, key) != NULL) ? TRUE : FALSE;
            
            /* Using checks so change data store */
            if (ckstore->priv->use_checks)
                gtk_tree_store_set (ckstore->priv->store, &iter, CRYPTUI_KEY_STORE_CHECK, have, -1);
                
            /* Using normal selection */
            else if (have)
                gtk_tree_selection_select_iter (sel, &iter);
            else
                gtk_tree_selection_unselect_iter (sel, &iter);
            
        } while (gtk_tree_model_iter_next (model, &iter));
    }
    
    g_hash_table_destroy (keyset);
}

const gchar*
cryptui_key_store_get_selected_key (CryptUIKeyStore *ckstore, GtkTreeView *view)
{
    const gchar *key = NULL;

    g_return_val_if_fail (CRYPTUI_IS_KEY_STORE (ckstore), NULL);
    g_return_val_if_fail (GTK_IS_TREE_VIEW (view), NULL);
    
    if (ckstore->priv->use_checks) {
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

void
cryptui_key_store_set_selected_key (CryptUIKeyStore *ckstore, GtkTreeView *view,
                                    const gchar *selkey)
{
    GtkTreeModel* model = GTK_TREE_MODEL (ckstore->priv->store);
    GtkTreeSelection *sel;
    GtkTreeIter iter;
    const gchar *key;
    gboolean have;
    
    g_return_if_fail (CRYPTUI_IS_KEY_STORE (ckstore));
    g_return_if_fail (GTK_IS_TREE_VIEW (view));
    
    sel = gtk_tree_view_get_selection (view);
    
    /* Go through all rows and select deselect as necessary */
    if (gtk_tree_model_get_iter_first (model, &iter)) {
        do {
            
            /* Is this row in our selection? */
            gtk_tree_model_get (model, &iter, CRYPTUI_KEY_STORE_KEY, &key, -1);
            have = (key && strcmp (selkey, key) == 0) ? TRUE : FALSE;
            
            /* Using checks so change data store */
            if (ckstore->priv->use_checks)
                gtk_tree_store_set (ckstore->priv->store, &iter, CRYPTUI_KEY_STORE_CHECK, have, -1);
                
            /* Using normal selection */
            else if (have)
                gtk_tree_selection_select_iter (sel, &iter);
            else
                gtk_tree_selection_unselect_iter (sel, &iter);
            
        } while (gtk_tree_model_iter_next (model, &iter));
    }
}

void
cryptui_key_store_set_search_mode (CryptUIKeyStore *ckstore, CryptUIKeyStoreMode mode)
{
    if (ckstore->priv->filter_mode != mode) {
        ckstore->priv->filter_mode = mode;
        refilter_later (ckstore);
    }
}

void
cryptui_key_store_set_search_text (CryptUIKeyStore *ckstore, const gchar *search_text)
{
    /* 
     * If we're not in filtered mode and there is text OR
     * we're in results mode (regardless of text or not)
     * then update the results
     */
    if ((ckstore->priv->filter_mode != CRYPTUI_KEY_STORE_MODE_RESULTS && 
         search_text && search_text[0]) ||
        (ckstore->priv->filter_mode == CRYPTUI_KEY_STORE_MODE_RESULTS)) {
        ckstore->priv->filter_mode = CRYPTUI_KEY_STORE_MODE_RESULTS;
        g_free (ckstore->priv->search_text);
            
        /* We always use lower case text (see filter_callback) */
        ckstore->priv->search_text = g_utf8_strdown (search_text, -1);
        refilter_later (ckstore);
    }
}

void
cryptui_key_store_set_filter (CryptUIKeyStore *ckstore, CryptUIKeyStoreFilterFunc func,
                              gpointer user_data)
{
    ckstore->priv->filter_func = func;
    ckstore->priv->filter_data = user_data;
    refilter_later (ckstore);
}
