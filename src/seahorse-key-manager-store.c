/*
 * Seahorse
 *
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

#include <gnome.h>
#include <eel/eel.h>

#include "seahorse-key-manager-store.h"
#include "seahorse-preferences.h"
#include "seahorse-validity.h"
#include "seahorse-util.h"
#include "seahorse-op.h"
#include "eggtreemultidnd.h"

#define KEY_MANAGER_SORT_KEY "/apps/seahorse/listing/sort_by"

/* Drag and drop targent entries */
const GtkTargetEntry seahorse_target_entries[] = {
    { "text/uri-list", 0, TEXT_URIS },
    { "text/plain", 0, TEXT_PLAIN },
    { "STRING", 0, TEXT_PLAIN }
};

guint seahorse_n_targets = 
    sizeof (seahorse_target_entries) / sizeof (seahorse_target_entries[0]);

enum {
    KEY_STORE_BASE_COLUMNS,
	VALIDITY_STR,
	EXPIRES_STR,
	TRUST_STR,
	TYPE,
    VALIDITY,
    EXPIRES,
    TRUST,
	COLS
};

static const gchar* col_ids[] = {
    KEY_STORE_BASE_IDS,
    "validity",
    "expires",
    "trust",
    "type",
    "validity",
    "expires",
    "trust"
};

static GType col_types[] = {
    KEY_STORE_BASE_TYPES, 
    G_TYPE_STRING,
    G_TYPE_STRING, 
    G_TYPE_STRING, 
    G_TYPE_STRING,
    G_TYPE_INT, 
    G_TYPE_LONG, 
    G_TYPE_INT
};

static void	seahorse_key_manager_store_class_init	(SeahorseKeyManagerStoreClass	*klass);

static gboolean seahorse_key_manager_store_append   (SeahorseKeyStore    *skstore,
                                                     SeahorseKey         *skey,
                                                     guint               uid,
                                                     GtkTreeIter         *iter);
static void     seahorse_key_manager_store_set      (SeahorseKeyStore    *store,
                                                     SeahorseKey         *skey,
                                                     guint               uid,
                                                     GtkTreeIter         *iter);
static void     seahorse_key_manager_store_changed  (SeahorseKeyStore    *skstore,
                                                     SeahorseKey         *skey,
                                                     guint               uid,
                                                     GtkTreeIter         *iter,
                                                     SeahorseKeyChange   change);

static SeahorseKeyStoreClass	*parent_class	= NULL;

GType
seahorse_key_manager_store_get_type (void)
{
	static GType key_manager_store_type = 0;
	
	if (!key_manager_store_type) {
		static const GTypeInfo key_manager_store_info =
		{
			sizeof (SeahorseKeyManagerStoreClass),
			NULL, NULL,
			(GClassInitFunc) seahorse_key_manager_store_class_init,
			NULL, NULL,
			sizeof (SeahorseKeyManagerStore),
			0, NULL
		};
		
		key_manager_store_type = g_type_register_static (SEAHORSE_TYPE_KEY_STORE,
			"SeahorseKeyManagerStore", &key_manager_store_info, 0);
	}
	
	return key_manager_store_type;
}

static void
seahorse_key_manager_store_class_init (SeahorseKeyManagerStoreClass *klass)
{
	SeahorseKeyStoreClass *skstore_class;
	
	parent_class = g_type_class_peek_parent (klass);
	skstore_class = SEAHORSE_KEY_STORE_CLASS (klass);
	
	skstore_class->append = seahorse_key_manager_store_append;
	skstore_class->set = seahorse_key_manager_store_set;
	skstore_class->changed = seahorse_key_manager_store_changed;
  
  	/* Base class behavior and columns */
    skstore_class->use_check = FALSE;
    skstore_class->use_icon = TRUE;
    skstore_class->n_columns = COLS;
    skstore_class->col_ids = col_ids;
    skstore_class->col_types = col_types;
    skstore_class->gconf_sort_key = KEY_MANAGER_SORT_KEY;
}

/* Do append for @skey & it's subkeys */
static gboolean
seahorse_key_manager_store_append (SeahorseKeyStore *skstore, SeahorseKey *skey, 
                                   guint uid, GtkTreeIter *iter)
{
    GtkTreeIter child;
    
    if (uid == 0) {
    	gtk_tree_store_append (GTK_TREE_STORE (skstore), iter, NULL);
        parent_class->append (skstore, skey, uid, iter);
    } else {
        gtk_tree_store_append (GTK_TREE_STORE (skstore), &child, iter);
        parent_class->append (skstore, skey, uid, &child);
    }
    
    return TRUE;
}

/* Sets attributes for @skey at @iter and @skey's subkeys at @iter's children */
static void
seahorse_key_manager_store_set (SeahorseKeyStore *store, SeahorseKey *skey, 
                                guint uid, GtkTreeIter *iter)
{
	SeahorseValidity validity, trust;
	gulong expires_date;
	gchar *expires;
    gchar *type;

    if (uid == 0) {
    	
    	validity = seahorse_key_get_validity (skey);
    	trust = seahorse_key_get_trust (skey);
    	
    	if (skey->key->expired) {
    		expires = g_strdup (_("Expired"));
    		expires_date = -1;
    	}
    	else {
    		expires_date = skey->key->subkeys->expires;
    		
    		if (expires_date == 0) {
    			expires = g_strdup (_("Never Expires"));
    			expires_date = G_MAXLONG;
    		}
    		else
    			expires = seahorse_util_get_date_string (expires_date);
    	}
        
        if (SEAHORSE_IS_KEY_PAIR (skey)) 
            type = _("Private PGP Key");
        else 
            type = _("Public PGP Key");
    	
    	gtk_tree_store_set (GTK_TREE_STORE (store), iter,
    		VALIDITY_STR, seahorse_validity_get_string (validity),
    		VALIDITY, validity,
    		EXPIRES_STR, expires,
    		EXPIRES, expires_date,
    		TRUST_STR, seahorse_validity_get_string (trust),
    		TRUST, trust,
    		TYPE, type,
    		-1);
         
        g_free (expires);
       
    } else {

		gtk_tree_store_set (GTK_TREE_STORE (store), iter,
			TYPE, _("User ID"), -1);

	}
	
	parent_class->set (store, skey, uid, iter);
}

/* Refreshed @skey if trust has changed */
static void
seahorse_key_manager_store_changed (SeahorseKeyStore *skstore, SeahorseKey *skey, 
				                    guint uid, GtkTreeIter *iter, SeahorseKeyChange change)
{
	switch (change) {
        case SKEY_CHANGE_ALL:
		case SKEY_CHANGE_TRUST: 
        case SKEY_CHANGE_EXPIRES:
		case SKEY_CHANGE_DISABLED:
			SEAHORSE_KEY_STORE_GET_CLASS (skstore)->set (skstore, skey, uid, iter);

		default:
			parent_class->changed (skstore, skey, uid, iter, change);
			break;
	}
}

static void
gconf_notification (GConfClient *gclient, guint id, GConfEntry *entry, GtkTreeView *view)
{
	const gchar *key;
	GConfValue *value;
	GtkTreeViewColumn *col;
	
	key = gconf_entry_get_key (entry);
	value = gconf_entry_get_value (entry);
	
	if (g_str_equal (key, SHOW_VALIDITY_KEY))
		col = gtk_tree_view_get_column (view, VALIDITY_STR - 4);
	else if (g_str_equal (key, SHOW_EXPIRES_KEY))
		col = gtk_tree_view_get_column (view, EXPIRES_STR - 4);
	else if (g_str_equal (key, SHOW_TRUST_KEY))
		col = gtk_tree_view_get_column (view, TRUST_STR - 4);
	else if (g_str_equal (key, SHOW_TYPE_KEY))
		col = gtk_tree_view_get_column (view, TYPE - 4);
	else
		return;
	
	gtk_tree_view_column_set_visible (col, gconf_value_get_bool (value));
}

static void  
drag_begin (GtkWidget *widget, GdkDragContext *context, gpointer data)
{
   	GList *keys = NULL;
    GtkTreeView *view = GTK_TREE_VIEW (data);

	g_printerr ("::DragBegin -->\n");
  	keys = seahorse_key_store_get_selected_keys (view);
    g_object_set_data (G_OBJECT (widget), "drag-keys", keys);    
	g_printerr ("::DragBegin <--\n");
}

static void
cleanup_file (GtkWidget *widget, gchar *file)
{
    g_return_if_fail (file != NULL);
    g_printerr ("deleting temp file: %s\n", file);
    unlink (file);
    g_free (file);
}

static void  
drag_end (GtkWidget *widget, GdkDragContext *context, gpointer data)
{
    GList *keys = NULL;
    gchar *t;
    
	g_printerr ("::DragEnd -->\n");
    keys = g_object_get_data (G_OBJECT (widget), "drag-keys");
    g_object_set_data (G_OBJECT (widget), "drag-keys", NULL);
    g_list_free (keys);
    
    t = (gchar*)g_object_get_data (G_OBJECT (widget), "drag-file");
    g_object_set_data (G_OBJECT (widget), "drag-file", NULL);
    if (t != NULL) /* Delete the files later */
        g_signal_connect (widget, "destroy", 
                    G_CALLBACK (cleanup_file), t);

    t = (gchar*)g_object_get_data (G_OBJECT (widget), "drag-data");
    g_object_set_data (G_OBJECT (widget), "drag-data", NULL);
    g_free (t);
    
    g_printerr ("::DragEnd <--\n");
}

static void  
drag_data_get (GtkWidget *widget, GdkDragContext *context, 
               GtkSelectionData *selection_data, guint info, 
               guint time, gpointer data)
{
    gchar *t, *n;
   	GList *keys = NULL;
    GError *err = NULL;
	g_printerr ("::DragDataGet %d -->\n", info); 

    keys = (GList*)g_object_get_data (G_OBJECT (widget), "drag-keys");
    if (keys == NULL)
        return;

    if (info == TEXT_PLAIN) {
        t = (gchar*)g_object_get_data (G_OBJECT (widget), "drag-text");
        
        if (t == NULL) {
            t = seahorse_op_export_text (keys, FALSE, &err);
            if (t == NULL)
                seahorse_util_handle_error (err, _("Couldn't export key(s)"));
            g_object_set_data (G_OBJECT (widget), "drag-text", t);
        }

    } else {
        t = (gchar*)g_object_get_data (G_OBJECT (widget), "drag-file");
        
        if (t == NULL) {
            n = seahorse_util_filename_for_keys (keys);
            g_return_if_fail (n != NULL);
            t = g_build_filename(g_get_tmp_dir (), n, NULL);
            g_free (n);
        
            seahorse_op_export_file (keys, FALSE, t, &err);         

            if (err != NULL) {
                seahorse_util_handle_error (err, _("Couldn't export key to \"%s\""),
                                seahorse_util_uri_get_last (t));
                g_free (t);
                t = NULL;
            }
            
            g_object_set_data (G_OBJECT (widget), "drag-file", t);
        }
    }
    
    if (t != NULL) {            
        g_printerr ("%s\n", t);
    	gtk_selection_data_set (selection_data, 
	    			selection_data->target, 8, t, 
				    strlen (t));
    }
    
#ifdef DEBUG
	g_printerr ("::DragDataGet <--\n");
#endif
}

/**
 * seahorse_key_manager_store_new:
 * @sctx: Current #SeahorseContext
 * @view: #GtkTreeView to show the new #SeahorseKeyManagerStore
 *
 * Creates a new #SeahorseKeyManagerStore.
 * Shown attributes are Name, KeyID, Trust, Type, and Length.
 *
 * Returns: The new #SeahorseKeyStore
 **/
SeahorseKeyStore*
seahorse_key_manager_store_new (SeahorseKeySource *sksrc, GtkTreeView *view)
{
	SeahorseKeyStore *skstore;
	GtkTreeViewColumn *col;

	skstore = g_object_new (SEAHORSE_TYPE_KEY_MANAGER_STORE, "key-source", sksrc, NULL);
    seahorse_key_store_init (skstore, view);
	
	col = seahorse_key_store_append_column (view, _("Validity"), VALIDITY_STR);
	gtk_tree_view_column_set_visible (col, eel_gconf_get_boolean (SHOW_VALIDITY_KEY));
	gtk_tree_view_column_set_sort_column_id (col, VALIDITY);
	
	col = seahorse_key_store_append_column (view, _("Expiration Date"), EXPIRES_STR);
	gtk_tree_view_column_set_visible (col, eel_gconf_get_boolean (SHOW_EXPIRES_KEY));
	gtk_tree_view_column_set_sort_column_id (col, EXPIRES);

	col = seahorse_key_store_append_column (view, _("Trust"), TRUST_STR);
	gtk_tree_view_column_set_visible (col, eel_gconf_get_boolean (SHOW_TRUST_KEY));
	gtk_tree_view_column_set_sort_column_id (col, TRUST);
	
	col = seahorse_key_store_append_column (view, _("Type"), TYPE);
	gtk_tree_view_column_set_visible (col, eel_gconf_get_boolean (SHOW_TYPE_KEY));
	gtk_tree_view_column_set_sort_column_id (col, TYPE);
	
	eel_gconf_notification_add (LISTING_SCHEMAS, (GConfClientNotifyFunc)gconf_notification, view);

    /* Tree drag */
	egg_tree_multi_drag_add_drag_support (view);    
    
    g_signal_connect (G_OBJECT (view), "drag_data_get",
			    G_CALLBACK (drag_data_get), view);
	g_signal_connect (G_OBJECT (view), "drag_begin", 
                G_CALLBACK (drag_begin), view);
	g_signal_connect (G_OBJECT (view), "drag_end", 
                G_CALLBACK (drag_end), view);

    gtk_drag_source_set (GTK_WIDGET (view), 
                GDK_BUTTON1_MASK | GDK_BUTTON2_MASK,
			    seahorse_target_entries, seahorse_n_targets, GDK_ACTION_MOVE);

	return skstore;
}
