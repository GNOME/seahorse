/* 
 * Seahorse
 * 
 * Copyright (C) 2005 Nate Nielsen 
 * 
 * This program is free software; you can redistribute it and/or modify 
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *  
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *  
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.  
 */

#ifndef __CRYPTUI_KEY_STORE_H__
#define __CRYPTUI_KEY_STORE_H__

#include <gtk/gtk.h>

#include "cryptui-keyset.h"

#define CRYPTUI_TYPE_KEY_STORE             (cryptui_key_store_get_type ())
#define CRYPTUI_KEY_STORE(obj)             (GTK_CHECK_CAST ((obj), CRYPTUI_TYPE_KEY_STORE, CryptUIKeyStore))
#define CRYPTUI_KEY_STORE_CLASS(klass)     (GTK_CHECK_CLASS_CAST ((klass), CRYPTUI_TYPE_KEY_STORE, CryptUIKeyStore))
#define CRYPTUI_IS_KEY_STORE(obj)          (GTK_CHECK_TYPE ((obj), CRYPTUI_TYPE_KEY_STORE))
#define CRYPTUI_IS_KEY_STORE_CLASS(klass)  (GTK_CHECK_CLASS_TYPE ((klass), CRYPTUI_TYPE_KEY_STORE))
#define CRYPTUI_KEY_STORE_GET_CLASS(obj)   (GTK_CHECK_GET_CLASS ((obj), CRYPTUI_TYPE_KEY_STORE, CryptUIKeyStore))

typedef struct _CryptUIKeyStore CryptUIKeyStore;
typedef struct _CryptUIKeyStorePriv CryptUIKeyStorePriv;
typedef struct _CryptUIKeyStoreClass CryptUIKeyStoreClass;

struct _CryptUIKeyStore {
    GtkTreeModelSort       parent;
 
    /*< public >*/
    CryptUIKeyset          *ckset;
    
    /*< private >*/
    CryptUIKeyStorePriv    *priv;
};

struct _CryptUIKeyStoreClass {
    GtkTreeModelSortClass       parent_class;
};

typedef enum _CryptUIKeyStoreMode {
    CRYPTUI_KEY_STORE_MODE_ALL,
    CRYPTUI_KEY_STORE_MODE_SELECTED,
    CRYPTUI_KEY_STORE_MODE_RESULTS
} CryptUIKeyStoreMode;

/* For custom filters */
typedef gboolean (*CryptUIKeyStoreFilterFunc) (CryptUIKeyset *ckset, const gchar *key, 
                                               gpointer user_data);

/* This should always match the list in cryptui-keystore.c */
enum {
    /* Displayable columns */
    CRYPTUI_KEY_STORE_NAME,         /* (string) Text to display as the key name */
    CRYPTUI_KEY_STORE_KEYID,        /* (string) Text to display as the key id */
    CRYPTUI_KEY_STORE_CHECK,        /* (boolean) Is this key selected or not */
    
    /* Metadata formatting */
    CRYPTUI_KEY_STORE_PAIR,         /* (boolean) A key pair or not */
    CRYPTUI_KEY_STORE_STOCK_ID,     /* (string) Image to display next to key */
    CRYPTUI_KEY_STORE_SEPARATOR,    /* (boolean) Row is a separator */
    CRYPTUI_KEY_STORE_KEY,          /* (pointer) Pointer to key handle */
    
    CRYPTUI_KEY_STORE_NCOLS
};

GType               cryptui_key_store_get_type              ();

CryptUIKeyStore*    cryptui_key_store_new                   (CryptUIKeyset *keyset, 
                                                             gboolean use_checks,
                                                             const gchar *none_option);

CryptUIKeyset*      cryptui_key_store_get_keyset            (CryptUIKeyStore *ckstore);

void                cryptui_key_store_set_sortable          (CryptUIKeyStore *ckstore,
                                                             gboolean sortable);

gboolean            cryptui_key_store_get_sortable          (CryptUIKeyStore *ckstore);

void                cryptui_key_store_check_toggled         (CryptUIKeyStore *ckstore, 
                                                             GtkTreeView *view, 
                                                             GtkTreeIter *iter);

gboolean            cryptui_key_store_get_iter_from_key     (CryptUIKeyStore *ckstore,
                                                             const gchar *key,
                                                             GtkTreeIter *iter);

const gchar*        cryptui_key_store_get_key_from_iter     (CryptUIKeyStore *ckstore,
                                                             GtkTreeIter *iter);

const gchar*        cryptui_key_store_get_key_from_path     (CryptUIKeyStore *ckstore, 
                                                             GtkTreePath *path);

GList*              cryptui_key_store_get_all_keys          (CryptUIKeyStore *ckstore);

gboolean            cryptui_key_store_have_selected_keys    (CryptUIKeyStore *ckstore,
                                                             GtkTreeView *view);

GList*              cryptui_key_store_get_selected_keys     (CryptUIKeyStore *ckstore, 
                                                             GtkTreeView *view);

void                cryptui_key_store_set_selected_keys     (CryptUIKeyStore *ckstore, 
                                                             GtkTreeView *view,
                                                             GList *keys);

const gchar*        cryptui_key_store_get_selected_key      (CryptUIKeyStore *ckstore, 
                                                             GtkTreeView *view);

void                cryptui_key_store_set_selected_key      (CryptUIKeyStore *ckstore, 
                                                             GtkTreeView *view,
                                                             const gchar *key);
                                                             
void                cryptui_key_store_set_search_mode       (CryptUIKeyStore *ckstore,
                                                             CryptUIKeyStoreMode mode);

void                cryptui_key_store_set_search_text       (CryptUIKeyStore *ckstore,
                                                             const gchar *search_text);
                                                             
void                cryptui_key_store_set_filter            (CryptUIKeyStore *ckstore,
                                                             CryptUIKeyStoreFilterFunc func,
                                                             gpointer user_data);

#endif /* __CRYPTUI_KEY_STORE_H__ */
