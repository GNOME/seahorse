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

#ifndef __SEAHORSE_KEY_STORE_H__
#define __SEAHORSE_KEY_STORE_H__

#include <gtk/gtk.h>

#include "seahorse-context.h"
#include "seahorse-keyset.h"

#define SEAHORSE_TYPE_KEY_STORE			(seahorse_key_store_get_type ())
#define SEAHORSE_KEY_STORE(obj)			(GTK_CHECK_CAST ((obj), SEAHORSE_TYPE_KEY_STORE, SeahorseKeyStore))
#define SEAHORSE_KEY_STORE_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_KEY_STORE, SeahorseKeyStoreClass))
#define SEAHORSE_IS_KEY_STORE(obj)		(GTK_CHECK_TYPE ((obj), SEAHORSE_TYPE_KEY_STORE))
#define SEAHORSE_IS_KEY_STORE_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_KEY_STORE))
#define SEAHORSE_KEY_STORE_GET_CLASS(obj)	(GTK_CHECK_GET_CLASS ((obj), SEAHORSE_TYPE_KEY_STORE, SeahorseKeyStoreClass))

typedef struct _SeahorseKeyStore SeahorseKeyStore;
typedef struct _SeahorseKeyStorePriv SeahorseKeyStorePriv;
typedef struct _SeahorseKeyStoreClass SeahorseKeyStoreClass;

typedef enum _SeahorseKeyStoreMode {
    KEY_STORE_MODE_ALL,
    KEY_STORE_MODE_SELECTED,
    KEY_STORE_MODE_FILTERED
} SeahorseKeyStoreMode;

struct _SeahorseKeyStore {
	GtkTreeStore		    parent;
 
    /*< public >*/
    SeahorseKeyset          *skset;
    
    /*< private >*/
    SeahorseKeyStorePriv    *priv;
};

struct _SeahorseKeyStoreClass
{
	GtkTreeStoreClass	parent_class;
    
   /* Virtual method for appending a key/uid. When subclass method finishes,
    * iter should be set to the parent of the new row.
    * This is needed since not every key/uid should always be added.
    */
    gboolean       (* append)   (SeahorseKeyStore   *skstore,
                                 SeahorseKey        *skey,
                                 guint              uid,
                                 GtkTreeIter        *iter); 
                                 
    /* Virtual method for setting the key's attributes. Name and KeyID are
     * set by the key-store, so implementation in subclasses is optional.
     */
    void            (* set)     (SeahorseKeyStore   *skstore,
                                 SeahorseKey        *skey,
                                 guint              uid,
                                 GtkTreeIter        *iter);
 
   /* Virtual method for when the key at iter has changed. Key-store
   * will already do user ID changes, so implementation is optional
   * depending on displayed attributes.
   */
    void            (* changed) (SeahorseKeyStore   *skstore,
                                 SeahorseKey        *skey,
                                 guint              uid,
                                 GtkTreeIter        *iter,
                                 SeahorseKeyChange  change);
                               
    gboolean        use_check;  /* use the check column */
    gboolean        use_icon;   /* display an icon for key pairs in the list */
    guint           n_columns;  /* Number of columns */
    const GType*    col_types;  /* Type of each column */
    const gchar**   col_ids;    /* schema identifier for each column */
    const gchar*    gconf_sort_key; /* Key to save sort order in */
};


enum {
    KEY_STORE_DATA,
    KEY_STORE_CHECK,
    KEY_STORE_PAIR,
    KEY_STORE_NOTPAIR,
    KEY_STORE_NAME,
    KEY_STORE_KEYID,
    KEY_STORE_UID,
    KEY_STORE_NCOLS
};

/* For use as first enum in base class' column index list */
#define KEY_STORE_BASE_COLUMNS \
            X_BASE_COLS = KEY_STORE_UID       

/* For use as first item in base class' column id list */
#define KEY_STORE_BASE_IDS   \
            NULL,            \
            NULL,            \
            "pair",          \
            NULL,            \
            "name",          \
            "id",            \
            NULL
            
/* For use in base class' type list */            
#define KEY_STORE_BASE_TYPES \
            G_TYPE_POINTER,  \
            G_TYPE_BOOLEAN,  \
            G_TYPE_BOOLEAN,  \
            G_TYPE_BOOLEAN,  \
            G_TYPE_STRING,   \
            G_TYPE_STRING,   \
            G_TYPE_INT
            

GType               seahorse_key_store_get_type             ();

void                seahorse_key_store_init                 (SeahorseKeyStore   *skstore,
								                             GtkTreeView        *view);

void                seahorse_key_store_populate             (SeahorseKeyStore   *skstore);

SeahorseKey*	    seahorse_key_store_get_key_from_path	(GtkTreeView		*view,
                                                             GtkTreePath		*path,
                                                             guint              *uid);

GtkTreeViewColumn*	seahorse_key_store_append_column	(GtkTreeView		*view,
								 const gchar		*name,
								 const gint		index);

GList*              seahorse_key_store_get_all_keys         (GtkTreeView        *view);

GList*              seahorse_key_store_get_selected_keys    (GtkTreeView        *view);

SeahorseKey*        seahorse_key_store_get_selected_key	    (GtkTreeView		*view,
                                                             guint              *uid);

void                seahorse_key_store_get_base_iter(SeahorseKeyStore* skstore, 
                                GtkTreeIter* base_iter, 
                                const GtkTreeIter* iter);

#endif /* __SEAHORSE_KEY_STORE_H__ */
