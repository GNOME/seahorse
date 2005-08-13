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

#ifndef __SEAHORSE_KEYSET_H__
#define __SEAHORSE_KEYSET_H__

#include "seahorse-context.h"
#include "seahorse-key.h"
#include "seahorse-key-source.h"

#define SEAHORSE_TYPE_KEYSET               (seahorse_keyset_get_type ())
#define SEAHORSE_KEYSET(obj)               (GTK_CHECK_CAST ((obj), SEAHORSE_TYPE_KEYSET, SeahorseKeyset))
#define SEAHORSE_KEYSET_CLASS(klass)       (GTK_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_KEYSET, SeahorseKeysetClass))
#define SEAHORSE_IS_KEYSET(obj)            (GTK_CHECK_TYPE ((obj), SEAHORSE_TYPE_KEYSET))
#define SEAHORSE_IS_KEYSET_CLASS(klass)    (GTK_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_KEYSET))
#define SEAHORSE_KEYSET_GET_CLASS(obj)     (GTK_CHECK_GET_CLASS ((obj), SEAHORSE_TYPE_KEYSET, SeahorseKeysetClass))

typedef struct _SeahorseKeyset SeahorseKeyset;
typedef struct _SeahorseKeysetClass SeahorseKeysetClass;
typedef struct _SeahorseKeysetPrivate SeahorseKeysetPrivate;
    
struct _SeahorseKeyset {
	GtkObject		        parent;
	
	/*<private>*/
    SeahorseKeysetPrivate   *pv;
};

struct _SeahorseKeysetClass {
	GtkObjectClass		parent_class;
    
    /* signals --------------------------------------------------------- */
    
    /* A key was added to this view */
    void (*added)   (SeahorseKeyset *skset, SeahorseKey *key);

    /* Removed a key from this view */
    void (*removed) (SeahorseKeyset *skset, SeahorseKey *key, gpointer closure);
    
	/* One of the key's attributes has changed */
	void (*changed) (SeahorseKeyset *skset, SeahorseKey *skey, 
                     SeahorseKeyChange change, gpointer closure);    
};

GType               seahorse_keyset_get_type           (void);

SeahorseKeyset*     seahorse_keyset_new                (GQuark             ktype,
                                                        SeahorseKeyEType   etype,
                                                        SeahorseKeyLoc     location,
                                                        guint              flags);

SeahorseKeyset*     seahorse_keyset_new_full           (SeahorseKeyPredicate *pred);

gboolean            seahorse_keyset_has_key            (SeahorseKeyset *skset,
                                                        SeahorseKey *skey);

GList*              seahorse_keyset_get_keys           (SeahorseKeyset *skset);

guint               seahorse_keyset_get_count          (SeahorseKeyset *skset);

void                seahorse_keyset_refresh            (SeahorseKeyset *skset);

gpointer            seahorse_keyset_get_closure        (SeahorseKeyset *skset,
                                                        SeahorseKey *skey);

void                seahorse_keyset_set_closure        (SeahorseKeyset *skset,
                                                        SeahorseKey *skey,
                                                        gpointer closure);

#endif /* __SEAHORSE_KEY_SET_H__ */
