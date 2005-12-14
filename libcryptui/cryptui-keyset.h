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
 
#ifndef __CRYPTUI_KEYSET_H__
#define __CRYPTUI_KEYSET_H__

#include "cryptui.h"

#include <gtk/gtk.h>

#define CRYPTUI_TYPE_KEYSET               (cryptui_keyset_get_type ())
#define CRYPTUI_KEYSET(obj)               (GTK_CHECK_CAST ((obj), CRYPTUI_TYPE_KEYSET, CryptUIKeyset))
#define CRYPTUI_KEYSET_CLASS(klass)       (GTK_CHECK_CLASS_CAST ((klass), CRYPTUI_TYPE_KEYSET, CryptUIKeysetClass))
#define CRYPTUI_IS_KEYSET(obj)            (GTK_CHECK_TYPE ((obj), CRYPTUI_TYPE_KEYSET))
#define CRYPTUI_IS_KEYSET_CLASS(klass)    (GTK_CHECK_CLASS_TYPE ((klass), CRYPTUI_TYPE_KEYSET))
#define CRYPTUI_KEYSET_GET_CLASS(obj)     (GTK_CHECK_GET_CLASS ((obj), CRYPTUI_TYPE_KEYSET, CryptUIKeysetClass))

typedef struct _CryptUIKeyset CryptUIKeyset;
typedef struct _CryptUIKeysetClass CryptUIKeysetClass;
typedef struct _CryptUIKeysetPrivate CryptUIKeysetPrivate;

struct _CryptUIKeyset {
    GtkObject parent;

    /*<private>*/
    CryptUIKeysetPrivate *priv;
};

struct _CryptUIKeysetClass {
    GtkObjectClass      parent_class;
    
    /* signals --------------------------------------------------------- */
    
    /* A key was added to this view */
    void (*added)   (CryptUIKeyset *keyset, const gchar *key);

    /* Removed a key from this view */
    void (*removed) (CryptUIKeyset *keyset, const gchar *key, gpointer closure);
    
    /* One of the key's attributes has changed */
    void (*changed) (CryptUIKeyset *keyset, const gchar *key, gpointer closure);    
};

GType               cryptui_keyset_get_type           (void);

CryptUIKeyset*      cryptui_keyset_new                (const gchar *keytype,
                                                       CryptUIEncType enctype);

gboolean            cryptui_keyset_has_key            (CryptUIKeyset *keyset,
                                                       const gchar *key);

GList*              cryptui_keyset_get_keys           (CryptUIKeyset *keyset);

guint               cryptui_keyset_get_count          (CryptUIKeyset *keyset);

void                cryptui_keyset_refresh            (CryptUIKeyset *keyset);

gpointer            cryptui_keyset_get_closure        (CryptUIKeyset *keyset,
                                                       const gchar *key);

void                cryptui_keyset_set_closure        (CryptUIKeyset *keyset,
                                                       const gchar *key,
                                                       gpointer closure);

#endif /* __CRYPTUI_KEYSET_H__ */
