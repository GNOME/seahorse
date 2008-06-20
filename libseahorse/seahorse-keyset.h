/*
 * Seahorse
 *
 * Copyright (C) 2005-2006 Stefan Walter
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

/**
 * SeahorseKeyset: A subset list of the keys in the SeahorseContext. 
 *
 * - Used all over by various views to narrow in on the keys that they're 
 *   interested in.
 * - Originally was going to be called SeahorseView (database parlance) but 
 *   that's far too confusing with overloaded terminology. 
 * - Uses a SeahorseKeyPredicate to match keys.
 * - Sends out events when keys get added and removed from it's view. Or a 
 *   key in the view changes etc...
 * - Supports per key event 'closures'. When a closure is set for a key, it's 
 *   then passed as an argument to the 'changed' and 'removed' events.
 * 
 * Signals:
 *   added: A key was added to this keyset.
 *   removed: A key disappeared from this keyset.
 *   changed: A key in the keyset changed.
 *   set-changed: The number of keys in the keyset changed
 * 
 * Properties:
 *   count: The number of keys 
 *   predicate: (SeahorseKeyPredicate) The predicate used for matching.
 */
 
#ifndef __SEAHORSE_KEYSET_H__
#define __SEAHORSE_KEYSET_H__

#include "seahorse-context.h"
#include "seahorse-key.h"
#include "seahorse-key-source.h"

#define SEAHORSE_TYPE_KEYSET               (seahorse_keyset_get_type ())
#define SEAHORSE_KEYSET(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_KEYSET, SeahorseKeyset))
#define SEAHORSE_KEYSET_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_KEYSET, SeahorseKeysetClass))
#define SEAHORSE_IS_KEYSET(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_KEYSET))
#define SEAHORSE_IS_KEYSET_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_KEYSET))
#define SEAHORSE_KEYSET_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_KEYSET, SeahorseKeysetClass))

typedef struct _SeahorseKeyset SeahorseKeyset;
typedef struct _SeahorseKeysetClass SeahorseKeysetClass;
typedef struct _SeahorseKeysetPrivate SeahorseKeysetPrivate;
    
struct _SeahorseKeyset {
    GtkObject parent;
    
    /*<private>*/
    SeahorseKeysetPrivate   *pv;
};

struct _SeahorseKeysetClass {
    GtkObjectClass parent_class;
    
    /* signals --------------------------------------------------------- */
    
    /* A key was added to this view */
    void (*added)   (SeahorseKeyset *skset, SeahorseKey *key);

    /* Removed a key from this view */
    void (*removed) (SeahorseKeyset *skset, SeahorseKey *key, gpointer closure);
    
    /* One of the key's attributes has changed */
    void (*changed) (SeahorseKeyset *skset, SeahorseKey *skey, 
                     SeahorseKeyChange change, gpointer closure);
    
    /* The set of keys changed */
    void (*set_changed) (SeahorseKeyset *skset);
};

GType               seahorse_keyset_get_type           (void);

SeahorseKeyset*     seahorse_keyset_new                (GQuark             ktype,
                                                        SeahorseKeyEType   etype,
                                                        SeahorseKeyLoc     location,
                                                        guint              flags,
                                                        guint              nflags);

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
