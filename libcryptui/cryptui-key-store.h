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

typedef enum _CryptUIKeyStoreMode {
    CRYPTUI_KEY_STORE_MODE_ALL,
    CRYPTUI_KEY_STORE_MODE_SELECTED,
    CRYPTUI_KEY_STORE_MODE_FILTERED
} CryptUIKeyStoreMode;

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

/* This should always match the list in cryptui-keystore.c */
enum {
    CRYPTUI_KEY_STORE_NAME,         /* string */
    CRYPTUI_KEY_STORE_KEYID,        /* keyid */
    CRYPTUI_KEY_STORE_CHECK,        /* boolean */
    CRYPTUI_KEY_STORE_PAIR,         /* boolean */
    CRYPTUI_KEY_STORE_STOCK_ID,     /* string */
    CRYPTUI_KEY_STORE_KEY,          /* pointer */
    CRYPTUI_KEY_STORE_NCOLS
};


GType               cryptui_key_store_get_type              ();

CryptUIKeyStore*    cryptui_key_store_new                   (CryptUIKeyset *keyset, 
                                                             gboolean use_check);

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

GList*              cryptui_key_store_get_selected_keys     (CryptUIKeyStore *ckstore, 
                                                             GtkTreeView *view);

const gchar*        cryptui_key_store_get_selected_key      (CryptUIKeyStore *ckstore, 
                                                             GtkTreeView *view);

#endif /* __CRYPTUI_KEY_STORE_H__ */
