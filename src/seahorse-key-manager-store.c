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

#include <string.h>
#include <unistd.h>

#include <glib/gi18n.h>

#include "seahorse-key-manager-store.h"
#include "seahorse-preferences.h"
#include "seahorse-validity.h"
#include "seahorse-util.h"
#include "seahorse-gconf.h"
#include "eggtreemultidnd.h"

#include "pgp/seahorse-pgp-key.h"

#ifdef WITH_SSH
#include "ssh/seahorse-ssh-key.h"
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

enum {
	DRAG_INFO_TEXT,
	DRAG_INFO_XDS,
};

#define XDS_FILENAME "xds.txt"
#define MAX_XDS_ATOM_VAL_LEN 4096
#define XDS_ATOM   gdk_atom_intern  ("XdndDirectSave0", FALSE)
#define TEXT_ATOM  gdk_atom_intern  ("text/plain", FALSE)

static GtkTargetEntry store_targets[] = {
	{ "text/plain", 0, DRAG_INFO_TEXT },
	{ "XdndDirectSave0", 0, DRAG_INFO_XDS }
};

struct _SeahorseKeyManagerStorePriv {    
    GtkTreeModelFilter      *filter;
    GtkTreeModelSort        *sort;
    
    SeahorseKeyManagerStoreMode    filter_mode;
    gchar*                  filter_text;
    guint                   filter_stag;
    
	gchar *drag_destination;
	GError *drag_error;
	GList *drag_keys;
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

/* The following three functions taken from bugzilla
 * (http://bugzilla.gnome.org/attachment.cgi?id=49362&action=view)
 * Author: Christian Neumair
 * Copyright: 2005 Free Software Foundation, Inc
 * License: GPL */
static char *
xds_get_atom_value (GdkDragContext *context)
{
	char *ret;

	g_return_val_if_fail (context != NULL, NULL);
	g_return_val_if_fail (context->source_window != NULL, NULL);

	gdk_property_get (context->source_window,
			  XDS_ATOM, TEXT_ATOM,
			  0, MAX_XDS_ATOM_VAL_LEN,
			  FALSE, NULL, NULL, NULL,
			  (unsigned char **) &ret);

	return ret;
}

static gboolean
xds_context_offers_target (GdkDragContext *context, GdkAtom target)
{
	return (g_list_find (context->targets, target) != NULL);
}

static gboolean
xds_is_dnd_valid_context (GdkDragContext *context)
{
	char *tmp;
	gboolean ret;

	g_return_val_if_fail (context != NULL, FALSE);

	tmp = NULL;
	if (xds_context_offers_target (context, XDS_ATOM)) {
		tmp = xds_get_atom_value (context);
	}

	ret = (tmp != NULL);
	g_free (tmp);

	return ret;
}

static gboolean
drag_begin (GtkWidget *widget, GdkDragContext *context, SeahorseKeyManagerStore *skstore)
{
	GtkTreeView *view = GTK_TREE_VIEW (widget);
    
	DBG_PRINT (("drag_begin -->\n"));

	g_free (skstore->priv->drag_destination);
	skstore->priv->drag_destination = NULL;
	
	g_clear_error (&skstore->priv->drag_error);
	
	g_list_free (skstore->priv->drag_keys);
	skstore->priv->drag_keys = seahorse_key_manager_store_get_selected_keys (view);
	if (skstore->priv->drag_keys)
		gdk_property_change (context->source_window, XDS_ATOM, TEXT_ATOM, 
		                     8, GDK_PROP_MODE_REPLACE, (guchar*)XDS_FILENAME,
		                     strlen (XDS_FILENAME));
	
	DBG_PRINT (("drag_begin <--\n"));
	return skstore->priv->drag_keys ? TRUE : FALSE;
}

static gboolean
export_keys_to_output (GList *keys, GOutputStream *output, GError **error)
{
	SeahorseMultiOperation *mop = NULL;
	SeahorseOperation *op;
	SeahorseKeySource *sksrc;
	SeahorseKey *skey;
	GList *next;
	gboolean ret;
	
	keys = seahorse_util_keylist_sort (keys);
	DBG_PRINT (("exporting %d keys\n", g_list_length (keys)));
	
	while (keys) {
		
		/* Break off one set (same keysource) */
		next = seahorse_util_keylist_splice (keys);

		g_assert (SEAHORSE_IS_KEY (keys->data));
		skey = SEAHORSE_KEY (keys->data);

		/* Export from this key source */        
		sksrc = seahorse_key_get_source (skey);
		g_return_val_if_fail (sksrc != NULL, FALSE);

		if (!mop) 
			mop = seahorse_multi_operation_new ();

		/* We pass our own data object, to which data is appended */
		op = seahorse_key_source_export (sksrc, keys, FALSE, output);
		g_return_val_if_fail (op != NULL, FALSE);

		seahorse_multi_operation_take (mop, op);
		
		g_list_free (keys);
		keys = next;		
	}
	
	/* Make sure it's complete before we can return data */
	op = SEAHORSE_OPERATION (mop);
	seahorse_operation_wait (op);

	ret = TRUE;
	if (!seahorse_operation_is_successful (op)) { 
		seahorse_operation_copy_error (op, error);
		ret = FALSE;
	}
	
	g_object_unref (mop);
	return ret;
}

static gboolean
export_to_text (SeahorseKeyManagerStore *skstore, GtkSelectionData *selection_data)
{
	GOutputStream *output;
	gboolean ret;
	GList *keys;
	
	ret = FALSE;
	
	g_return_val_if_fail (skstore->priv->drag_keys, FALSE);
	keys = g_list_copy (skstore->priv->drag_keys);

	DBG_PRINT (("exporting to text\n"));

	output = g_memory_output_stream_new (NULL, 0, g_realloc, g_free);
	g_return_val_if_fail (output, FALSE);

	/* This modifies and frees keys */
	ret = export_keys_to_output (keys, output, &skstore->priv->drag_error) &&
	       g_output_stream_close (output, NULL, &skstore->priv->drag_error);
	if (ret) {
		DBG_PRINT (("setting selection text\n"));
		gtk_selection_data_set_text (selection_data, 
		                             g_memory_output_stream_get_data (G_MEMORY_OUTPUT_STREAM (output)), 
		                             seahorse_util_memory_output_length (G_MEMORY_OUTPUT_STREAM (output)));
	} else {
		g_message ("error occurred on export: %s", 
		           skstore->priv->drag_error && skstore->priv->drag_error->message ? 
		                      skstore->priv->drag_error->message : "");
	}
	
	g_object_unref (output);
	return ret;
}

static gboolean
export_to_filename (SeahorseKeyManagerStore *skstore, const gchar *filename)
{
	GOutputStream *output;
	gboolean ret;
	gchar *uri;
	GFile *file;
	GList *keys;
	
	DBG_PRINT (("exporting to %s\n", filename));

	ret = FALSE;
	g_return_val_if_fail (skstore->priv->drag_keys, FALSE);
	keys = g_list_copy (skstore->priv->drag_keys);
	
	uri = seahorse_util_uri_unique (filename);

	/* Create output file */
	file = g_file_new_for_uri (uri);
	g_free (uri);
	output = G_OUTPUT_STREAM (g_file_replace (file, NULL, FALSE, G_FILE_CREATE_NONE, 
	                                          NULL, &skstore->priv->drag_error));
	g_object_unref (file);
	
	if (output) {
		/* This modifies and frees keys */
		ret = export_keys_to_output (keys, output, &skstore->priv->drag_error) &&
		      g_output_stream_close (output, NULL, &skstore->priv->drag_error);
		
		g_object_unref (output);
	}
	
	return ret;
}

static gboolean
drag_data_get (GtkWidget *widget, GdkDragContext *context, 
               GtkSelectionData *selection_data, guint info, 
               guint time, SeahorseKeyManagerStore *skstore)
{
	gchar *destination;
	gboolean ret;

	DBG_PRINT (("drag_data_get %d -->\n", info)); 
	
	g_return_val_if_fail (skstore->priv->drag_keys, FALSE);

	/* The caller wants plain text */
	if (info == DRAG_INFO_TEXT) {
		DBG_PRINT (("returning key text\n"));
		export_to_text (skstore, selection_data);
		
	/* The caller wants XDS */
	} else if (info == DRAG_INFO_XDS) {
		
		if (xds_is_dnd_valid_context (context)) {
			destination = xds_get_atom_value (context);
			g_return_val_if_fail (destination, FALSE);
			skstore->priv->drag_destination = g_path_get_dirname (destination);
			g_free (destination);
		
			gtk_selection_data_set (selection_data, selection_data->target, 8, (guchar*)"S", 1);
			ret = TRUE;
		}
		
	/* Unrecognized format */
	} else {
		DBG_PRINT (("Unrecognized format: %d\n", info));
	}
	
	DBG_PRINT(("drag_data_get <--\n"));
	return ret;
}

static void
drag_end (GtkWidget *widget, GdkDragContext *context, SeahorseKeyManagerStore *skstore)
{
	gchar *filename, *name;
	
	DBG_PRINT (("drag_end -->\n"));
	
	if (skstore->priv->drag_destination && !skstore->priv->drag_error) {
		g_return_if_fail (skstore->priv->drag_keys);
	
		name = seahorse_util_filename_for_keys (skstore->priv->drag_keys);
		g_return_if_fail (name);
	
		filename = g_build_filename (skstore->priv->drag_destination, name, NULL);
		g_free (name);
	
		export_to_filename (skstore, filename);
		g_free (filename);
	}
	
	if (skstore->priv->drag_error) {
		seahorse_util_show_error (widget, _("Couldn't export keys"),
		                          skstore->priv->drag_error->message);
	}
	
	g_clear_error (&skstore->priv->drag_error);
	g_list_free (skstore->priv->drag_keys);
	skstore->priv->drag_keys = NULL;
	g_free (skstore->priv->drag_destination);
	skstore->priv->drag_destination = NULL;
	
	DBG_PRINT (("drag_end <--\n"));
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

    gtk_drag_source_set (GTK_WIDGET (view), GDK_BUTTON1_MASK,
                         store_targets, G_N_ELEMENTS (store_targets), GDK_ACTION_COPY);

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
