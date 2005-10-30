/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
 * Copyright (C) 2004, 2005 Nate Nielsen
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
#include "seahorse-op.h"
#include "seahorse-gconf.h"
#include "seahorse-gpgmex.h"
#include "eggtreemultidnd.h"
#include "seahorse-pgp-key.h"
#include "seahorse-ssh-key.h"

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
	TRUST_STR,
	TYPE,
	EXPIRES_STR,
    VALIDITY,
    EXPIRES,
    TRUST,
	COLS
};

static const gchar* col_ids[] = {
    KEY_STORE_BASE_IDS,
    "validity",
    "trust",
    "type",
    "expires",
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

static void	        class_init	                (SeahorseKeyManagerStoreClass	*klass);

static gboolean     store_append                (SeahorseKeyStore    *skstore,
                                                 SeahorseKey         *skey,
                                                 guint               uid,
                                                 GtkTreeIter         *iter);

static void         store_set                   (SeahorseKeyStore    *store,
                                                 SeahorseKey         *skey,
                                                 guint               uid,
                                                 GtkTreeIter         *iter);
                                                 
static void         store_changed               (SeahorseKeyStore    *skstore,
                                                 SeahorseKey         *skey,
                                                 guint               uid,
                                                 GtkTreeIter         *iter,
                                                 SeahorseKeyChange   change);

static SeahorseKeyStoreClass *parent_class = NULL;

GType
seahorse_key_manager_store_get_type (void)
{
	static GType gtype = 0;
	
	if (!gtype) {
		static const GTypeInfo gtinfo = {
			sizeof (SeahorseKeyManagerStoreClass), NULL, NULL,
			(GClassInitFunc) class_init, NULL, NULL,
			sizeof (SeahorseKeyManagerStore), 0, NULL
		};
		
		gtype = g_type_register_static (SEAHORSE_TYPE_KEY_STORE,
			                            "SeahorseKeyManagerStore", &gtinfo, 0);
	}
	
	return gtype;
}

static void
class_init (SeahorseKeyManagerStoreClass *klass)
{
    SeahorseKeyStoreClass *skstore_class;
    GObjectClass *gobject_class;
	
    parent_class = g_type_class_peek_parent (klass);
    gobject_class = G_OBJECT_CLASS (klass);
	skstore_class = SEAHORSE_KEY_STORE_CLASS (klass);
	  
    skstore_class->append = store_append;
    skstore_class->set = store_set;
    skstore_class->changed = store_changed;
  
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
store_append (SeahorseKeyStore *skstore, SeahorseKey *skey, guint uid, GtkTreeIter *iter)
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
store_set (SeahorseKeyStore *store, SeahorseKey *skey, guint uid, GtkTreeIter *iter)
{
	SeahorseValidity validity, trust;
    SeahorseKeyPredicate *pred;
	gulong expires_date;
	gchar *expires;
    gchar *type;
	
	parent_class->set (store, skey, uid, iter);
    
    if (uid == 0) {
    	
    	validity = seahorse_key_get_validity (skey);
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
    			expires = seahorse_util_get_date_string (expires_date);
    	}
        
        /* Only differentiate if the view shows more than one type of key */
        if (seahorse_key_get_ktype (skey) == SKEY_PGP) {
            g_object_get (store->skset, "predicate", &pred, NULL);
            if (pred->etype == 0) {
                /* TODO: With more than one ktype this becomes invalid */
                if (seahorse_key_get_etype (skey) == SKEY_PRIVATE) 
                    type = _("Private PGP Key");
                else 
                    type = _("Public PGP Key");
            } else {
                type = _("PGP Key");
            }
        }
#ifdef WITH_SSH
        else if (seahorse_key_get_ktype (skey) == SKEY_SSH)
            type = _("SSH Key");
#endif
        
    	gtk_tree_store_set (GTK_TREE_STORE (store), iter,
    		VALIDITY_STR, validity == SEAHORSE_VALIDITY_UNKNOWN ? "" : seahorse_validity_get_string (validity),
    		VALIDITY, validity,
    		TRUST_STR, trust == SEAHORSE_VALIDITY_UNKNOWN ? "" : seahorse_validity_get_string (trust),
    		TRUST, trust,
    		TYPE, type,
    		EXPIRES_STR, expires,
    		EXPIRES, expires_date,
    		-1);
         
        g_free (expires);
       
    } else {

		gtk_tree_store_set (GTK_TREE_STORE (store), iter,
			TYPE, _("User ID"), KEY_STORE_KEYID, "", -1);

	}
}

/* Refreshed @skey if trust has changed */
static void
store_changed (SeahorseKeyStore *skstore, SeahorseKey *skey, guint uid, 
               GtkTreeIter *iter, SeahorseKeyChange change)
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
    GtkTreeViewColumn *col = NULL;
    GList *columns, *l;
    GConfValue *value;
    const gchar *key;
    const gchar *t;

    key = gconf_entry_get_key (entry);

    g_return_if_fail (key != NULL);
    g_return_if_fail (GTK_IS_TREE_VIEW (view));
    
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

static void  
drag_begin (GtkWidget *widget, GdkDragContext *context, SeahorseKeyStore *skstore)
{
    GtkTreeView *view = GTK_TREE_VIEW (widget);
    SeahorseKeySource *sksrc;
    SeahorseMultiOperation *mop = NULL;
    SeahorseOperation *op = NULL;
   	GList *next, *keys, *sel_keys = NULL;
    gpgme_data_t data = NULL;
    SeahorseKey *skey;
    
    DBG_PRINT (("drag_begin -->\n"));
    
  	sel_keys = seahorse_key_store_get_selected_keys (view);
    if(sel_keys != NULL) {
        
        /* Sort by key source */
        keys = g_list_copy (sel_keys);
        keys = seahorse_util_keylist_sort (keys);
    
        while (keys) {
     
            /* Break off one set (same keysource) */
            next = seahorse_util_keylist_splice (keys);

            g_return_if_fail (SEAHORSE_IS_KEY (keys->data));
            skey = SEAHORSE_KEY (keys->data);

            /* Export from this key source */        
            sksrc = seahorse_key_get_source (skey);
            g_return_if_fail (sksrc != NULL);
            
            if (!mop) 
                mop = seahorse_multi_operation_new ();
            
            /* The data object where we export to */
            if (!data) {
                gpgme_data_new (&data);
                g_return_if_fail (data != NULL);
                g_object_set_data_full (G_OBJECT (mop), "result-data", data,
                                        (GDestroyNotify)gpgme_data_release);
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
    g_return_if_fail (file != NULL);
    DBG_PRINT (("deleting temp file: %s\n", file));
    unlink (file);
    g_free (file);
}

static void  
drag_end (GtkWidget *widget, GdkDragContext *context, SeahorseKeyStore *skstore)
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
               guint time, SeahorseKeyStore *skstore)
{
    SeahorseOperation *op;
    gchar *t, *n;
    GList *keys;
    GError *err = NULL;
    gpgme_error_t gerr;
    gpgme_data_t data;

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
    
    if (info == TEXT_PLAIN) {
        t = seahorse_util_write_data_to_text (data, FALSE);

    } else {
        t = (gchar*)g_object_get_data (G_OBJECT (widget), "drag-file");
        
        if (t == NULL) {
            keys = g_object_get_data (G_OBJECT (widget), "drag-keys");
            g_return_if_fail (keys != NULL);
            
            n = seahorse_util_filename_for_keys (keys);
            g_return_if_fail (n != NULL);
            t = g_build_filename(g_get_tmp_dir (), n, NULL);
            g_free (n);
            
            gerr = seahorse_util_write_data_to_file (t, data, FALSE);
            g_return_if_fail (GPG_IS_OK (gerr));

            g_object_set_data_full (G_OBJECT (widget), "drag-file", t,
                                    (GDestroyNotify)cleanup_file);
        } 
        
        t = g_strdup (t);
    }
    
    if (t != NULL) {            
        DBG_PRINT (("%s\n", t));
    	gtk_selection_data_set (selection_data,  selection_data->target, 8, 
                                (const guchar*)t, strlen (t));
    }

    DBG_PRINT(("drag_data_get <--\n"));

    g_free(t);
}

/**
 * seahorse_key_manager_store_new:
 * @view: #GtkTreeView to show the new #SeahorseKeyManagerStore
 *
 * Creates a new #SeahorseKeyManagerStore.
 * Shown attributes are Name, KeyID, Trust, Type, and Length.
 *
 * Returns: The new #SeahorseKeyStore
 **/
SeahorseKeyStore*
seahorse_key_manager_store_new (SeahorseKeyset *skset, GtkTreeView *view)
{
	SeahorseKeyStore *skstore;
	GtkTreeViewColumn *col;
    SeahorseKeyPredicate *pred;

	skstore = g_object_new (SEAHORSE_TYPE_KEY_MANAGER_STORE, "keyset", skset, NULL);
    seahorse_key_store_init (skstore, view);
    
    g_object_get (skset, "predicate", &pred, NULL);
	
    if (pred->etype & SKEY_PUBLIC) {
    	col = seahorse_key_store_append_column (view, _("Validity"), VALIDITY_STR);
        g_object_set_data (G_OBJECT (col), "gconf-key", SHOW_VALIDITY_KEY);
	    gtk_tree_view_column_set_visible (col, seahorse_gconf_get_boolean (SHOW_VALIDITY_KEY));
	    gtk_tree_view_column_set_sort_column_id (col, VALIDITY);
	
    	col = seahorse_key_store_append_column (view, _("Trust"), TRUST_STR);
        g_object_set_data (G_OBJECT (col), "gconf-key", SHOW_TRUST_KEY);
	    gtk_tree_view_column_set_visible (col, seahorse_gconf_get_boolean (SHOW_TRUST_KEY));
	    gtk_tree_view_column_set_sort_column_id (col, TRUST);
    }
    
	col = seahorse_key_store_append_column (view, _("Type"), TYPE);
    g_object_set_data (G_OBJECT (col), "gconf-key", SHOW_TYPE_KEY);
	gtk_tree_view_column_set_visible (col, seahorse_gconf_get_boolean (SHOW_TYPE_KEY));
	gtk_tree_view_column_set_sort_column_id (col, TYPE);

	col = seahorse_key_store_append_column (view, _("Expiration Date"), EXPIRES_STR);
    g_object_set_data (G_OBJECT (col), "gconf-key", SHOW_EXPIRES_KEY);
	gtk_tree_view_column_set_visible (col, seahorse_gconf_get_boolean (SHOW_EXPIRES_KEY));
	gtk_tree_view_column_set_sort_column_id (col, EXPIRES);

    seahorse_gconf_notify_lazy (LISTING_SCHEMAS, (GConfClientNotifyFunc)gconf_notification, 
                                view, GTK_WIDGET (view));

    /* Tree drag */
	egg_tree_multi_drag_add_drag_support (view);    
    
    g_signal_connect (G_OBJECT (view), "drag_data_get",
			    G_CALLBACK (drag_data_get), skstore);
	g_signal_connect (G_OBJECT (view), "drag_begin", 
                G_CALLBACK (drag_begin), skstore);
	g_signal_connect (G_OBJECT (view), "drag_end", 
                G_CALLBACK (drag_end), skstore);

    gtk_drag_source_set (GTK_WIDGET (view), GDK_BUTTON1_MASK | GDK_BUTTON2_MASK,
			    seahorse_target_entries, seahorse_n_targets, GDK_ACTION_COPY);

	return skstore;
}
